# AmbientSynth -- build the binaries other people get, and wrap them in a setup.
#
#   powershell -File Deploy\build_release.ps1 [-Version 1.0.0] [-SkipBuild] [-NoSetup]
#
# Three things make this build different from an everyday one:
#
#   * the MSVC runtime is linked in (AMBIENT_STATIC_RUNTIME), so nothing has to be installed
#     first -- no redistributable, no DLL beside the executable;
#   * AVX2 is on (39x realtime without it, 52x with it, both measured). Every x86-64 processor
#     since 2013 has it, which is every machine anybody makes music on; the setup checks for it
#     before installing rather than letting an old one fail with an illegal instruction;
#   * it builds in its own folder, so the everyday build tree is left alone.
#
# The result is Deploy\out\AmbientSynth-<version>-Setup.exe plus Deploy\out\AmbientSynth-<version>-portable.zip
# for people who would rather not run an installer at all.
param(
    [string]$Version = "",
    [switch]$SkipBuild,       # reuse whatever is in build-release already
    [switch]$NoSetup,         # stage and zip, but do not call the Inno compiler
    [switch]$SkipManual       # reuse the manual already in docs/manual
)
$python = "Tools\TextureGen\.venv\Scripts\python.exe"
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
Set-Location $root

if (-not $Version) {
    $m = Select-String -Path (Join-Path $root "CMakeLists.txt") -Pattern 'project\(AmbientSynth VERSION ([0-9.]+)'
    if (-not $m) { throw "no version in CMakeLists.txt and none given" }
    $Version = $m.Matches[0].Groups[1].Value
}
Write-Host "AmbientSynth $Version" -ForegroundColor Cyan

$buildDir = Join-Path $root "build-release"
$stage = Join-Path $root "Deploy\stage"
$out = Join-Path $root "Deploy\out"

if (-not $SkipBuild) {
    # A build for other people is not worth having in a hurry: -j 2 leaves the machine usable.
    cmake -S . -B $buildDir -G "Visual Studio 18 2026" -A x64 `
        -DAMBIENT_STATIC_RUNTIME=ON -DAMBIENT_AVX2=ON -DAMBIENT_BUILD_TOOLS=ON `
        "-DFETCHCONTENT_SOURCE_DIR_JUCE=$root/build/_deps/juce-src"   # the JUCE already fetched
    if ($LASTEXITCODE -ne 0) { throw "configure failed" }
    cmake --build $buildDir --config Release --parallel 2
    if ($LASTEXITCODE -ne 0) { throw "build failed" }

    # The tests are built in the same configuration that ships, and have to pass in it: a static
    # runtime and a missing AVX2 are exactly the kind of change that is fine until it is not.
    & (Join-Path $buildDir "Tests\Release\ambient_selftest.exe")
    if ($LASTEXITCODE -ne 0) { throw "self test failed in the release configuration" }
    & (Join-Path $buildDir "Tests\Release\ambient_hosttest.exe")
    if ($LASTEXITCODE -ne 0) { throw "host test failed in the release configuration" }
}

$art = Join-Path $buildDir "Plugin\AmbientSynth_artefacts\Release"
$exe = Join-Path $art "Standalone\AmbientSynth.exe"
$vst = Join-Path $art "VST3\AmbientSynth.vst3"
foreach ($p in @($exe, $vst)) { if (-not (Test-Path $p)) { throw "missing build output: $p" } }

# ---------------------------------------------------------------- what must not be there
# A binary that still wants the Visual C++ runtime would fail on a machine without it, and the
# failure looks like "the app just does not start". Checked here rather than discovered later.
$dumpbin = Get-ChildItem "C:\Program Files\Microsoft Visual Studio\*\*\VC\Tools\MSVC\*\bin\Hostx64\x64\dumpbin.exe" -ErrorAction SilentlyContinue |
           Select-Object -First 1 -ExpandProperty FullName
if ($dumpbin) {
    foreach ($bin in @($exe, (Join-Path $vst "Contents\x86_64-win\AmbientSynth.vst3"))) {
        $deps = & $dumpbin /dependents $bin | Select-String -Pattern '^\s+\S+\.dll' | ForEach-Object { $_.Line.Trim() }
        $bad = $deps | Where-Object { $_ -match '^(VCRUNTIME|MSVCP|CONCRT|api-ms-win-crt)' }
        if ($bad) { throw "$([System.IO.Path]::GetFileName($bin)) still needs the Visual C++ runtime: $($bad -join ', ')" }
        Write-Host ("  {0}: {1} system DLLs, none of them a redistributable" -f [System.IO.Path]::GetFileName($bin), $deps.Count)
    }
} else {
    Write-Warning "dumpbin not found -- the runtime check was skipped"
}

