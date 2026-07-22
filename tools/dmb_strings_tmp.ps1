param($Path)
$b=[IO.File]::ReadAllBytes($Path)
$tokens = [regex]::Matches([Text.Encoding]::ASCII.GetString($b), '[ -~]{4,}') | ForEach-Object { [pscustomobject]@{Index=$_.Index; Text=$_.Value} }
$tokens | Select-Object -First 100 | Format-Table -Auto
