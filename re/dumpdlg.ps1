#requires -Version 5.1
<#
    dumpdlg.ps1

    RT_DIALOG / DLGTEMPLATEEX sablonlarini .rc benzeri metne cevirir.

    cleanmgr'in butun yerlesimi bu sablonlarda; klonun "birebir" olmasi
    icin denetim kimlikleri, siniflari, stilleri ve DLU koordinatlarinin
    tamaminin buradan cikarilmasi gerekir.

        dumpdlg.ps1 -Path C:\Windows\System32\cleanmgr.exe
        dumpdlg.ps1 -Path ... -Id 101
#>
param(
    [string]$Path = 'C:\Windows\System32\cleanmgr.exe',
    [int]   $Id   = 0
)

Add-Type @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;

public class DlgRes
{
    public const uint AS_DATAFILE = 0x00000002;
    public const uint AS_IMAGE    = 0x00000020;

    [DllImport("kernel32", CharSet = CharSet.Unicode, SetLastError = true)]
    public static extern IntPtr LoadLibraryExW(string f, IntPtr h, uint flags);
    [DllImport("kernel32", CharSet = CharSet.Unicode, SetLastError = true)]
    public static extern IntPtr FindResourceExW(IntPtr h, IntPtr type, IntPtr name, ushort lang);
    [DllImport("kernel32", SetLastError = true)]
    public static extern IntPtr LoadResource(IntPtr h, IntPtr hRes);
    [DllImport("kernel32", SetLastError = true)]
    public static extern IntPtr LockResource(IntPtr hData);
    [DllImport("kernel32", SetLastError = true)]
    public static extern uint SizeofResource(IntPtr h, IntPtr hRes);

    public delegate bool EnumNameProc(IntPtr h, IntPtr type, IntPtr name, IntPtr lp);
    public delegate bool EnumLangProc(IntPtr h, IntPtr type, IntPtr name, ushort lang, IntPtr lp);

    [DllImport("kernel32", CharSet = CharSet.Unicode, SetLastError = true)]
    public static extern bool EnumResourceNamesW(IntPtr h, IntPtr type, EnumNameProc cb, IntPtr lp);
    [DllImport("kernel32", CharSet = CharSet.Unicode, SetLastError = true)]
    public static extern bool EnumResourceLanguagesW(IntPtr h, IntPtr type, IntPtr name, EnumLangProc cb, IntPtr lp);

    public static List<int>    Ids   = new List<int>();
    public static List<ushort> Langs = new List<ushort>();

    public static void List(IntPtr h)
    {
        EnumResourceNamesW(h, (IntPtr)5 /*RT_DIALOG*/, delegate(IntPtr hm, IntPtr t, IntPtr n, IntPtr lp)
        {
            EnumResourceLanguagesW(hm, t, n, delegate(IntPtr hm2, IntPtr t2, IntPtr n2, ushort lang, IntPtr lp2)
            {
                Ids.Add((int)n2.ToInt64());
                Langs.Add(lang);
                return true;
            }, IntPtr.Zero);
            return true;
        }, IntPtr.Zero);
    }

    public static byte[] Get(IntPtr h, int id, ushort lang)
    {
        IntPtr hr = FindResourceExW(h, (IntPtr)5, (IntPtr)id, lang);
        if (hr == IntPtr.Zero) return null;
        uint sz = SizeofResource(h, hr);
        IntPtr hg = LoadResource(h, hr);
        IntPtr p  = LockResource(hg);
        byte[] b = new byte[sz];
        Marshal.Copy(p, b, 0, (int)sz);
        return b;
    }
}
'@

$h = [DlgRes]::LoadLibraryExW($Path, [IntPtr]::Zero, ([DlgRes]::AS_DATAFILE -bor [DlgRes]::AS_IMAGE))
if ($h -eq [IntPtr]::Zero) { Write-Output "LoadLibraryEx FAILED"; exit 1 }

# ---------------------------------------------------------------------------
#  Sablon cozumleyici
# ---------------------------------------------------------------------------

$script:buf = $null
$script:off = 0

function R-U16 { $v = [BitConverter]::ToUInt16($script:buf, $script:off); $script:off += 2; return $v }
function R-U32 { $v = [BitConverter]::ToUInt32($script:buf, $script:off); $script:off += 4; return $v }
function R-I16 { $v = [BitConverter]::ToInt16($script:buf, $script:off); $script:off += 2; return $v }
function Align4 { if ($script:off % 4) { $script:off += (4 - ($script:off % 4)) } }

