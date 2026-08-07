#requires -Version 5.1
<#
    regression.ps1

    ShadeOS gerileme ağı — tek komutla "hiçbir şeyi bozmadım" kanıtı.

    NEDEN GEREKLİ: dört araç tek bir `shared` katmanını paylaşıyor ve her biri
    onu ayrı bir depoda submodule olarak sabitliyor. Katmanda yapılan bir
    düzeltme dört araca birden yayılıyor — düzeltme de, kaza da. Tek bir
    oturumda `shared` yedi kez değişti; hangi değişikliğin hangi aracı
    bozduğunu gözle takip etmek mümkün değil.

    NE ÖLÇER — iki şey, ikisi de ikili (geçti/kaldı):

      1. DERLEME   Her araç 0 uyarıyla derleniyor mu?
      2. AYNILIK   Aracın /classic çıktısı ORİJİNALİYLE piksel piksel aynı mı?

    İkincisi projenin ana vaadi: karanlık mod bir EKLENTİ, yeniden yazım
    değil. /classic kipinde tek pikselin bile değişmemesi gerekir.

    ÖLÇÜT: maskelenmemiş piksellerin TAMAMI aynı. "%99,9" kabul edilmez —
    615x710'luk bir pencerede %0,1 ≈ 630 piksel eder ve bu tam olarak
    "1 piksel kalınlığında uzun bir kenarlık rengi farkı", yani aradığımız
    kusurun ta kendisi.

    GÜRÜLTÜ TABANI: her karşılaştırmadan önce ORİJİNAL kendisiyle
    karşılaştırılır. O fark sıfır değilse pencere zamana bağlı bir şey
    gösteriyordur (imleç, saat) ve sonuç "belirsiz" sayılır — sahte bir
    "geçti" üretmemek için.

    GÜVENLİK: hiçbir pencerede onay düğmesine basılmaz; hepsi WM_CLOSE ile
    kapatılır. cleanmgr'da yalnızca sürücü seçimi kutusu açılır — tarama ve
    temizleme HİÇ çalıştırılmaz.

        regression.ps1                 -> derleme + aynılık
        regression.ps1 -SkipBuild      -> yalnızca aynılık
        regression.ps1 -Only winver    -> tek araç
#>
param(
    [string]$Root      = 'C:\Users\shades\Desktop\ShadeOS',
    [string]$Config    = 'Release',
    [string]$Platform  = 'x64',
    [string[]]$Only    = @(),
    [switch]$SkipBuild,
    [string]$OutDir    = ''
)

if ($OutDir -eq '') { $OutDir = Join-Path $env:TEMP 'shadeos-regression' }
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

$sistem = Join-Path $env:SystemRoot 'System32'

