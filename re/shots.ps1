#requires -Version 5.1
<#
    shots.ps1

    Bir cleanmgr'i (orijinal ya da klon) calistirir ve ACILAN HER PENCEREYI
    sirayla yakalar. Asamalar onceden bilinmek zorunda degil: yeni bir gorunur
    ust duzey pencere belirdiginde ya da mevcut pencerenin basligi/boyutu
    degistiginde bir kare alinir.

        shots.ps1 -Exe ...\cleanmgr.exe -Args "/d C /dark" -Prefix klon-dark
        shots.ps1 -Exe C:\Windows\System32\cleanmgr.exe -Args "/d C" -Prefix orijinal

    -Press ile, yakalamadan sonra bir dugmeye basilir (surucu secimini gecmek
    icin): -Press 1 (IDOK) gibi.

    HICBIR SEY SILINMEZ: betik yalnizca yakalar ve sonunda WM_CLOSE gonderir.
    Onay adimina asla "evet" denmez.
#>
param(
    [Parameter(Mandatory=$true)][string]$Exe,
    [string]$Arguments = '',
    [Parameter(Mandatory=$true)][string]$Prefix,
    [string]$OutDir = 'C:\Users\shades\Desktop\ShadeOS\cleanmgr\re\tema',
    [int]$Seconds = 45,
    [int]$Pad = 0,
    [int]$Press = 0,
    [int]$PressAfter = 1
)

Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @'
using System;
using System.Drawing;
using System.Text;
using System.Collections.Generic;
using System.Runtime.InteropServices;

