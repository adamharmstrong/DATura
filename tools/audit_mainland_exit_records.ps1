param(
    [string]$GameRoot = 'C:\Program Files (x86)\PlayOnline\SquareEnix\FINAL FANTASY XI',
    [int[]]$SelectedZones = @(109,119,111,112,147,148,151,152,158,161,162,165,204,206,212)
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$selected = $SelectedZones
$table = Get-Content (Join-Path $repo 'DATura/zone_dat_table.h') -Raw
$zones = @{}
$slugs = @{}
foreach ($m in [regex]::Matches($table, '\{\s*(\d+),\s*"([^"]+)",\s*"([^"]+)"')) {
    $id = [int]$m.Groups[1].Value
    $name = $m.Groups[2].Value
    $slug = ($name.ToLowerInvariant() -replace "['#]",'' -replace '[^a-z0-9]+','_').Trim('_')
    $zones[$id] = @{Id=$id;Name=$name;Path=$m.Groups[3].Value;Slug=$slug}
    $slugs[$slug] = $id
}
# Reference naming differs from the display name for these areas.
$slugs['feiyin'] = 204
function Read-Volumes($zone) {
    $bytes = [IO.File]::ReadAllBytes((Join-Path $GameRoot $zone.Path))
    $out = @()
    for ($p=0; $p -lt $bytes.Length;) {
        $info = [BitConverter]::ToUInt32($bytes,$p+4)
        $len = ($info -shr 3) -band 0x7ffff0
        if ($len -lt 16 -or $p+$len -gt $bytes.Length) { throw 'Invalid DAT chunk' }
        if (($info -band 127) -eq 0x36) {
            $data = $p+16
            $t = $data+[BitConverter]::ToUInt32($bytes,$data+16)
            $count = [BitConverter]::ToUInt32($bytes,$t)
            for ($i=0; $i -lt $count; $i++) {
                $r = $t+16+$i*64
                $token = [Text.Encoding]::ASCII.GetString($bytes,$r+36,4).ToLowerInvariant()
                if (-not $token.StartsWith('z')) { continue }
                $center = @(0,4,8 | ForEach-Object { [BitConverter]::ToSingle($bytes,$r+$_) })
                $size = @(24,28,32 | ForEach-Object { [math]::Abs([BitConverter]::ToSingle($bytes,$r+$_)) })
                $out += @{Token=$token;Center=$center;Size=$size;Yaw=[BitConverter]::ToSingle($bytes,$r+16);Target=[BitConverter]::ToUInt32($bytes,$r+40)}
            }
        }
        $p += $len
    }
    return $out
}
$references = @()
foreach ($dir in Get-ChildItem (Join-Path $repo '.tmp_lsb/data/zones') -Directory) {
    if (-not $slugs.ContainsKey($dir.Name)) { continue }
    $yamlPath = Join-Path $dir.FullName 'zone.yaml'
    if (-not (Test-Path $yamlPath)) { continue }
    $yaml = Get-Content $yamlPath -Raw
    foreach ($m in [regex]::Matches($yaml, '(?ms)^  (z[a-z0-9]{3}):\s*\r?\n(.*?)(?=^  \S|\z)')) {
        $b = $m.Groups[2].Value
        $dest = [regex]::Match($b, '(?m)^    to:\s*(\w+)').Groups[1].Value
        $at = [regex]::Match($b, '(?m)^    at:\s*\[([^\]]+)\]').Groups[1].Value
        if (-not $slugs.ContainsKey($dest) -or -not $at) { continue }
        $from = $slugs[$dir.Name]; $to = $slugs[$dest]
        if ($selected -notcontains $from -and $selected -notcontains $to) { continue }
        $values = @($at.Split(',') | ForEach-Object {[double]::Parse($_,[Globalization.CultureInfo]::InvariantCulture)})
        $references += @{Token=$m.Groups[1].Value;From=$from;To=$to;Arrival=@($values[0..2]);Heading=$(if($values.Count -gt 3){$values[3]}else{0})}
    }
}
$needed = @($selected) + @($references | ForEach-Object {$_.From; $_.To}) | Sort-Object -Unique
$out = @()
foreach ($id in $needed) {
    $z = $zones[$id].Clone()
    $z.Volumes = @(Read-Volumes $z)
    $z.Selected = $selected -contains $id
    $out += $z
}
@{Zones=$out;References=$references} | ConvertTo-Json -Depth 8 -Compress