# ── araç tanımları ────────────────────────────────────────────────────────
#
#   klasor   : $Root altındaki depo dizini
#   cikti    : bin\<platform>\<config> altındaki exe adı
#   orijinal : karşılaştırılacak sistem programı (yoksa yalnızca derlenir)
#   arg      : orijinale ve klona verilen ortak argüman
#   klonArg  : klona ek olarak verilen (tema zorlaması)
#   minG/minY: pencereyi tanımak için en küçük genişlik/yükseklik
#
$araclar = @(
    @{ ad='winver'; klasor='winver'; cikti='winver.exe'
       orijinal=(Join-Path $sistem 'winver.exe'); arg=''; minG=380; minY=250 }

    @{ ad='SystemProperties'; klasor='SystemProperties'; cikti='SystemPropertiesAdvanced.exe'
       orijinal=(Join-Path $sistem 'SystemPropertiesAdvanced.exe'); arg=''; minG=400; minY=500 }

    # control: AYNILIK ÖLÇÜMÜ ATLANIR — sebebi ölçüldü.
    #
    # Karşılaştırma için orijinal tarafın da aynı pencereyi açması gerekir.
    # İki kabuk yolu da denendi ve yükseltilmiş/etkileşimsiz bağlamda HİÇ
    # pencere açmadı (10 sn beklendi, sistem genelinde yeni pencere yok):
    #
    #     control.exe mmsys.cpl
    #     rundll32.exe shell32.dll,Control_RunDLL mmsys.cpl
    #
    # İkisi de kabuk bağlamına dayanıyor. Klonumuz ise .cpl'yi kendi sürecine
    # yükleyip CPlApplet'i doğrudan sürdüğü için pencereyi açabiliyor —
    # yakalanan tek taraf o oluyor.
    #
    # Sürekli KALDI raporlamak yanlış alarmdır ve ağa olan güveni bitirir;
    # bu yüzden atlanıyor. Kayıp da küçük: control pencereyi ZATEN sistemin
    # kendi .cpl'sine çizdiriyor, yani aynılık iddiası burada winver'daki
    # gibi neredeyse totoloji. Asıl sınav cleanmgr'da.
    @{ ad='control'; klasor='control'; cikti='control.exe'
       orijinal=(Join-Path $sistem 'rundll32.exe')
       orjArg='shell32.dll,Control_RunDLL mmsys.cpl'
       arg='mmsys.cpl'; minG=250; minY=200
       atla='orijinal kabuk yolu yukseltilmis baglamda pencere acmiyor' }

    # cleanmgr: sürücü VERİLEREK açılır ve ANA pencere beklenir.
    #
    # Ölçüldü: argümansız açınca yakalanan pencere 506x222 oluyor ve bu
    # sürücü seçimi değil, "Hesaplanıyor..." ilerleme kutusu. İki çalıştırma
    # arasında 1882 piksel fark çıkıyordu; farkın kapsamı x=20..239,
    # y=165..182 -- yani ilerleme çubuğu ve "Taranıyor:" satırı. Zamana bağlı
    # bir ekranı karşılaştırmanın anlamı yok.
    #
    # minG/minY o kutuyu elemek için yükseltildi; ana pencere ~559x713.
    #
    # GÜVENLİK: tarama salt okumadır. Hiçbir düğmeye basılmaz, pencere
    # WM_CLOSE ile kapatılır -- temizleme ÇALIŞTIRILMAZ.
    @{ ad='cleanmgr'; klasor='cleanmgr'; cikti='cleanmgr.exe'
       orijinal=(Join-Path $sistem 'cleanmgr.exe'); arg='/d C'
       minG=520; minY=600 }
)

if ($Only.Count -gt 0) {
    $araclar = @($araclar | Where-Object { $Only -contains $_.ad })
}

Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @'
using System; using System.Drawing; using System.Text;
using System.Collections.Generic; using System.Runtime.InteropServices;
public class RG {
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L,T,R,B; }
  [DllImport("user32")] public static extern bool SetProcessDPIAware();
  [DllImport("user32")] public static extern bool EnumWindows(P cb, IntPtr lp);
  [DllImport("user32")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32", CharSet=CharSet.Unicode)] public static extern int GetWindowTextW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32")] public static extern bool SetWindowPos(IntPtr h,IntPtr a,int x,int y,int cx,int cy,uint f);
  [DllImport("user32")] public static extern bool PostMessageW(IntPtr h,uint m,IntPtr w,IntPtr l);
  [DllImport("user32")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32")] public static extern IntPtr GetDC(IntPtr h);
  [DllImport("user32")] public static extern int ReleaseDC(IntPtr h,IntPtr dc);
  [DllImport("gdi32")]  public static extern bool BitBlt(IntPtr d,int x,int y,int w,int h,IntPtr s,int sx,int sy,int rop);
  [DllImport("dwmapi")] public static extern int DwmGetWindowAttribute(IntPtr h,int a,out RECT r,int cb);
  public delegate bool P(IntPtr h, IntPtr lp);
  static uint pid; public static List<IntPtr> L=new List<IntPtr>();
  public static List<IntPtr> Top(uint p_){ pid=p_; L=new List<IntPtr>();
    EnumWindows(delegate(IntPtr h,IntPtr lp){ uint p; GetWindowThreadProcessId(h,out p);
      if(p==pid&&IsWindowVisible(h)) L.Add(h); return true; },IntPtr.Zero); return L; }
  public static List<IntPtr> Tumu(){ List<IntPtr> A=new List<IntPtr>();
    EnumWindows(delegate(IntPtr h,IntPtr lp){ if(IsWindowVisible(h)) A.Add(h); return true; },IntPtr.Zero);
    return A; }
  public static uint Pid(IntPtr h){ uint p; GetWindowThreadProcessId(h,out p); return p; }
  public static string T(IntPtr h){ StringBuilder s=new StringBuilder(200); GetWindowTextW(h,s,200); return s.ToString(); }
  public static RECT Fr(IntPtr h){ RECT r;
    if(DwmGetWindowAttribute(h,9,out r,Marshal.SizeOf(typeof(RECT)))==0) return r;
    GetWindowRect(h,out r); return r; }
  public static Bitmap Grab(RECT r){ int w=r.R-r.L, hh=r.B-r.T; if(w<=0||hh<=0) return null;
    Bitmap b=new Bitmap(w,hh);
    using(Graphics g=Graphics.FromImage(b)){ IntPtr d=g.GetHdc(); IntPtr s=GetDC(IntPtr.Zero);
      BitBlt(d,0,0,w,hh,s,r.L,r.T,0x00CC0020); ReleaseDC(IntPtr.Zero,s); g.ReleaseHdc(d); }
    return b; }
}
'@

