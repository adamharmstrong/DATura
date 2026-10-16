param([string]$GameRoot = 'C:\Program Files (x86)\PlayOnline\SquareEnix\FINAL FANTASY XI')
$ErrorActionPreference = 'Stop'
$baseZones = @(0,100,101,102,103,104,105,106,107,108,109,110,111,112,115,116,117,118,119,120,126,127,131,139,140,141,142,143,144,145,146,147,148,149,150,151,152,157,158,161,162,165,166,167,169,170,172,184,190,191,192,193,194,195,196,197,198,200,204,206,220,221,223,224,225,227,228,230,231,232,233,234,235,236,237,238,239,240,241,242,243,244,245,246,248,249)
$audit = & "$PSScriptRoot/audit_mainland_exit_records.ps1" -GameRoot $GameRoot -SelectedZones $baseZones | ConvertFrom-Json
$header = Get-Content "$PSScriptRoot/../DATura/zone_transition.h" -Raw
$routes = @()
function Numbers([string]$text) {
    return @($text.Split(',') | ForEach-Object { [double]::Parse(($_.Trim() -replace 'f$',''),[Globalization.CultureInfo]::InvariantCulture) })
}
foreach ($m in [regex]::Matches($header,'\{ "([^"]+)", (\d+), (\d+), \{([^}]+)\}, \{([^}]+)\}')) {
    $token = $m.Groups[1].Value
    if ($token -eq 'mh_bm_n') {$token='zmrc'}
    if ($token -eq 'mh_bm_s') {$token='zmra'}
    $routes += [pscustomobject]@{Token=$token;From=[int]$m.Groups[2].Value;Outward=(Numbers $m.Groups[5].Value)}
}
function DistanceSquared($a,$b) {
    $sum=0.; for($axis=0;$axis -lt 3;$axis++) {$sum+=($a[$axis]-$b[$axis])*($a[$axis]-$b[$axis])};return $sum
}
$issues=@();$competition=@();$checked=0;$samples=0;$eligible=0;$alignments=@();$floorWarnings=@()
foreach ($zone in $audit.Zones | Where-Object Selected) {
    $candidates=@()
    foreach ($v in $zone.Volumes | Where-Object Target) {
        $matching=@($routes | Where-Object {$_.From -eq $zone.Id -and $_.Token -eq $v.Token})
        if($matching.Count -ne 1) {$issues += "$($zone.Id):$($v.Token) route count $($matching.Count)";continue}
        $route=$matching[0]
        $c=[math]::Cos($v.Yaw);$s=[math]::Sin($v.Yaw)
        $normal = if($v.Size[0] -lt $v.Size[2]) {@($c,0.,-$s)} else {@($s,0.,$c)}
        $alignment=$normal[0]*$route.Outward[0]+$normal[2]*$route.Outward[2]
        if([math]::Abs($alignment) -lt .98) {$alignments += "$($zone.Id):$($v.Token) alignment=$alignment"}
        if($alignment -lt 0){$normal=@($normal | ForEach-Object {-$_})}
        $arrival=$zone.Volumes | Where-Object {-not $_.Target} | Sort-Object {DistanceSquared $_.Center $v.Center} | Select-Object -First 1
        $side=($arrival.Center[0]-$v.Center[0])*$normal[0]+($arrival.Center[2]-$v.Center[2])*$normal[2]
        if($side -ge 0){$issues += "$($zone.Id):$($v.Token) dots on opposite side from nearest arrival (projection=$side)"}
        $depth=[math]::Min($v.Size[0],$v.Size[2])*.5
        $threshold=@(($v.Center[0]-$normal[0]*$depth),$v.Center[1],($v.Center[2]-$normal[2]*$depth))
        $candidate=[pscustomobject]@{Volume=$v;Normal=$normal;Threshold=$threshold;Width=[math]::Max($v.Size[0],$v.Size[2])*.5;Arrival=$arrival}
        $candidates += $candidate;$checked++
        if([math]::Abs($arrival.Center[1]-$v.Center[1]) -ge $v.Size[1]*.5){$floorWarnings += "$($zone.Id):$($v.Token) nearest arrival outside trigger floor"}
    }
    foreach($expected in $candidates) {
        foreach($distance in @(5.,15.,30.)) {
            foreach($lateralFraction in @(0.,-.4,.4)) {
                $n=$expected.Normal;$t=$expected.Threshold
                $lateral=$expected.Width*$lateralFraction
                $player=@(($t[0]-$n[0]*$distance+$n[2]*$lateral),$expected.Arrival.Center[1],($t[2]-$n[2]*$distance-$n[0]*$lateral))
                $nearest=1e30;$chosen=$null
                foreach($candidate in $candidates) {
                    $v=$candidate.Volume;$normal=$candidate.Normal
                    if([math]::Abs($player[1]-$v.Center[1]) -ge $v.Size[1]*.5){continue}
                    $dx=$player[0]-$candidate.Threshold[0];$dz=$player[2]-$candidate.Threshold[2]
                    if([math]::Abs($dx*$normal[2]-$dz*$normal[0]) -gt $candidate.Width+2){continue}
                    $d=[math]::Abs($dx*$normal[0]+$dz*$normal[2])
                    if($d -lt $nearest){$nearest=$d;$chosen=$candidate}
                }
                $samples++
                if($chosen -and $nearest -lt 35){$eligible++}
                if(-not $chosen -or $nearest -ge 35){$issues += "$($zone.Id):$($expected.Volume.Token) no row at distance=$distance lateral=$lateralFraction"}
                elseif($chosen.Volume.Token -ne $expected.Volume.Token){$competition += "$($zone.Id):$($expected.Volume.Token) selects $($chosen.Volume.Token) at distance=$distance lateral=$lateralFraction"}
            }
        }
    }
}
[pscustomobject]@{Checked=$checked;Samples=$samples;Eligible=$eligible;Issues=$issues;CompetingRows=$competition;NormalAlignmentWarnings=$alignments;ArrivalFloorWarnings=$floorWarnings} | ConvertTo-Json -Depth 5
