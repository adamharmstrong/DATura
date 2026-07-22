param(
    [string]$Path,
    [string]$ChunkName
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

[byte[]]$bytes = [IO.File]::ReadAllBytes($Path)
$offset = 0
$found = $false
while ($offset -le $bytes.Length - 16) {
    $name = [Text.Encoding]::ASCII.GetString($bytes, $offset, 4)
    $info = [BitConverter]::ToUInt32($bytes, $offset + 4)
    $type = $info -band 0x7f
    $size = ($info -shr 3) -band 0x7ffff0
    if ($size -le 0 -or ($offset + $size) -gt $bytes.Length) { break }

    if ($name -eq $ChunkName) {
        $found = $true
        Write-Host "--- $name offset=$offset type=0x$($type.ToString('X2')) size=$size data=$($size - 16)"
        $dataOffset = $offset + 16
        $max = [Math]::Min($size - 16, 160)
        for ($i = 0; $i -lt $max; $i += 16) {
            $hexParts = New-Object System.Collections.Generic.List[string]
            for ($j = 0; $j -lt 16 -and ($i + $j) -lt ($size - 16); ++$j) {
                $hexParts.Add($bytes[$dataOffset + $i + $j].ToString("X2"))
            }
            $ints = New-Object System.Collections.Generic.List[string]
            for ($j = 0; $j -lt 16 -and ($i + $j + 4) -le ($size - 16); $j += 4) {
                $ints.Add(([BitConverter]::ToInt32($bytes, $dataOffset + $i + $j)).ToString())
            }
            "{0:X4}: {1,-47} | {2}" -f $i, ($hexParts -join " "), ($ints -join ", ")
        }
        break
    }

    $offset += $size
}

if (!$found) {
    throw "Chunk '$ChunkName' was not found."
}