# sz_Or_Ord: 0x0000 -> bos, 0xFFFF -> ordinal, degilse UTF-16 dize
function R-SzOrd {
    $w = [BitConverter]::ToUInt16($script:buf, $script:off)
    if ($w -eq 0x0000) { $script:off += 2; return '' }
    if ($w -eq 0xFFFF) { $script:off += 2; $o = [BitConverter]::ToUInt16($script:buf, $script:off); $script:off += 2; return ('#' + $o) }
    $sb = New-Object System.Text.StringBuilder
    while ($true) {
        $c = [BitConverter]::ToUInt16($script:buf, $script:off); $script:off += 2
        if ($c -eq 0) { break }
        [void]$sb.Append([char]$c)
    }
    return $sb.ToString()
}

$WS = @{
    0x80000000 = 'WS_POPUP'; 0x40000000='WS_CHILD'; 0x20000000='WS_MINIMIZE'
    0x10000000 = 'WS_VISIBLE'; 0x08000000='WS_DISABLED'; 0x04000000='WS_CLIPSIBLINGS'
    0x02000000 = 'WS_CLIPCHILDREN'; 0x01000000='WS_MAXIMIZE'; 0x00800000='WS_BORDER'
    0x00400000 = 'WS_DLGFRAME'; 0x00200000='WS_VSCROLL'; 0x00100000='WS_HSCROLL'
    0x00080000 = 'WS_SYSMENU'; 0x00040000='WS_THICKFRAME'; 0x00020000='WS_GROUP'
    0x00010000 = 'WS_TABSTOP'
}
$DS = @{
    0x0001='DS_ABSALIGN'; 0x0002='DS_SYSMODAL'; 0x0004='DS_3DLOOK'; 0x0008='DS_FIXEDSYS'
    0x0010='DS_NOFAILCREATE'; 0x0020='DS_LOCALEDIT'; 0x0040='DS_SETFONT'
    0x0080='DS_MODALFRAME'; 0x0100='DS_NOIDLEMSG'; 0x0200='DS_SETFOREGROUND'
    0x0400='DS_CONTROL'; 0x0800='DS_CENTER'; 0x1000='DS_CENTERMOUSE'
    0x2000='DS_CONTEXTHELP'; 0x10000='DS_SHELLFONT_HI'
}
$EX = @{
    0x00000001='WS_EX_DLGMODALFRAME'; 0x00000004='WS_EX_NOPARENTNOTIFY'; 0x00000008='WS_EX_TOPMOST'
    0x00000010='WS_EX_ACCEPTFILES'; 0x00000020='WS_EX_TRANSPARENT'; 0x00000040='WS_EX_MDICHILD'
    0x00000080='WS_EX_TOOLWINDOW'; 0x00000100='WS_EX_WINDOWEDGE'; 0x00000200='WS_EX_CLIENTEDGE'
    0x00000400='WS_EX_CONTEXTHELP'; 0x00001000='WS_EX_RIGHT'; 0x00002000='WS_EX_RTLREADING'
    0x00004000='WS_EX_LEFTSCROLLBAR'; 0x00010000='WS_EX_CONTROLPARENT'; 0x00020000='WS_EX_STATICEDGE'
    0x00040000='WS_EX_APPWINDOW'; 0x00080000='WS_EX_LAYERED'; 0x00100000='WS_EX_NOINHERITLAYOUT'
    0x00400000='WS_EX_LAYOUTRTL'; 0x02000000='WS_EX_COMPOSITED'; 0x08000000='WS_EX_NOACTIVATE'
}

$CLS = @{ 0x0080='BUTTON'; 0x0081='EDIT'; 0x0082='STATIC'; 0x0083='LISTBOX'; 0x0084='SCROLLBAR'; 0x0085='COMBOBOX' }

$BS = @('BS_PUSHBUTTON','BS_DEFPUSHBUTTON','BS_CHECKBOX','BS_AUTOCHECKBOX','BS_RADIOBUTTON',
        'BS_3STATE','BS_AUTO3STATE','BS_GROUPBOX','BS_USERBUTTON','BS_AUTORADIOBUTTON',
        'BS_PUSHBOX','BS_OWNERDRAW','BS_SPLITBUTTON','BS_DEFSPLITBUTTON','BS_COMMANDLINK','BS_DEFCOMMANDLINK')
