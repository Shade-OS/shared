#requires -Version 5.1
<#
    lvdump.ps1

    Calisan bir cleanmgr'in SysListView32 icerigini SURECLER ARASI okur:
    sira, ad, boyut ve onay durumu.

    UIA bu PropertySheet'te liste dondurmedi (olculdu), bu yuzden klasik yol:
    hedef surecte VirtualAllocEx ile tampon ayirip LVM_GETITEMTEXTW gonderiyoruz.
    cleanmgr asInvoker calistigi icin ayni butunluk duzeyinde; yonetici hakki
    gerekmez.

    SALT OKUR. Hicbir sey degistirmez, hicbir dugmeye basmaz.
#>
param(
    [Parameter(Mandatory=$true)][string]$Exe,
    [string]$Arguments = '/d C',
    [int]$Seconds = 90
)

Add-Type -TypeDefinition @'
using System; using System.Text; using System.Collections.Generic;
using System.Runtime.InteropServices;

public class LV {
  [DllImport("user32")] public static extern bool EnumWindows(P cb, IntPtr lp);
  [DllImport("user32")] public static extern bool EnumChildWindows(IntPtr h, P cb, IntPtr lp);
  [DllImport("user32")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32", CharSet=CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32", CharSet=CharSet.Unicode)] public static extern IntPtr SendMessageW(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("kernel32")] public static extern IntPtr OpenProcess(uint a, bool inh, uint pid);
  [DllImport("kernel32")] public static extern IntPtr VirtualAllocEx(IntPtr p, IntPtr addr, IntPtr sz, uint type, uint prot);
  [DllImport("kernel32")] public static extern bool VirtualFreeEx(IntPtr p, IntPtr addr, IntPtr sz, uint type);
  [DllImport("kernel32")] public static extern bool WriteProcessMemory(IntPtr p, IntPtr addr, byte[] buf, IntPtr sz, out IntPtr wrote);
  [DllImport("kernel32")] public static extern bool ReadProcessMemory(IntPtr p, IntPtr addr, byte[] buf, IntPtr sz, out IntPtr read);
  [DllImport("kernel32")] public static extern bool CloseHandle(IntPtr h);

  public delegate bool P(IntPtr h, IntPtr lp);

  const uint LVM_FIRST = 0x1000;
  const uint LVM_GETITEMCOUNT = LVM_FIRST + 4;
  const uint LVM_GETITEMTEXTW = LVM_FIRST + 115;
  const uint LVM_GETITEMSTATE = LVM_FIRST + 44;
  const uint LVIS_STATEIMAGEMASK = 0xF000;

  static uint s_pid;
  public static List<IntPtr> Hits = new List<IntPtr>();

  public static IntPtr FindList(uint pid) {
    s_pid = pid; Hits = new List<IntPtr>();
    EnumWindows(delegate(IntPtr h, IntPtr lp) {
      uint p; GetWindowThreadProcessId(h, out p);
      if (p == s_pid && IsWindowVisible(h)) {
        EnumChildWindows(h, delegate(IntPtr c, IntPtr lp2) {
          StringBuilder sb = new StringBuilder(64); GetClassNameW(c, sb, 64);
          if (sb.ToString() == "SysListView32") Hits.Add(c);
          return true; }, IntPtr.Zero);
      }
      return true; }, IntPtr.Zero);
    return Hits.Count > 0 ? Hits[0] : IntPtr.Zero;
  }

  // x64 LVITEMW: mask,iItem,iSubItem,state,stateMask,pszText,cchTextMax,iImage,lParam,...
  public static List<string[]> Dump(IntPtr hList, uint pid) {
    List<string[]> rows = new List<string[]>();
    int count = (int)SendMessageW(hList, LVM_GETITEMCOUNT, IntPtr.Zero, IntPtr.Zero);

    IntPtr hProc = OpenProcess(0x0008 | 0x0010 | 0x0020 | 0x0400, false, pid); // VM_OPERATION|VM_READ|VM_WRITE|QUERY_INFO
    if (hProc == IntPtr.Zero) return rows;

    int itemSize = 88;          // x64 LVITEMW (fazlasiyla yeterli)
    int textBytes = 1024;
    IntPtr remote = VirtualAllocEx(hProc, IntPtr.Zero, (IntPtr)(itemSize + textBytes), 0x1000 /*COMMIT*/, 0x04 /*RW*/);
    if (remote == IntPtr.Zero) { CloseHandle(hProc); return rows; }

    IntPtr remoteText = (IntPtr)(remote.ToInt64() + itemSize);

    for (int i = 0; i < count; i++) {
      string[] cols = new string[3];
      for (int sub = 0; sub < 2; sub++) {
        byte[] item = new byte[itemSize];
        // mask = LVIF_TEXT(1)
        BitConverter.GetBytes((uint)1).CopyTo(item, 0);
        BitConverter.GetBytes(i).CopyTo(item, 4);
        BitConverter.GetBytes(sub).CopyTo(item, 8);
        // state@12, stateMask@16, pszText@24 (8 hizali), cchTextMax@32
        BitConverter.GetBytes(remoteText.ToInt64()).CopyTo(item, 24);
        BitConverter.GetBytes((int)(textBytes / 2)).CopyTo(item, 32);

        IntPtr wrote;
        WriteProcessMemory(hProc, remote, item, (IntPtr)itemSize, out wrote);
        SendMessageW(hList, LVM_GETITEMTEXTW, (IntPtr)i, remote);

        byte[] txt = new byte[textBytes];
        IntPtr read;
        ReadProcessMemory(hProc, remoteText, txt, (IntPtr)textBytes, out read);
        string s = Encoding.Unicode.GetString(txt);
        int z = s.IndexOf('\0'); if (z >= 0) s = s.Substring(0, z);
        cols[sub] = s;
      }
      int st = (int)SendMessageW(hList, LVM_GETITEMSTATE, (IntPtr)i, (IntPtr)LVIS_STATEIMAGEMASK);
      cols[2] = ((st >> 12) == 2) ? "x" : " ";
      rows.Add(cols);
    }

    VirtualFreeEx(hProc, remote, IntPtr.Zero, 0x8000 /*RELEASE*/);
    CloseHandle(hProc);
    return rows;
  }
}
'@

$proc = Start-Process $Exe -ArgumentList $Arguments -PassThru
Write-Output ("PID {0}  ({1} {2})" -f $proc.Id, $Exe, $Arguments)

$hList = [IntPtr]::Zero
$sw = [Diagnostics.Stopwatch]::StartNew()
while ($sw.Elapsed.TotalSeconds -lt $Seconds -and $hList -eq [IntPtr]::Zero) {
    Start-Sleep -Milliseconds 300
    if ($proc.HasExited) { break }
    $hList = [LV]::FindList([uint32]$proc.Id)
}

if ($hList -eq [IntPtr]::Zero) { Write-Output "liste bulunamadi"; try{$proc.Kill()}catch{}; exit 1 }

Start-Sleep -Milliseconds 900
$rows = [LV]::Dump($hList, [uint32]$proc.Id)

Write-Output ("oge sayisi: {0}" -f $rows.Count)
Write-Output ""
$i = 1
foreach ($r in $rows) { Write-Output ("{0,3}  [{1}] {2,-46} {3}" -f $i, $r[2], $r[0], $r[1]); $i++ }

try { $proc.Kill() } catch {}
