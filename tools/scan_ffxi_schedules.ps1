param(
    [string]$FfxiRoot = "C:\Program Files (x86)\PlayOnline\SquareEnix\FINAL FANTASY XI",
    [string]$CharacterTable = ".\DATura\character_dat_table.h",
    [string]$AnimationCatalog = ".\tools\ffxi_animation_catalog.csv",
    [string]$OutMarkdown = ".\tools\ffxi_schedule_catalog.md",
    [string]$OutCsv = ".\tools\ffxi_schedule_catalog.csv"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Read-U32([byte[]]$Bytes, [int]$Offset) { [BitConverter]::ToUInt32($Bytes, $Offset) }

function Get-CharacterEntries([string]$Path) {
    $text = Get-Content -LiteralPath $Path -Raw
    $raceNameByArray = @{}
    foreach ($match in [regex]::Matches($text, '\{\s*"([^"]+)"\s*,\s*(k\w+Entries)\s*,')) {
        $raceNameByArray[$match.Groups[2].Value] = $match.Groups[1].Value
    }

    $entries = New-Object System.Collections.Generic.List[object]
    foreach ($match in [regex]::Matches($text, 'static const FFXICharEntry\s+(k\w+Entries)\[\]\s*=\s*\{(?<body>.*?)\};', [Text.RegularExpressions.RegexOptions]::Singleline)) {
        $arrayName = $match.Groups[1].Value
        $raceName = if ($raceNameByArray.ContainsKey($arrayName)) { $raceNameByArray[$arrayName] } else { $arrayName }
        foreach ($entry in [regex]::Matches($match.Groups["body"].Value, '\{\s*"([^"]+)"\s*,\s*"([^"]+)"\s*\}')) {
            $entries.Add([pscustomobject]@{
                Race = $raceName
                Label = $entry.Groups[1].Value
                Dat = $entry.Groups[2].Value
            })
        }
    }
    return $entries
}

function Get-AsciiTokens([byte[]]$Bytes, [int]$Start, [int]$Length) {
    $ascii = [Text.Encoding]::ASCII.GetString($Bytes, $Start, $Length)
    $tokens = New-Object System.Collections.Generic.List[string]
    foreach ($match in [regex]::Matches($ascii, '[A-Za-z0-9 _]{3,4}')) {
        $token = $match.Value.Trim()
        if ($token -match '[A-Za-z]' -and !$tokens.Contains($token)) {
            $tokens.Add($token)
        }
    }
    return @($tokens)
}

function Scan-ScheduleDat([string]$FullPath, [string]$RelativePath, [string[]]$KnownViewerNames) {
    if (!(Test-Path -LiteralPath $FullPath)) {
        return @([pscustomobject]@{
            Dat = $RelativePath; Schedule = ""; ChunkIndex = ""; Offset = ""; Size = ""
            Tokens = ""; MotionRefs = ""; OtherRefs = "missing file"
        })
    }

    [byte[]]$bytes = [IO.File]::ReadAllBytes($FullPath)
    $rows = New-Object System.Collections.Generic.List[object]
    $offset = 0
    $chunkIndex = 0
    while ($offset -le $bytes.Length - 16) {
        $name = [Text.Encoding]::ASCII.GetString($bytes, $offset, 4).TrimEnd([char]0)
        $info = Read-U32 $bytes ($offset + 4)
        $type = $info -band 0x7f
        $size = ($info -shr 3) -band 0x7ffff0
        if ($size -le 0 -or ($offset + $size) -gt $bytes.Length) {
            break
        }

        if ($type -eq 0x07) {
            $tokens = @(Get-AsciiTokens $bytes ($offset + 16) ($size - 16))
            $motionRefs = @($tokens | Where-Object { $KnownViewerNames -contains $_ })
            $otherRefs = @($tokens | Where-Object { !($KnownViewerNames -contains $_) })
            $rows.Add([pscustomobject]@{
                Dat = $RelativePath
                Schedule = $name
                ChunkIndex = $chunkIndex
                Offset = $offset
                Size = $size
                Tokens = $tokens -join ","
                MotionRefs = $motionRefs -join ","
                OtherRefs = $otherRefs -join ","
            })
        }

        $offset += $size
        ++$chunkIndex
    }

    return $rows
}

if (!(Test-Path -LiteralPath $AnimationCatalog)) {
    throw "Animation catalog was not found. Run tools/scan_ffxi_animations.ps1 first."
}

$root = (Resolve-Path -LiteralPath $FfxiRoot).Path
$animationRows = Import-Csv -LiteralPath $AnimationCatalog
$viewerNamesByRace = @{}
foreach ($group in ($animationRows | Where-Object { $_.ViewerName } | Group-Object Race)) {
    $viewerNamesByRace[$group.Name] = @($group.Group | Select-Object -ExpandProperty ViewerName -Unique)
}

$characterEntries = Get-CharacterEntries $CharacterTable
$scheduleSources = $characterEntries |
    Where-Object { $_.Label -eq "Skeleton + Base Tex" -or $_.Label -like "Animation set*" } |
    Sort-Object Race, Dat -Unique

$rows = New-Object System.Collections.Generic.List[object]
foreach ($entry in $scheduleSources) {
    $knownNames = if ($viewerNamesByRace.ContainsKey($entry.Race)) { $viewerNamesByRace[$entry.Race] } else { @() }
    $fullPath = Join-Path $root ($entry.Dat -replace '/', '\')
    $scanRows = Scan-ScheduleDat $fullPath $entry.Dat $knownNames
    foreach ($row in $scanRows) {
        $rows.Add([pscustomobject]@{
            Race = $entry.Race
            Source = $entry.Label
            Dat = $row.Dat
            Schedule = $row.Schedule
            ChunkIndex = $row.ChunkIndex
            Offset = $row.Offset
            Size = $row.Size
            Tokens = $row.Tokens
            MotionRefs = $row.MotionRefs
            OtherRefs = $row.OtherRefs
        })
    }
}

$rows | Export-Csv -LiteralPath $OutCsv -NoTypeInformation

$lines = New-Object System.Collections.Generic.List[string]
$lines.Add("# FFXI Character Schedule Catalog")
$lines.Add("")
$lines.Add("Generated from ``DATura/character_dat_table.h`` by ``tools/scan_ffxi_schedules.ps1``.")
$lines.Add("")
$lines.Add("Schedule chunks are type ``0x07`` records. ``MotionRefs`` are embedded tokens that match known motion display names from the animation catalog; ``OtherRefs`` are still-undecoded helper/action/effect tokens.")
$lines.Add("")

foreach ($raceGroup in ($rows | Group-Object Race)) {
    $lines.Add("## $($raceGroup.Name)")
    $lines.Add("")
    foreach ($datGroup in ($raceGroup.Group | Group-Object Dat)) {
        $source = ($datGroup.Group | Select-Object -First 1).Source
        $lines.Add("### $source - ``$($datGroup.Name)``")
        $lines.Add("")
        $lines.Add("| Schedule | MotionRefs | OtherRefs | Size | Offset |")
        $lines.Add("| --- | --- | --- | ---: | ---: |")
        foreach ($row in ($datGroup.Group | Sort-Object ChunkIndex)) {
            $lines.Add("| ``$($row.Schedule)`` | ``$($row.MotionRefs)`` | ``$($row.OtherRefs)`` | $($row.Size) | $($row.Offset) |")
        }
        $lines.Add("")
    }
}

Set-Content -LiteralPath $OutMarkdown -Value $lines -Encoding UTF8
Write-Host "Wrote $OutMarkdown"
Write-Host "Wrote $OutCsv"