$SS = @('SS_LEFT','SS_CENTER','SS_RIGHT','SS_ICON','SS_BLACKRECT','SS_GRAYRECT','SS_WHITERECT',
        'SS_BLACKFRAME','SS_GRAYFRAME','SS_WHITEFRAME','SS_USERITEM','SS_SIMPLE','SS_LEFTNOWORDWRAP',
        'SS_OWNERDRAW','SS_BITMAP','SS_ENHMETAFILE','SS_ETCHEDHORZ','SS_ETCHEDVERT','SS_ETCHEDFRAME')

function Flags($v, $tbl) {
    $o = @()
    foreach ($k in ($tbl.Keys | Sort-Object -Descending)) { if ($v -band $k) { $o += $tbl[$k] } }
    return $o
}

function ClassStyle($cls, $style) {
    $o = @()
    switch ($cls) {
        'BUTTON' {
            $t = $style -band 0x0F
            if ($t -lt $BS.Count) { $o += $BS[$t] }
            if ($style -band 0x0020) { $o += 'BS_LEFTTEXT' }
            if ($style -band 0x0040) { $o += 'BS_ICON/TEXT' }
            if ($style -band 0x2000) { $o += 'BS_MULTILINE' }
            if ($style -band 0x4000) { $o += 'BS_NOTIFY' }
            if ($style -band 0x8000) { $o += 'BS_FLAT' }
        }
        'STATIC' {
            $t = $style -band 0x1F
            if ($t -lt $SS.Count) { $o += $SS[$t] }
            if ($style -band 0x0080) { $o += 'SS_NOPREFIX' }
            if ($style -band 0x0100) { $o += 'SS_NOTIFY' }
            if ($style -band 0x0200) { $o += 'SS_CENTERIMAGE' }
            if ($style -band 0x0400) { $o += 'SS_RIGHTJUST' }
            if ($style -band 0x0800) { $o += 'SS_REALSIZEIMAGE' }
            if ($style -band 0x1000) { $o += 'SS_SUNKEN' }
            if ($style -band 0x8000) { $o += 'SS_ENDELLIPSIS?' }
        }
        'EDIT' {
            if ($style -band 0x0002) { $o += 'ES_UPPERCASE' }
            if ($style -band 0x0004) { $o += 'ES_MULTILINE' }
            if ($style -band 0x0008) { $o += 'ES_UPPERCASE2' }
            if ($style -band 0x0020) { $o += 'ES_PASSWORD' }
            if ($style -band 0x0040) { $o += 'ES_AUTOVSCROLL' }
            if ($style -band 0x0080) { $o += 'ES_AUTOHSCROLL' }
            if ($style -band 0x0800) { $o += 'ES_READONLY' }
        }
        'COMBOBOX' {
            $t = $style -band 0x03
            $o += @('CBS_SIMPLE','CBS_DROPDOWN','CBS_DROPDOWNLIST')[[Math]::Min($t,2)]
            if ($style -band 0x0100) { $o += 'CBS_SORT' }
            if ($style -band 0x0200) { $o += 'CBS_HASSTRINGS' }
            if ($style -band 0x0010) { $o += 'CBS_OWNERDRAWFIXED' }
        }
    }
    return $o
}