[RG]::SetProcessDPIAware() | Out-Null

<#
    Pencereyi AYNI ekran konumuna taşıyıp yakalar.

    Konum sabitlenmezse Windows 11'in saydam yuvarlak köşeleri arkadaki
    duvar kağıdını harmanlar ve iki kare hiçbir zaman eşleşmez.
#>
function Yakala([string]$exe, [string]$arg, [int]$minG, [int]$minY, [string]$png)
{
    <#
        Pencere önce KENDİ PID'imizde aranır. Bulunamazsa sistem genelinde,
        "başlatmadan sonra beliren yeni pencere" olarak aranır.

        İkinci yol şart: sistemin control.exe'si bir launcher'dır, applet'i
        BAŞKA bir proseste açar ve kendi PID'inde hiç pencere olmaz. İlk
        sürümde bu yüzden "PENCERE YOK" çıkıyordu.
    #>
    $once = @{}
    foreach ($h in [RG]::Tumu()) { $once[$h] = $true }

    $proc = if ($arg -ne '') { Start-Process $exe -ArgumentList $arg -PassThru }
            else             { Start-Process $exe -PassThru }

    $w = [IntPtr]::Zero
    $yabanciPid = 0
    for ($i=0; $i -lt 200; $i++) {
        Start-Sleep -Milliseconds 200

        foreach ($h in [RG]::Top([uint32]$proc.Id)) {
            $r = [RG]::Fr($h)
            if (($r.R-$r.L) -ge $minG -and ($r.B-$r.T) -ge $minY) { $w = $h; break }
        }
        if ($w -ne [IntPtr]::Zero) { break }

        if ($proc.HasExited -or $i -gt 12) {
            foreach ($h in [RG]::Tumu()) {
                if ($once.ContainsKey($h)) { continue }
                $r = [RG]::Fr($h)
                if (($r.R-$r.L) -ge $minG -and ($r.B-$r.T) -ge $minY) {
                    $w = $h; $yabanciPid = [RG]::Pid($h); break
                }
            }
            if ($w -ne [IntPtr]::Zero) { break }
        }
    }
    if ($w -eq [IntPtr]::Zero) { try{$proc.Kill()}catch{}; return $null }

    # SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE
    [RG]::SetWindowPos($w,[IntPtr]::Zero,200,120,0,0,0x15) | Out-Null
    Start-Sleep -Milliseconds 2200
    [RG]::SetForegroundWindow($w) | Out-Null
    Start-Sleep -Milliseconds 1400

    $r = [RG]::Fr($w)
    $bmp = [RG]::Grab($r)
    if ($bmp -ne $null) { $bmp.Save($png, [System.Drawing.Imaging.ImageFormat]::Png); $bmp.Dispose() }

    [RG]::PostMessageW($w, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null
    Start-Sleep -Milliseconds 900
    try { $proc.Kill() } catch {}
    # Pencere baska bir proseste acildiysa onu da kapat; kalirsa sonraki
    # olcumde "yeni pencere" sanilir ve karsilastirma bozulur.
    if ($yabanciPid -ne 0 -and $yabanciPid -ne $proc.Id) {
        try { Stop-Process -Id $yabanciPid -Force -ErrorAction Stop } catch {}
    }
    Start-Sleep -Milliseconds 700
    return $png
}

<#
    İki PNG'yi karşılaştırır. Dönüş:
        >= 0  farklı piksel sayısı
        -1    boyut farkı
        -2    dosya yok
        -3    KARŞILAŞTIRILAMADI

    -3 AYRI BİR DEĞER OLMAK ZORUNDA. İlk sürümde hata durumunda sayaç
    hiç artmıyor ve fonksiyon 0 döndürüyordu; rapor "AYNI" yazıyordu.
    Yani ölçüm çöktüğünde ağ SESSİZCE GEÇTİ diyordu -- tam olarak önlemesi
    gereken şeyi kendisi yapıyordu.

    (O sürümdeki hata da şuydu: PowerShell değişken adları büyük/küçük harf
    DUYARSIZDIR, dolayısıyla $A ile parametredeki $a aynı değişkendi ve
    bitmap'in üzerine dosya yolu yazılıyordu.)
#>
function Fark([string]$yolA, [string]$yolB)
{
    if (-not (Test-Path $yolA) -or -not (Test-Path $yolB)) { return -2 }

    $bmpA = $null; $bmpB = $null
    try {
        Add-Type -AssemblyName System.Drawing
        $bmpA = [System.Drawing.Bitmap]::FromFile($yolA)
        $bmpB = [System.Drawing.Bitmap]::FromFile($yolB)

        if ($bmpA.Width -ne $bmpB.Width -or $bmpA.Height -ne $bmpB.Height) { return -1 }

        $rc  = New-Object System.Drawing.Rectangle 0, 0, $bmpA.Width, $bmpA.Height
        $fmt = [System.Drawing.Imaging.PixelFormat]::Format32bppArgb
        $la  = $bmpA.LockBits($rc, [System.Drawing.Imaging.ImageLockMode]::ReadOnly, $fmt)
        $lb  = $bmpB.LockBits($rc, [System.Drawing.Imaging.ImageLockMode]::ReadOnly, $fmt)

        $n  = [int]($la.Stride * $bmpA.Height)
        $ba = New-Object 'byte[]' $n
        $bb = New-Object 'byte[]' $n
        [System.Runtime.InteropServices.Marshal]::Copy($la.Scan0, $ba, 0, $n)
        [System.Runtime.InteropServices.Marshal]::Copy($lb.Scan0, $bb, 0, $n)
        $bmpA.UnlockBits($la); $bmpB.UnlockBits($lb)

        $farkli = 0
        for ($i = 0; $i -lt $n; $i += 4) {
            if ($ba[$i]   -ne $bb[$i]   -or
                $ba[$i+1] -ne $bb[$i+1] -or
                $ba[$i+2] -ne $bb[$i+2]) { $farkli++ }
        }
        return $farkli
    }
    catch {
        Write-Output ("      [olcum hatasi] {0}" -f $_.Exception.Message)
        return -3
    }
    finally {
        if ($bmpA -ne $null) { $bmpA.Dispose() }
        if ($bmpB -ne $null) { $bmpB.Dispose() }
    }
}

# ── 1. DERLEME ────────────────────────────────────────────────────────────
$sonuc = @()

foreach ($t in $araclar) {
    $dizin = Join-Path $Root $t.klasor
    $satir = @{ ad=$t.ad; derleme='-'; uyari=0; aynilik='-'; not='' }

    if (-not (Test-Path $dizin)) {
        $satir.derleme = 'DIZIN YOK'; $sonuc += $satir; continue
    }

    if (-not $SkipBuild) {
        $cmd = Join-Path $dizin 'build.cmd'
        $out = cmd /c "`"$cmd`" $Config $Platform" 2>&1
        $hata  = @($out | Select-String ': error|: fatal|HATA')
        $uyari = @($out | Select-String ': warning')
        $satir.uyari = $uyari.Count
        $satir.derleme = if ($hata.Count -gt 0) { 'HATA' }
                         elseif ($uyari.Count -gt 0) { 'UYARI' }
                         else { 'tamam' }
        if ($hata.Count -gt 0) { $satir.not = ($hata[0].Line.Trim()) }
    }

    $sonuc += $satir
}

# ── 2. AYNILIK ────────────────────────────────────────────────────────────
foreach ($t in $araclar) {
    $satir = $sonuc | Where-Object { $_.ad -eq $t.ad } | Select-Object -First 1
    if ($satir.derleme -eq 'HATA' -or $satir.derleme -eq 'DIZIN YOK') { continue }

    $binDir = if ($Platform -eq 'x86') { 'Win32' } else { $Platform }
    $klon = Join-Path (Join-Path $Root $t.klasor) "bin\$binDir\$Config\$($t.cikti)"

    if (-not (Test-Path $klon))      { $satir.aynilik = 'KLON YOK'; continue }
    if (-not (Test-Path $t.orijinal)) { $satir.aynilik = 'ORJ YOK';  continue }

    if ($t.ContainsKey('atla')) {
        $satir.aynilik = 'ATLANDI'
        $satir.not = $t.atla
        continue
    }

    # Orijinal, klondan farkli bir arguman gerektirebilir (bkz. control).
    $orjArg = if ($t.ContainsKey('orjArg')) { $t.orjArg } else { $t.arg }

    $o1 = Yakala $t.orijinal $orjArg $t.minG $t.minY (Join-Path $OutDir "$($t.ad)-orj-1.png")
    $o2 = Yakala $t.orijinal $orjArg $t.minG $t.minY (Join-Path $OutDir "$($t.ad)-orj-2.png")
    $k1 = Yakala $klon (($t.arg + ' /classic').Trim()) $t.minG $t.minY (Join-Path $OutDir "$($t.ad)-klon.png")

    if ($null -eq $o1 -or $null -eq $o2 -or $null -eq $k1) {
        $eksik = @()
        if ($null -eq $o1 -or $null -eq $o2) { $eksik += 'orijinal' }
        if ($null -eq $k1)                   { $eksik += 'klon' }
        $satir.aynilik = 'PENCERE YOK'
        $satir.not = ("yakalanamayan: {0}  (orj='{1} {2}', klon='{3} {4} /classic')" -f `
                      ($eksik -join '+'), (Split-Path $t.orijinal -Leaf), $t.arg,
                      $t.cikti, $t.arg)
        continue
    }

    # Gürültü tabanı: orijinal kendisiyle. Sıfır değilse sonuç belirsizdir.
    $gurultu = Fark $o1 $o2
    if ($gurultu -eq -3) {
        $satir.aynilik = 'OLCULEMEDI'; $satir.not = 'gurultu tabani olculemedi'; continue
    }
    if ($gurultu -ne 0) {
        $satir.aynilik = 'BELIRSIZ'
        $satir.not = "orijinal kendisiyle $gurultu piksel farkli"
        continue
    }

    $f = Fark $o1 $k1
    switch ($f) {
        -3      { $satir.aynilik = 'OLCULEMEDI' }
        -2      { $satir.aynilik = 'KARE YOK' }
        -1      { $satir.aynilik = 'BOYUT FARKLI' }
         0      { $satir.aynilik = 'AYNI' }
        default { $satir.aynilik = "$f PIKSEL FARKLI" }
    }
}

# ── rapor ─────────────────────────────────────────────────────────────────
Write-Output ''
Write-Output '  arac              derleme  uyari  /classic aynilik'
Write-Output '  ----------------  -------  -----  ----------------'
foreach ($s in $sonuc) {
    Write-Output ("  {0,-16}  {1,-7}  {2,5}  {3}" -f $s.ad, $s.derleme, $s.uyari, $s.aynilik)
    if ($s.not -ne '') { Write-Output ("                              {0}" -f $s.not) }
}

$kotu = @($sonuc | Where-Object {
    $_.derleme -eq 'HATA' -or $_.uyari -gt 0 -or
    ($_.aynilik -ne 'AYNI' -and $_.aynilik -ne '-' -and $_.aynilik -ne 'ATLANDI')
})

Write-Output ''
if ($kotu.Count -eq 0) { Write-Output '  GECTI'; exit 0 }
Write-Output ("  KALDI -- {0} arac" -f $kotu.Count)
exit 1
