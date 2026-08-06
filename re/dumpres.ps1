param([string]$Path = 'C:\Windows\System32\winver.exe')

Add-Type @'
using System;
using System.Text;
using System.Collections.Generic;
using System.Runtime.InteropServices;

public class ResDump
{
    public const uint LOAD_LIBRARY_AS_DATAFILE = 0x00000002;
    public const uint LOAD_LIBRARY_AS_IMAGE_RESOURCE = 0x00000020;

    [DllImport("kernel32", CharSet = CharSet.Unicode, SetLastError = true)]
    public static extern IntPtr LoadLibraryExW(string f, IntPtr h, uint flags);

    [DllImport("user32", CharSet = CharSet.Unicode, SetLastError = true)]
    public static extern int LoadStringW(IntPtr h, uint id, StringBuilder buf, int n);

    public delegate bool EnumResTypeProc(IntPtr hModule, IntPtr lpszType, IntPtr lParam);
    public delegate bool EnumResNameProc(IntPtr hModule, IntPtr lpszType, IntPtr lpszName, IntPtr lParam);
    public delegate bool EnumResLangProc(IntPtr hModule, IntPtr lpszType, IntPtr lpszName, ushort wLang, IntPtr lParam);

    [DllImport("kernel32", CharSet = CharSet.Unicode, SetLastError = true)]
    public static extern bool EnumResourceTypesW(IntPtr hModule, EnumResTypeProc cb, IntPtr lParam);
    [DllImport("kernel32", CharSet = CharSet.Unicode, SetLastError = true)]
    public static extern bool EnumResourceNamesW(IntPtr hModule, IntPtr type, EnumResNameProc cb, IntPtr lParam);
    [DllImport("kernel32", CharSet = CharSet.Unicode, SetLastError = true)]
    public static extern bool EnumResourceLanguagesW(IntPtr hModule, IntPtr type, IntPtr name, EnumResLangProc cb, IntPtr lParam);

    [DllImport("kernel32", CharSet = CharSet.Unicode, SetLastError = true)]
    public static extern IntPtr FindResourceExW(IntPtr hModule, IntPtr type, IntPtr name, ushort lang);
    [DllImport("kernel32", SetLastError = true)]
    public static extern IntPtr LoadResource(IntPtr hModule, IntPtr hResInfo);
    [DllImport("kernel32", SetLastError = true)]
    public static extern IntPtr LockResource(IntPtr hResData);
    [DllImport("kernel32", SetLastError = true)]
    public static extern uint SizeofResource(IntPtr hModule, IntPtr hResInfo);

    public static List<string> Types = new List<string>();
    public static List<string> Entries = new List<string>();

    static string Nm(IntPtr p)
    {
        if ((ulong)p.ToInt64() <= 0xFFFF) return "#" + p.ToInt64().ToString();
        return Marshal.PtrToStringUni(p);
    }

    public static void Dump(IntPtr h)
    {
        EnumResourceTypesW(h, delegate(IntPtr hm, IntPtr t, IntPtr lp)
        {
            string tn = Nm(t);
            Types.Add(tn);
            EnumResourceNamesW(hm, t, delegate(IntPtr hm2, IntPtr t2, IntPtr n2, IntPtr lp2)
            {
                EnumResourceLanguagesW(hm2, t2, n2, delegate(IntPtr hm3, IntPtr t3, IntPtr n3, ushort lang, IntPtr lp3)
                {
                    IntPtr hr = FindResourceExW(hm3, t3, n3, lang);
                    uint sz = (hr == IntPtr.Zero) ? 0 : SizeofResource(hm3, hr);
                    Entries.Add(tn + "\t" + Nm(n2) + "\tlang=0x" + lang.ToString("X4") + "\tsize=" + sz);
                    return true;
                }, IntPtr.Zero);
                return true;
            }, IntPtr.Zero);
            return true;
        }, IntPtr.Zero);
    }
}
'@

$h = [ResDump]::LoadLibraryExW($Path, [IntPtr]::Zero, ([ResDump]::LOAD_LIBRARY_AS_DATAFILE -bor [ResDump]::LOAD_LIBRARY_AS_IMAGE_RESOURCE))
if ($h -eq [IntPtr]::Zero) { Write-Output ("LoadLibraryEx FAILED " + [Runtime.InteropServices.Marshal]::GetLastWin32Error()); exit 1 }

Write-Output "===== RESOURCES: $Path ====="
[ResDump]::Dump($h)
[ResDump]::Entries | ForEach-Object { Write-Output $_ }

Write-Output ""
Write-Output "===== STRING TABLE ====="
$h2 = [ResDump]::LoadLibraryExW($Path, [IntPtr]::Zero, [ResDump]::LOAD_LIBRARY_AS_DATAFILE)
for ($i = 0; $i -lt 8192; $i++) {
    $sb = New-Object System.Text.StringBuilder 4096
    $r = [ResDump]::LoadStringW($h2, [uint32]$i, $sb, 4096)
    if ($r -gt 0) { Write-Output ("{0} ({1:X4}) : {2}" -f $i, $i, $sb.ToString()) }
}
