#requires -Version 5.1
<#
    catch.ps1

    Kisa omurlu bir pencereyi yakalar. shots.ps1 her kareden once 600 ms
    bekler (pencerenin oturmasi icin); tarama diyalogu bu makinede ~0,4 s
    yasadigi icin o gecikmeyle kaciriliyor.

    Bu betik hedef basligi/boyutu bekleyip GECIKMESIZ yakalar.

        catch.ps1 -Exe ... -Arguments "/d C /dark" -Out tarama.png -MaxW 600 -MaxH 300
#>
param(
    [Parameter(Mandatory=$true)][string]$Exe,
    [string]$Arguments = '',
    [Parameter(Mandatory=$true)][string]$Out,
    [int]$MinW = 200,
    [int]$MaxW = 640,
    [int]$MinH = 100,
    [int]$MaxH = 320,
    [int]$Seconds = 40,
    [int]$SettleMs = 220
)

Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @'
using System; using System.Drawing; using System.Text;
using System.Collections.Generic; using System.Runtime.InteropServices;
public class C {
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L,T,R,B; }
  [DllImport("user32")] public static extern bool SetProcessDPIAware();
  [DllImport("user32")] public static extern bool EnumWindows(P cb, IntPtr lp);
  [DllImport("user32")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32", CharSet=CharSet.Unicode)] public static extern int GetWindowTextW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32")] public static extern IntPtr GetDC(IntPtr h);
  [DllImport("user32")] public static extern int ReleaseDC(IntPtr h, IntPtr dc);
  [DllImport("gdi32")]  public static extern bool BitBlt(IntPtr d,int x,int y,int w,int h,IntPtr s,int sx,int sy,int rop);
  [DllImport("dwmapi")] public static extern int DwmGetWindowAttribute(IntPtr h,int a,out RECT r,int cb);
  public delegate bool P(IntPtr h, IntPtr lp);
  static uint pid; public static List<IntPtr> R = new List<IntPtr>();
  public static List<IntPtr> Vis(uint p_) {
    pid=p_; R=new List<IntPtr>();
    EnumWindows(delegate(IntPtr h, IntPtr lp){
      uint p; GetWindowThreadProcessId(h,out p);
      if(p==pid && IsWindowVisible(h)) R.Add(h); return true; }, IntPtr.Zero);
    return R;
  }
  public static string T(IntPtr h){ StringBuilder s=new StringBuilder(256); GetWindowTextW(h,s,256); return s.ToString(); }
  public static RECT F(IntPtr h){ RECT r; if(DwmGetWindowAttribute(h,9,out r,Marshal.SizeOf(typeof(RECT)))==0) return r; GetWindowRect(h,out r); return r; }
  public static Bitmap Grab(RECT r){
    int w=r.R-r.L, hh=r.B-r.T; if(w<=0||hh<=0) return null;
    Bitmap b=new Bitmap(w,hh);
    using(Graphics g=Graphics.FromImage(b)){
      IntPtr d=g.GetHdc(); IntPtr s=GetDC(IntPtr.Zero);
      BitBlt(d,0,0,w,hh,s,r.L,r.T,0x00CC0020);
      ReleaseDC(IntPtr.Zero,s); g.ReleaseHdc(d);
    }
    return b;
  }
}
'@

[C]::SetProcessDPIAware() | Out-Null

$proc = if ($Arguments -ne '') { Start-Process $Exe -ArgumentList $Arguments -PassThru }
        else { Start-Process $Exe -PassThru }

Write-Output ("PID {0} bekleniyor..." -f $proc.Id)

$sw = [Diagnostics.Stopwatch]::StartNew()
$got = $false

while ($sw.Elapsed.TotalSeconds -lt $Seconds -and -not $proc.HasExited -and -not $got) {
    foreach ($h in [C]::Vis([uint32]$proc.Id)) {
        $r = [C]::F($h)
        $w = $r.R - $r.L; $ht = $r.B - $r.T
        if ($w -ge $MinW -and $w -le $MaxW -and $ht -ge $MinH -and $ht -le $MaxH) {
            # DWM'in acilis solmasi bitene kadar bekle; yoksa kare yari saydam
            # cikiyor ve renk olcumu anlamsizlasiyor.
            Start-Sleep -Milliseconds $SettleMs
            $r = [C]::F($h)
            $bmp = [C]::Grab($r)
            if ($bmp -ne $null) {
                $bmp.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
                $bmp.Dispose()
                Write-Output ("YAKALANDI {0:N2}s  {1}x{2}  [{3}]  -> {4}" -f $sw.Elapsed.TotalSeconds, $w, $ht, [C]::T($h), $Out)
                $got = $true
            }
            break
        }
    }
    Start-Sleep -Milliseconds 15
}

if (-not $got) { Write-Output "yakalanamadi" }
Start-Sleep -Milliseconds 400
try { if (-not $proc.HasExited) { $proc.Kill() } } catch {}