# ---------------------------------------------------------------- the manual
# Made from a running instrument: its pictures are snapshots of the real panel, so they cannot go
# out of date the way a drawing would. AMBIENT_PRESET picks a patch with all three sources and the
# effects in use, or the Sources chapter would be illustrated with a greyed-out section.
$manualDir = Join-Path $root "docs\manual"
if (-not $SkipManual) {
    # Patiently: a browser that has just finished printing can still hold the PDF for a moment,
    # and losing a whole release build to that would be silly.
    for ($i = 0; $i -lt 10 -and (Test-Path $manualDir); $i++) {
        Remove-Item $manualDir -Recurse -Force -ErrorAction SilentlyContinue
        if (Test-Path $manualDir) { Start-Sleep -Milliseconds 700 }
    }
    if (Test-Path $manualDir) { throw "cannot clear $manualDir -- something still has a file open" }
    $env:AMBIENT_PRESET = "Three Voices, One Key"
    $env:AMBIENT_MANUAL = $manualDir
    # A clip for the manual's gallery of source types: the Texture and Stretch pictures show it
    # loaded and playing. Any seamless field recording will do; the first one alphabetically is
    # the same one every time.
    $clip = Get-ChildItem (Join-Path $root "Library\Textures") -Filter "field_recordings_*_loop.wav" -ErrorAction SilentlyContinue | Sort-Object Name | Select-Object -First 1
    if ($clip) { $env:AMBIENT_MANUAL_CLIP = $clip.FullName }
    # And the library: the browser and the map are photographed too, and without the packs they
    # show the 196 built-in presets of an instrument that ships with 6400.
    $env:AMBIENT_PACKS = Join-Path $root "Library\Packs"
    $mp = Start-Process $exe -PassThru
    if (-not $mp.WaitForExit(90000)) { $mp.Kill() ; throw "the manual export did not finish" }
    Remove-Item env:AMBIENT_MANUAL, env:AMBIENT_PRESET, env:AMBIENT_MANUAL_CLIP, env:AMBIENT_PACKS -ErrorAction SilentlyContinue
    & $python (Join-Path $root "Tools\make_manual.py")
}
$manualPdf = Join-Path $manualDir "AmbientSynth-Manual.pdf"

# ---------------------------------------------------------------- stage
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
New-Item -ItemType Directory -Force -Path $stage, $out | Out-Null
Copy-Item $exe $stage
Copy-Item $vst (Join-Path $stage "AmbientSynth.vst3") -Recurse
Copy-Item (Join-Path $root "docs\logo.ico") $stage
if (Test-Path $manualPdf) { Copy-Item $manualPdf $stage } else { Write-Warning "no manual PDF to ship" }
Copy-Item (Join-Path $root "LICENSE") (Join-Path $stage "LICENSE.txt")
New-Item -ItemType Directory -Force -Path (Join-Path $stage "Packs") | Out-Null
Copy-Item (Join-Path $root "Library\Packs\*.ambientpack") (Join-Path $stage "Packs")
$packCount = (Get-ChildItem (Join-Path $stage "Packs") -Filter *.ambientpack).Count
if ($packCount -lt 1) { throw "no preset packs staged" }

@"
AmbientSynth $Version
=====================

A drone and ambient synthesiser: three source slots, two filters, a resonating body, delays, a
convolution room, a far reverb, the Cosmos feedback network, and a conductor that plays it.

WHAT IS HERE

  AmbientSynth.exe        the standalone instrument. Nothing else needs to be installed.
  AmbientSynth.vst3       the plug-in. Copy the whole folder to
                          C:\Program Files\Common Files\VST3\ and rescan in your DAW.
  AmbientSynth-Manual.pdf the manual, the same one the Help page shows.
  Packs\                  $packCount preset packs. Copy them to
                          C:\ProgramData\AmbientSynth\Packs (for everyone on the machine) or
                          Documents\AmbientSynth\Packs (just for you). The instrument reads both.

The setup does all of that for you; this archive is for anyone who would rather it did not.

FIRST RUN

Start it and wait: the conductor begins a piece within a few seconds. Point at any control to
read what it does; Help (or F1) opens the manual. The standalone comes back the way you left it
-- the Recall button in the header turns that off.

The preset library that the packs name (samples, wavetables and impulse responses) is a separate
download; presets that cannot find their sample fall back to the built-in sources and still play.

$(Get-Content (Join-Path $root "LICENSE") -TotalCount 1)
"@ | Set-Content (Join-Path $stage "README.txt") -Encoding utf8

# ---------------------------------------------------------------- portable archive
$zip = Join-Path $out "AmbientSynth-$Version-portable.zip"
if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path (Join-Path $stage "*") -DestinationPath $zip -CompressionLevel Optimal
Write-Host ("  portable zip: {0:N1} MB" -f ((Get-Item $zip).Length / 1MB))

# ---------------------------------------------------------------- setup
if (-not $NoSetup) {
    $iscc = Get-ChildItem "C:\Program Files\Inno Setup *\ISCC.exe", "C:\Program Files (x86)\Inno Setup *\ISCC.exe" -ErrorAction SilentlyContinue |
            Select-Object -First 1 -ExpandProperty FullName
    if (-not $iscc) { throw "Inno Setup not found. winget install JRSoftware.InnoSetup, or run with -NoSetup." }
    & $iscc "/DVersion=$Version" (Join-Path $root "Deploy\AmbientSynth.iss")
    if ($LASTEXITCODE -ne 0) { throw "the installer failed to build" }
    $setup = Join-Path $out "AmbientSynth-$Version-Setup.exe"
    Write-Host ("  setup: {0:N1} MB" -f ((Get-Item $setup).Length / 1MB)) -ForegroundColor Green
}
Get-ChildItem $out | Format-Table Name, @{n="MB";e={"{0:N1}" -f ($_.Length/1MB)}}, LastWriteTime
