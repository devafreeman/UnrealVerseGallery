<#
.SYNOPSIS
Copies the intro camera tour (camera spots, start times and settings) from the Unity scene
into GalleryData/gallery.json, converted to Unreal coordinates.

The Unity scene is only read, never written.

.EXAMPLE
powershell -ExecutionPolicy Bypass -File Tools\SyncIntroTourFromUnity.ps1
powershell -ExecutionPolicy Bypass -File Tools\SyncIntroTourFromUnity.ps1 -UnityScene "E:\The Verse Gallery\Assets\Scenes\my scene.unity"
#>
param(
    [string]$UnityScene = "D:\The Verse Gallery\Assets\Scenes\my scene.unity",
    [string]$GalleryJson = (Join-Path $PSScriptRoot "..\GalleryData\gallery.json")
)

$ErrorActionPreference = 'Stop'
$Inv = [Globalization.CultureInfo]::InvariantCulture

if (-not (Test-Path $UnityScene)) { throw "Unity scene not found: $UnityScene" }
if (-not (Test-Path $GalleryJson)) { throw "gallery.json not found: $GalleryJson" }

$assetsDir = $UnityScene.Substring(0, $UnityScene.LastIndexOf('\Assets\') + 7)
$tourMeta = Join-Path $assetsDir "Gallery\Scripts\GalleryIntroTour.cs.meta"
$tourGuid = [regex]::Match([IO.File]::ReadAllText($tourMeta), 'guid: (\w+)').Groups[1].Value

# ---------------------------------------------------------------- Unity YAML

$text = [IO.File]::ReadAllText($UnityScene)
$docs = @{}
foreach ($m in [regex]::Matches($text, '(?ms)^--- !u!(\d+) &(-?\d+)[^\n]*\n(.*?)(?=^--- !u!|\z)')) {
    $docs[$m.Groups[2].Value] = $m.Groups[3].Value
}

function Get-Num([string]$body, [string]$key, [double]$default = 0) {
    $m = [regex]::Match($body, "(?m)^\s*${key}:\s*(-?[\d.]+(?:[eE][-+]?\d+)?)\s*$")
    if ($m.Success) { return [double]::Parse($m.Groups[1].Value, $Inv) }
    return $default
}

function Get-Vec([string]$body, [string]$key) {
    $m = [regex]::Match($body, "${key}: \{x: (\S+), y: (\S+), z: (\S+?)(?:, w: (\S+?))?\}")
    $v = @([double]::Parse($m.Groups[1].Value, $Inv), [double]::Parse($m.Groups[2].Value, $Inv), [double]::Parse($m.Groups[3].Value, $Inv))
    if ($m.Groups[4].Success) { $v += [double]::Parse($m.Groups[4].Value, $Inv) }
    return , $v
}

# ---------------------------------------------------------------- math (Unity space)

function Mul-Quat($a, $b) {
    $ax, $ay, $az, $aw = $a; $bx, $by, $bz, $bw = $b
    return , @(
        ($aw * $bx + $ax * $bw + $ay * $bz - $az * $by),
        ($aw * $by - $ax * $bz + $ay * $bw + $az * $bx),
        ($aw * $bz + $ax * $by - $ay * $bx + $az * $bw),
        ($aw * $bw - $ax * $bx - $ay * $by - $az * $bz))
}

function Rotate-Vec($q, $v) {
    $qx, $qy, $qz, $qw = $q; $vx, $vy, $vz = $v
    $tx = 2 * ($qy * $vz - $qz * $vy)
    $ty = 2 * ($qz * $vx - $qx * $vz)
    $tz = 2 * ($qx * $vy - $qy * $vx)
    return , @(
        ($vx + $qw * $tx + ($qy * $tz - $qz * $ty)),
        ($vy + $qw * $ty + ($qz * $tx - $qx * $tz)),
        ($vz + $qw * $tz + ($qx * $ty - $qy * $tx)))
}

function Get-WorldTransform([string]$transformId) {
    $body = $docs[$transformId]
    if (-not $body) { throw "Transform $transformId not found in the scene" }
    $pos = Get-Vec $body 'm_LocalPosition'
    $rot = Get-Vec $body 'm_LocalRotation'
    $scale = Get-Vec $body 'm_LocalScale'
    $father = [regex]::Match($body, 'm_Father: \{fileID: (-?\d+)\}').Groups[1].Value
    if (-not $father -or $father -eq '0') { return @{ Pos = $pos; Rot = $rot; Scale = $scale } }

    $parent = Get-WorldTransform $father
    $scaled = @(($pos[0] * $parent.Scale[0]), ($pos[1] * $parent.Scale[1]), ($pos[2] * $parent.Scale[2]))
    $offset = Rotate-Vec $parent.Rot $scaled
    return @{
        Pos   = @(($parent.Pos[0] + $offset[0]), ($parent.Pos[1] + $offset[1]), ($parent.Pos[2] + $offset[2]))
        Rot   = (Mul-Quat $parent.Rot $rot)
        Scale = @(($parent.Scale[0] * $scale[0]), ($parent.Scale[1] * $scale[1]), ($parent.Scale[2] * $scale[2]))
    }
}

function F([double]$v, [string]$fmt = '0.###') { $v.ToString($fmt, $Inv) }

# ---------------------------------------------------------------- read the tour

$tourBody = $null
foreach ($body in $docs.Values) {
    if ($body -match "guid: $tourGuid" -and $body -match '(?m)^\s*shots:') { $tourBody = $body; break }
}
if (-not $tourBody) { throw "No GalleryIntroTour found in $UnityScene. Run Verse Gallery > Setup Intro Camera Tour in Unity first." }

$fov = 50
$camId = [regex]::Match($tourBody, 'tourCamera: \{fileID: (-?\d+)\}').Groups[1].Value
if ($camId -and $docs[$camId]) { $fov = Get-Num $docs[$camId] 'FieldOfView' 50 }

$shotJson = @()
$shotPattern = '(?m)^\s*- label: (.*?)\s*\r?\n\s*viewpoint: \{fileID: (-?\d+)\}\s*\r?\n\s*startTime: (\S+)'
foreach ($m in [regex]::Matches($tourBody, $shotPattern)) {
    $label = $m.Groups[1].Value.Trim().Trim('"', "'")
    $viewId = $m.Groups[2].Value
    $start = [double]::Parse($m.Groups[3].Value, $Inv)
    if ($viewId -eq '0') { Write-Warning "Shot '$label' has no camera spot; skipped."; continue }

    $world = Get-WorldTransform $viewId
    $p = $world.Pos
    $f = Rotate-Vec $world.Rot @(0, 0, 1)

    # Unity (x right, y up, z forward, meters) -> Unreal (x forward, y right, z up, centimeters)
    $ue = @(($p[2] * 100), ($p[0] * 100), ($p[1] * 100))
    $dir = @($f[2], $f[0], $f[1])
    $pitch = [Math]::Asin([Math]::Max(-1.0, [Math]::Min(1.0, [double]$dir[2]))) * 180 / [Math]::PI
    $yaw = [Math]::Atan2($dir[1], $dir[0]) * 180 / [Math]::PI

    $safeLabel = $label.Replace('\', '\\').Replace('"', '\"')
    $shotJson += '{"label":"' + $safeLabel + '","pos":[' + (F $ue[0] '0.##') + ',' + (F $ue[1] '0.##') + ',' + (F $ue[2] '0.##') + '],"pitch":' + (F $pitch) + ',"yaw":' + (F $yaw) + ',"start":' + (F $start) + '}'
    Write-Host ("{0,7}s  {1}" -f (F $start '0.0'), $label)
}
if ($shotJson.Count -eq 0) { throw "The Unity tour has no shots." }

$tourLine = '"introTour":{' +
    '"handBack":' + (F (Get-Num $tourBody 'handBackAtSeconds')) +
    ',"moveSeconds":' + (F (Get-Num $tourBody 'moveSeconds' 2.5)) +
    ',"pushInSpeed":' + (F ((Get-Num $tourBody 'pushInSpeed' 0.06) * 100)) +
    ',"fov":' + (F $fov) +
    ',"allowSkip":' + $(if ((Get-Num $tourBody 'allowSkip' 1) -ne 0) { 'true' } else { 'false' }) +
    ',"skipFadeSeconds":' + (F (Get-Num $tourBody 'skipFadeSeconds' 0.75)) +
    ',"maxWaitForVoice":' + (F (Get-Num $tourBody 'maxWaitForVoice' 6)) +
    ',"shots":[' + ($shotJson -join ',') + ']},'

# ---------------------------------------------------------------- write gallery.json

$jsonPath = (Resolve-Path $GalleryJson).Path
$lines = [IO.File]::ReadAllText($jsonPath) -split "`n"
$existing = [Array]::FindIndex($lines, [Predicate[string]] { param($l) $l.StartsWith('"introTour":') })
if ($existing -ge 0) {
    $lines[$existing] = $tourLine
}
else {
    $voice = [Array]::FindIndex($lines, [Predicate[string]] { param($l) $l.StartsWith('"voice":') })
    if ($voice -lt 0) { throw 'gallery.json has no "voice" line to insert the tour after.' }
    $list = [Collections.Generic.List[string]]::new([string[]]$lines)
    $list.Insert($voice + 1, $tourLine)
    $lines = $list.ToArray()
}
[IO.File]::WriteAllText($jsonPath, ($lines -join "`n"), [Text.UTF8Encoding]::new($false))
Write-Host "Updated $jsonPath with $($shotJson.Count) shots (hand back at $(F (Get-Num $tourBody 'handBackAtSeconds'))s)."
