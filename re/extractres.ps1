param(
    [string]$Path = 'C:\Windows\System32\winver.exe',
    [string]$Type = '24',
    [string]$Name = '1',
    [int]$Lang = 0x0409,
    [string]$Out = ''
)

Add-Type @'
using System;
using System.Runtime.InteropServices;
public class RX {
    [DllImport("kernel32", CharSet=CharSet.Unicode, SetLastError=true)]
    public static extern IntPtr LoadLibraryExW(string f, IntPtr h, uint flags);
    [DllImport("kernel32", CharSet=CharSet.Unicode, SetLastError=true)]
    public static extern IntPtr FindResourceExW(IntPtr h, IntPtr type, IntPtr name, ushort lang);
    [DllImport("kernel32", SetLastError=true)]
    public static extern IntPtr LoadResource(IntPtr h, IntPtr r);
    [DllImport("kernel32", SetLastError=true)]
    public static extern IntPtr LockResource(IntPtr r);
    [DllImport("kernel32", SetLastError=true)]
    public static extern uint SizeofResource(IntPtr h, IntPtr r);
}
'@

$flags = 0x00000002 -bor 0x00000020   # DATAFILE | IMAGE_RESOURCE
$h = [RX]::LoadLibraryExW($Path, [IntPtr]::Zero, $flags)
if ($h -eq [IntPtr]::Zero) { Write-Output "LoadLibraryEx failed"; exit 1 }

$t = [IntPtr][int]$Type
$n = [IntPtr][int]$Name
$r = [RX]::FindResourceExW($h, $t, $n, [uint16]$Lang)
if ($r -eq [IntPtr]::Zero) { Write-Output ("FindResourceEx failed err=" + [Runtime.InteropServices.Marshal]::GetLastWin32Error()); exit 1 }
$sz = [RX]::SizeofResource($h, $r)
$d = [RX]::LockResource([RX]::LoadResource($h, $r))
$bytes = New-Object byte[] $sz
[Runtime.InteropServices.Marshal]::Copy($d, $bytes, 0, [int]$sz)

if ($Out -ne '') {
    [IO.File]::WriteAllBytes($Out, $bytes)
    Write-Output "wrote $sz bytes -> $Out"
} else {
    Write-Output ([Text.Encoding]::UTF8.GetString($bytes))
}