public class Shot
{
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }

    [DllImport("user32")] public static extern bool SetProcessDPIAware();
    [DllImport("user32")] public static extern bool EnumWindows(EnumProc cb, IntPtr lp);
    [DllImport("user32")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
    [DllImport("user32")] public static extern bool IsWindowVisible(IntPtr h);
    [DllImport("user32")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
    [DllImport("user32", CharSet=CharSet.Unicode)] public static extern int GetWindowTextW(IntPtr h, StringBuilder s, int n);
    [DllImport("user32", CharSet=CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, StringBuilder s, int n);
    [DllImport("user32")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32")] public static extern bool BringWindowToTop(IntPtr h);
    [DllImport("user32")] public static extern IntPtr GetDC(IntPtr h);
    [DllImport("user32")] public static extern int ReleaseDC(IntPtr h, IntPtr dc);
    [DllImport("user32")] public static extern bool PostMessageW(IntPtr h, uint m, IntPtr w, IntPtr l);
    [DllImport("user32")] public static extern IntPtr GetDlgItem(IntPtr h, int id);
    [DllImport("gdi32")]  public static extern bool BitBlt(IntPtr d, int x, int y, int w, int h, IntPtr s, int sx, int sy, int rop);
    [DllImport("dwmapi")] public static extern int DwmGetWindowAttribute(IntPtr h, int a, out RECT r, int cb);

    public delegate bool EnumProc(IntPtr h, IntPtr lp);

    static uint s_pid;
    public static List<IntPtr> Found = new List<IntPtr>();

    public static List<IntPtr> TopLevel(uint pid)
    {
        s_pid = pid;
        Found = new List<IntPtr>();
        EnumWindows(delegate(IntPtr h, IntPtr lp)
        {
            uint p; GetWindowThreadProcessId(h, out p);
            if (p == s_pid && IsWindowVisible(h)) Found.Add(h);
            return true;
        }, IntPtr.Zero);
        return Found;
    }

    public static string Title(IntPtr h)
    { StringBuilder sb = new StringBuilder(512); GetWindowTextW(h, sb, 512); return sb.ToString(); }

    public static string Cls(IntPtr h)
    { StringBuilder sb = new StringBuilder(256); GetClassNameW(h, sb, 256); return sb.ToString(); }

    // Golge/kenarlik dahil olmayan gercek pencere dikdortgeni.
    public static RECT Frame(IntPtr h)
    {
        RECT r;
        if (DwmGetWindowAttribute(h, 9 /*DWMWA_EXTENDED_FRAME_BOUNDS*/, out r, Marshal.SizeOf(typeof(RECT))) == 0)
            return r;
        GetWindowRect(h, out r);
        return r;
    }

    // Ekrandan alir: DWM kompozisyonu, yuvarlak koseler ve baslik cubugu dahil.
    public static Bitmap Grab(IntPtr h, int pad)
    {
        RECT r = Frame(h);
        int x = r.L - pad, y = r.T - pad;
        int w = (r.R - r.L) + pad * 2, hh = (r.B - r.T) + pad * 2;
        if (w <= 0 || hh <= 0) return null;

        Bitmap bmp = new Bitmap(w, hh);
        using (Graphics g = Graphics.FromImage(bmp))
        {
            IntPtr dst = g.GetHdc();
            IntPtr src = GetDC(IntPtr.Zero);
            BitBlt(dst, 0, 0, w, hh, src, x, y, 0x00CC0020);
            ReleaseDC(IntPtr.Zero, src);
            g.ReleaseHdc(dst);
        }
        return bmp;
    }
}
'@

[Shot]::SetProcessDPIAware() | Out-Null
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

if ($Arguments -ne '') {
    $proc = Start-Process -FilePath $Exe -ArgumentList $Arguments -PassThru
} else {
    $proc = Start-Process -FilePath $Exe -PassThru
}

Write-Output ("EXE   : {0} {1}" -f $Exe, $Arguments)
Write-Output ("PID   : {0}" -f $proc.Id)

$seen    = @{}
$shotNo  = 0
$deadline = (Get-Date).AddSeconds($Seconds)
$pressed = 0

while ((Get-Date) -lt $deadline) {
    Start-Sleep -Milliseconds 250

    if ($proc.HasExited) { Write-Output "  (surec kapandi)"; break }

    $wins = [Shot]::TopLevel([uint32]$proc.Id)
    foreach ($h in $wins) {
        $r = [Shot]::Frame($h)
        $w = $r.R - $r.L; $ht = $r.B - $r.T
        if ($w -lt 80 -or $ht -lt 60) { continue }

        $title = [Shot]::Title($h)
        $cls   = [Shot]::Cls($h)
        $key   = "{0}|{1}|{2}x{3}" -f $cls, $title, $w, $ht

        if ($seen.ContainsKey($key)) { continue }
        $seen[$key] = $true

        [Shot]::BringWindowToTop($h) | Out-Null
        [Shot]::SetForegroundWindow($h) | Out-Null
        Start-Sleep -Milliseconds 600

        $shotNo++
        $file = Join-Path $OutDir ("{0}-{1}.png" -f $Prefix, $shotNo)
        $bmp  = [Shot]::Grab($h, $Pad)
        if ($bmp -ne $null) {
            $bmp.Save($file, [System.Drawing.Imaging.ImageFormat]::Png)
            $bmp.Dispose()
            Write-Output ("  [{0}] {1,-18} {2,-42} {3}x{4}  -> {5}" -f $shotNo, $cls, $title, $w, $ht, (Split-Path $file -Leaf))
        }

        if ($Press -ne 0 -and $shotNo -eq $PressAfter) {
            Start-Sleep -Milliseconds 300
            [Shot]::PostMessageW($h, 0x0111 <# WM_COMMAND #>, [IntPtr]$Press, [IntPtr]::Zero) | Out-Null
            $pressed++
            Write-Output ("      -> dugme {0} gonderildi" -f $Press)
        }
    }
}

# Temiz kapanis. Onay adimina asla evet demiyoruz.
foreach ($h in [Shot]::TopLevel([uint32]$proc.Id)) {
    [Shot]::PostMessageW($h, 0x0010 <# WM_CLOSE #>, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null
}
Start-Sleep -Milliseconds 900
try { if (-not $proc.HasExited) { $proc.Kill() } } catch {}

Write-Output ("Toplam {0} kare -> {1}" -f $shotNo, $OutDir)