function Dump-One([int]$id, [UInt16]$lang) {
    $b = [DlgRes]::Get($h, $id, $lang)
    if ($b -eq $null) { Write-Output "  (bulunamadi)"; return }

    $script:buf = $b
    $script:off = 0

    $sig = R-U16          # dlgVer for EX = 1 ... actually first U16 is dlgVer
    $sig2 = R-U16         # signature 0xFFFF for DLGTEMPLATEEX
    $isEx = ($sig2 -eq 0xFFFF)

    Write-Output ("========================================================================")
    Write-Output ("DIALOG {0}   lang=0x{1:X4}   {2}   ({3} bayt)" -f $id, $lang, $(if($isEx){'DIALOGEX'}else{'DIALOG'}), $b.Length)
    Write-Output ("========================================================================")

    if ($isEx) {
        $helpId = R-U32
        $exSty  = R-U32
        $style  = R-U32
        $cdit   = R-U16
        $x = R-I16; $y = R-I16; $cx = R-I16; $cy = R-I16
    } else {
        $script:off = 0
        $style = R-U32
        $exSty = R-U32
        $cdit  = R-U16
        $x = R-I16; $y = R-I16; $cx = R-I16; $cy = R-I16
        $helpId = 0
    }

    $menu  = R-SzOrd
    $class = R-SzOrd
    $title = R-SzOrd

    Write-Output ("  KONUM   x={0} y={1} cx={2} cy={3}   (DLU)" -f $x,$y,$cx,$cy)
    Write-Output ("  BASLIK  `"{0}`"" -f $title)
    if ($menu  -ne '') { Write-Output ("  MENU    {0}" -f $menu) }
    if ($class -ne '') { Write-Output ("  CLASS   {0}" -f $class) }
    Write-Output ("  STYLE   0x{0:X8}  {1}" -f $style, ((Flags $style $WS) + (Flags ($style -band 0xFFFF) $DS) -join ' | '))
    Write-Output ("  EXSTYLE 0x{0:X8}  {1}" -f $exSty, ((Flags $exSty $EX) -join ' | '))
    if ($helpId -ne 0) { Write-Output ("  HELPID  {0}" -f $helpId) }

    if ($style -band 0x40 <# DS_SETFONT #> -or $style -band 0x1000000) {
        $pt = R-U16
        if ($isEx) {
            $wt = R-U16; $it = $script:buf[$script:off]; $script:off += 1; $cs = $script:buf[$script:off]; $script:off += 1
            $fn = R-SzOrd
            Write-Output ("  FONT    {0} pt `"{1}`"  weight={2} italic={3} charset={4}" -f $pt,$fn,$wt,$it,$cs)
        } else {
            $fn = R-SzOrd
            Write-Output ("  FONT    {0} pt `"{1}`"" -f $pt,$fn)
        }
    }

    Write-Output ""
    Write-Output ("  {0} denetim:" -f $cdit)
    Write-Output ("  {0,-6} {1,-22} {2,-4} {3,-4} {4,-4} {5,-4}  {6}" -f 'ID','SINIF','x','y','cx','cy','METIN')
    Write-Output ("  " + ('-' * 110))

    for ($i = 0; $i -lt $cdit; $i++) {
        Align4
        if ($isEx) {
            $chelp = R-U32; $cex = R-U32; $csty = R-U32
            $cxp = R-I16; $cyp = R-I16; $ccx = R-I16; $ccy = R-I16
            $cid = R-U32
        } else {
            $csty = R-U32; $cex = R-U32
            $cxp = R-I16; $cyp = R-I16; $ccx = R-I16; $ccy = R-I16
            $cid = R-U16
            $chelp = 0
        }
        $ccls = R-SzOrd
        $ctxt = R-SzOrd
        $cbExtra = R-U16
        if ($cbExtra -gt 0) { $script:off += $cbExtra }

        $clsName = $ccls
        if ($ccls.StartsWith('#')) {
            $ord = [int]$ccls.Substring(1)
            if ($CLS.ContainsKey($ord)) { $clsName = $CLS[$ord] }
        }

        Write-Output ("  {0,-6} {1,-22} {2,-4} {3,-4} {4,-4} {5,-4}  `"{6}`"" -f $cid, $clsName, $cxp, $cyp, $ccx, $ccy, $ctxt)
        $fl = (ClassStyle $clsName $csty) + (Flags $csty $WS)
        Write-Output ("         style=0x{0:X8}  {1}" -f $csty, ($fl -join ' | '))
        if ($cex -ne 0) { Write-Output ("         exstyle=0x{0:X8}  {1}" -f $cex, ((Flags $cex $EX) -join ' | ')) }
    }
    Write-Output ""
}

[DlgRes]::List($h)

if ($Id -ne 0) {
    for ($i = 0; $i -lt [DlgRes]::Ids.Count; $i++) {
        if ([DlgRes]::Ids[$i] -eq $Id) { Dump-One ([DlgRes]::Ids[$i]) ([DlgRes]::Langs[$i]) }
    }
} else {
    Write-Output ("Toplam {0} diyalog: {1}" -f [DlgRes]::Ids.Count, ([DlgRes]::Ids -join ', '))
    Write-Output ""
    for ($i = 0; $i -lt [DlgRes]::Ids.Count; $i++) {
        Dump-One ([DlgRes]::Ids[$i]) ([DlgRes]::Langs[$i])
    }
}
