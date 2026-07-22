param(
    [Parameter(Mandatory = $true)]
    [string]$Path,

    [int]$MaxStrings = 80
)

$bytes = [System.IO.File]::ReadAllBytes($Path)
$size = $bytes.Length

function U16($o) { [BitConverter]::ToUInt16($bytes, $o) }
function I16($o) { [BitConverter]::ToInt16($bytes, $o) }
function U32($o) { [BitConverter]::ToUInt32($bytes, $o) }
function F32($o) { [BitConverter]::ToSingle($bytes, $o) }

function FindPattern([byte[]]$pat) {
    $hits = New-Object System.Collections.Generic.List[int]
    for ($i = 0; $i -le $size - $pat.Length; ++$i) {
        $ok = $true
        for ($j = 0; $j -lt $pat.Length; ++$j) {
            if ($bytes[$i + $j] -ne $pat[$j]) { $ok = $false; break }
        }
        if ($ok) { $hits.Add($i) }
    }
    return $hits
}

Write-Host "File: $Path"
Write-Host "Size: $size"
Write-Host ("Magic: {0}" -f ([Text.Encoding]::ASCII.GetString($bytes, 0, [Math]::Min(16, $size))))

Write-Host "`nHeader u32:"
for ($o = 0; $o -lt [Math]::Min(0x100, $size); $o += 4) {
    Write-Host ("  {0:X6}: {1:X8} {2,12}  f={3}" -f $o, (U32 $o), (U32 $o), (F32 $o))
}

$bad = FindPattern ([byte[]](0xBA,0xDD,0xBE,0xEF))
$feed = FindPattern ([byte[]](0xFE,0xED,0xFE,0xED))
Write-Host "`nBADDBEEF hits:" ($bad | ForEach-Object { ("0x{0:X}" -f $_) })
Write-Host "FEEDFEED hits:" ($feed | ForEach-Object { ("0x{0:X}" -f $_) })

Write-Host "`nASCII strings:"
$count = 0
$i = 0
while ($i -lt $size -and $count -lt $MaxStrings) {
    if ($bytes[$i] -ge 32 -and $bytes[$i] -le 126) {
        $s = $i
        while ($i -lt $size -and $bytes[$i] -ge 32 -and $bytes[$i] -le 126) { ++$i }
        $len = $i - $s
        if ($len -ge 4) {
            $text = [Text.Encoding]::ASCII.GetString($bytes, $s, $len)
            Write-Host ("  {0:X6}: {1}" -f $s, $text)
            ++$count
        }
    }
    ++$i
}

Write-Host "`nCandidate float3 runs (first 40):"
$runs = 0
for ($o = 0; $o -le $size - 36 -and $runs -lt 40; $o += 4) {
    $good = 0
    for ($k = 0; $k -lt 9; ++$k) {
        $v = F32 ($o + $k * 4)
        if ([float]::IsNaN($v) -or [float]::IsInfinity($v)) { break }
        if ([Math]::Abs($v) -lt 10000.0 -and ([Math]::Abs($v) -gt 0.000001 -or $v -eq 0.0)) { ++$good }
    }
    if ($good -ge 9) {
        $vals = 0..8 | ForEach-Object { "{0,9:F3}" -f (F32 ($o + $_ * 4)) }
        Write-Host ("  {0:X6}: {1}" -f $o, ($vals -join " "))
        ++$runs
        $o += 128
    }
}
