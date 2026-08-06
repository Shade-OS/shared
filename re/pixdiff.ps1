#requires -Version 5.1
<#
    pixdiff.ps1

    Iki PNG'yi piksel piksel karsilastirir ve farki raporlar.

        pixdiff.ps1 -A orijinal.png -B klon.png
        pixdiff.ps1 -A a.png -B b.png -Mask "40,180,480,160"   (x,y,w,h yok say)
        pixdiff.ps1 -A a.png -B b.png -Out fark.png            (fark haritasi)

    -Mask, ICERIGI CALISTAN CALISA DEGISEN bolgeleri disarida birakmak icin:
    cleanmgr'in listesindeki boyutlar (Geri Donusum Kutusu, WER dosyalari) ya
    da winver'in calisma suresi satiri gibi. O bolgeler maskelenmeden yapilan
    bir karsilastirma her zaman "farkli" der ve hicbir sey ogretmez.

    GetPixel yerine LockBits kullaniliyor; 700x900'luk bir cift icin fark
    saniyeler degil milisaniyeler suruyor.
#>
param(
    [Parameter(Mandatory=$true)][string]$A,
    [Parameter(Mandatory=$true)][string]$B,
    [string]$Mask = '',
    [string]$Out = '',
    [string]$Label = ''
)

Add-Type -AssemblyName System.Drawing

if (-not (Test-Path $A)) { Write-Output "YOK: $A"; exit 2 }
if (-not (Test-Path $B)) { Write-Output "YOK: $B"; exit 2 }

$bmpA = [System.Drawing.Bitmap]::FromFile($A)
$bmpB = [System.Drawing.Bitmap]::FromFile($B)

$ad = if ($Label -ne '') { $Label } else { (Split-Path $A -Leaf) + " vs " + (Split-Path $B -Leaf) }

if ($bmpA.Width -ne $bmpB.Width -or $bmpA.Height -ne $bmpB.Height) {
    Write-Output ("{0}: BOYUT FARKLI  {1}x{2} vs {3}x{4}" -f $ad, $bmpA.Width, $bmpA.Height, $bmpB.Width, $bmpB.Height)
    $bmpA.Dispose(); $bmpB.Dispose()
    exit 1
}

$w = $bmpA.Width; $h = $bmpA.Height
$rect = New-Object System.Drawing.Rectangle 0, 0, $w, $h
$fmt  = [System.Drawing.Imaging.PixelFormat]::Format32bppArgb

$dA = $bmpA.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::ReadOnly, $fmt)
$dB = $bmpB.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::ReadOnly, $fmt)

$n = $w * $h
$pxA = New-Object int[] $n
$pxB = New-Object int[] $n
[System.Runtime.InteropServices.Marshal]::Copy($dA.Scan0, $pxA, 0, $n)
[System.Runtime.InteropServices.Marshal]::Copy($dB.Scan0, $pxB, 0, $n)

$bmpA.UnlockBits($dA)
$bmpB.UnlockBits($dB)

# Maske bolgeleri
$masks = @()
if ($Mask -ne '') {
    foreach ($m in ($Mask -split ';')) {
        $p = $m -split ','
        if ($p.Count -eq 4) {
            $masks += ,@([int]$p[0], [int]$p[1], [int]$p[2], [int]$p[3])
        }
    }
}

$farkli = 0; $sayilan = 0
$minY = [int]::MaxValue; $maxY = -1
$minX = [int]::MaxValue; $maxX = -1
$diff = $null
if ($Out -ne '') { $diff = New-Object int[] $n }

for ($y = 0; $y -lt $h; $y++) {
    $row = $y * $w
    for ($x = 0; $x -lt $w; $x++) {
        $i = $row + $x

        $maskeli = $false
        foreach ($m in $masks) {
            if ($x -ge $m[0] -and $x -lt ($m[0]+$m[2]) -and $y -ge $m[1] -and $y -lt ($m[1]+$m[3])) { $maskeli = $true; break }
        }
        if ($maskeli) { if ($diff) { $diff[$i] = -8355712 } ; continue }

        $sayilan++
        if ($pxA[$i] -ne $pxB[$i]) {
            $farkli++
            if ($y -lt $minY) { $minY = $y }
            if ($y -gt $maxY) { $maxY = $y }
            if ($x -lt $minX) { $minX = $x }
            if ($x -gt $maxX) { $maxX = $x }
            if ($diff) { $diff[$i] = -65536 }      # kirmizi
        }
        elseif ($diff) { $diff[$i] = -16777216 }   # siyah
    }
}

if ($farkli -eq 0) {
    Write-Output ("{0}: {1} pikselin TAMAMI ayni" -f $ad, $sayilan)
} else {
    $pct = [Math]::Round(100.0 * $farkli / $sayilan, 4)
    Write-Output ("{0}: {1}/{2} piksel farkli (%{3})" -f $ad, $farkli, $sayilan, $pct)
    Write-Output ("    farkli bolge: x={0}..{1}  y={2}..{3}" -f $minX, $maxX, $minY, $maxY)
}

if ($diff) {
    $bmpD = New-Object System.Drawing.Bitmap $w, $h, $fmt
    $dD = $bmpD.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::WriteOnly, $fmt)
    [System.Runtime.InteropServices.Marshal]::Copy($diff, 0, $dD.Scan0, $n)
    $bmpD.UnlockBits($dD)
    $bmpD.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmpD.Dispose()
    Write-Output ("    fark haritasi -> {0}" -f $Out)
}

$bmpA.Dispose(); $bmpB.Dispose()
exit $(if ($farkli -eq 0) { 0 } else { 1 })
