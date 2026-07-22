param(
    [string]$FfxiRoot = "C:\Program Files (x86)\PlayOnline\SquareEnix\FINAL FANTASY XI",
    [string]$CharacterTable = ".\DATura\character_dat_table.h",
    [string]$OutMarkdown = ".\tools\ffxi_animation_catalog.md",
    [string]$OutCsv = ".\tools\ffxi_animation_catalog.csv"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Read-U32([byte[]]$Bytes, [int]$Offset) { [BitConverter]::ToUInt32($Bytes, $Offset) }
function Read-U16([byte[]]$Bytes, [int]$Offset) { [BitConverter]::ToUInt16($Bytes, $Offset) }
function Read-I32([byte[]]$Bytes, [int]$Offset) { [BitConverter]::ToInt32($Bytes, $Offset) }
function Read-F32([byte[]]$Bytes, [int]$Offset) { [BitConverter]::ToSingle($Bytes, $Offset) }

function Get-AnimMeaning([string]$Name) {
    $base = $Name
    $half = "unknown"
    if ($Name -match "^(.*)([01])$") {
        $base = $Matches[1]
        $half = if ($Matches[2] -eq "0") { "root/lower-body half" } else { "upper/body half" }
    }

    $meaning = switch -Regex ($base) {
        "^wlk$" { "walk"; break }
        "^run$" { "run"; break }
        "^idl$" { "idle"; break }
        "^std$" { "standard/standing cycle"; break }
        "^mvb$" { "move backward"; break }
        "^mvl$" { "strafe/turn left"; break }
        "^mvr$" { "strafe/turn right"; break }
        "^cm0$" { "combat/stance group 0"; break }
        "^cm1$" { "combat/stance group 1"; break }
        "^btl$" { "battle stance"; break }
        default { "unidentified" }
    }

    if ($half -ne "unknown") {
        return "$meaning ($half)"
    }
    return $meaning
}

function Get-ViewerAnimName([string]$Name) {
    if ($Name -match "^(.*\d)[01]$") {
        return $Matches[1]
    }
    if ($Name -match "^(.*)[01]$") {
        return $Matches[1]
    }
    if ($Name -match "^([A-Za-z]+)\d+$") {
        return $Matches[1]
    }
    return $Name
}

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

function Scan-AnimationDat([string]$FullPath, [string]$RelativePath) {
    if (!(Test-Path -LiteralPath $FullPath)) {
        return @([pscustomobject]@{
            Dat = $RelativePath; Chunk = ""; ViewerName = ""; ChunkIndex = ""; Offset = ""; Frames = ""; Fps = ""
            Elements = ""; Bones = ""; Meaning = "missing file"
        })
    }

    [byte[]]$bytes = [IO.File]::ReadAllBytes($FullPath)
    $rows = New-Object System.Collections.Generic.List[object]
    $offset = 0
    $chunkIndex = 0
    while ($offset -le $bytes.Length - 16) {
        $name = [Text.Encoding]::ASCII.GetString($bytes, $offset, 4)
        $info = Read-U32 $bytes ($offset + 4)
        $type = $info -band 0x7f
        $size = ($info -shr 3) -band 0x7ffff0
        if ($size -le 0 -or ($offset + $size) -gt $bytes.Length) {
            break
        }

        if ($type -eq 0x2b) {
            $dataOffset = $offset + 16
            $elemCount = Read-U16 $bytes ($dataOffset + 2)
            $frameCount = Read-U16 $bytes ($dataOffset + 4)
            $speedScale = Read-F32 $bytes ($dataOffset + 6)
            $bones = New-Object System.Collections.Generic.List[int]
            for ($elemIndex = 0; $elemIndex -lt $elemCount; ++$elemIndex) {
                $elemOffset = $dataOffset + 10 + ($elemIndex * 84)
                if (($elemOffset + 4) -le $bytes.Length) {
                    $bones.Add((Read-I32 $bytes $elemOffset))
                }
            }

            $uniqueBones = @($bones | Sort-Object -Unique)
            $boneText = if ($uniqueBones.Count -gt 0) { $uniqueBones -join "," } else { "" }
            $rows.Add([pscustomobject]@{
                Dat = $RelativePath
                Chunk = $name
                ViewerName = Get-ViewerAnimName $name
                ChunkIndex = $chunkIndex
                Offset = $offset
                Frames = $frameCount
                Fps = [Math]::Round($speedScale * 30.0, 3)
                Elements = $elemCount
                Bones = $boneText
                Meaning = Get-AnimMeaning $name
            })
        }

        $offset += $size
        ++$chunkIndex
    }

    return $rows
}

$root = (Resolve-Path -LiteralPath $FfxiRoot).Path
$characterEntries = Get-CharacterEntries $CharacterTable
$animationSources = $characterEntries |
    Where-Object { $_.Label -eq "Skeleton + Base Tex" -or $_.Label -like "Animation set*" } |
    Sort-Object Race, Dat -Unique

$rows = New-Object System.Collections.Generic.List[object]
foreach ($entry in $animationSources) {
    $fullPath = Join-Path $root ($entry.Dat -replace '/', '\')
    $scanRows = Scan-AnimationDat $fullPath $entry.Dat
    foreach ($row in $scanRows) {
        $rows.Add([pscustomobject]@{
            Race = $entry.Race
            Source = $entry.Label
            Dat = $row.Dat
            Chunk = $row.Chunk
            ViewerName = $row.ViewerName
            ChunkIndex = $row.ChunkIndex
            Offset = $row.Offset
            Frames = $row.Frames
            Fps = $row.Fps
            Elements = $row.Elements
            Bones = $row.Bones
            Meaning = $row.Meaning
        })
    }
}

$rows | Export-Csv -LiteralPath $OutCsv -NoTypeInformation

$lines = New-Object System.Collections.Generic.List[string]
$lines.Add("# FFXI Character Animation Catalog")
$lines.Add("")
$lines.Add("Generated from ``DATura/character_dat_table.h`` by ``tools/scan_ffxi_animations.ps1``.")
$lines.Add("")
$lines.Add("The numbered animation names appear to be split-body pairs: suffix ``0`` is the root/lower-body half, and suffix ``1`` is the complementary upper/body half.")
$lines.Add("")

foreach ($raceGroup in ($rows | Group-Object Race)) {
    $lines.Add("## $($raceGroup.Name)")
    $lines.Add("")
    foreach ($datGroup in ($raceGroup.Group | Group-Object Dat)) {
        $source = ($datGroup.Group | Select-Object -First 1).Source
        $lines.Add("### $source - ``$($datGroup.Name)``")
        $lines.Add("")
        $lines.Add("| Chunk | Meaning | Frames | FPS | Elements | Bones |")
        $lines.Add("| --- | --- | ---: | ---: | ---: | --- |")
        foreach ($row in ($datGroup.Group | Sort-Object ChunkIndex)) {
            if ([string]::IsNullOrEmpty($row.Chunk)) {
                $lines.Add("| | $($row.Meaning) | | | | |")
            } else {
                $lines.Add("| ``$($row.Chunk)`` | $($row.Meaning) | $($row.Frames) | $($row.Fps) | $($row.Elements) | ``$($row.Bones)`` |")
            }
        }
        $lines.Add("")
    }
}

Set-Content -LiteralPath $OutMarkdown -Value $lines -Encoding UTF8
Write-Host "Wrote $OutMarkdown"
Write-Host "Wrote $OutCsv"
