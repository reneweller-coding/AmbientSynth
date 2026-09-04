# Fetches what the Quest build needs into ThirdParty/ (ignored by git):
#   openxr-loader/  Khronos OpenXR loader for Android (prefab AAR from Maven Central: headers + libopenxr_loader.so)
#   oboe/           Google Oboe (low-latency Android audio), built from source by CMake
#   meta-openxr-sdk/ Meta's OpenXR SDK samples (reference only, optional)
param([switch]$WithMetaSamples)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$third = Join-Path $root "ThirdParty"
New-Item -ItemType Directory -Force $third | Out-Null

$loaderDir = Join-Path $third "openxr-loader"
if (-not (Test-Path (Join-Path $loaderDir "prefab"))) {
    $meta = Invoke-WebRequest -UseBasicParsing "https://repo1.maven.org/maven2/org/khronos/openxr/openxr_loader_for_android/maven-metadata.xml"
    $version = ([xml]$meta.Content).metadata.versioning.release
    $aar = Join-Path $third "openxr_loader_for_android.zip"
    Invoke-WebRequest -UseBasicParsing "https://repo1.maven.org/maven2/org/khronos/openxr/openxr_loader_for_android/$version/openxr_loader_for_android-$version.aar" -OutFile $aar
    New-Item -ItemType Directory -Force $loaderDir | Out-Null
    Expand-Archive -Path $aar -DestinationPath $loaderDir -Force
    Remove-Item $aar
    Write-Host "OpenXR loader $version"
}

$oboe = Join-Path $third "oboe"
if (-not (Test-Path (Join-Path $oboe "CMakeLists.txt"))) {
    & git clone --depth 1 https://github.com/google/oboe.git $oboe
    if ($LASTEXITCODE -ne 0) { throw "git clone oboe failed" }
}

if ($WithMetaSamples) {
    $meta = Join-Path $third "meta-openxr-sdk"
    if (-not (Test-Path $meta)) {
        $zip = Join-Path $third "meta-openxr-sdk.zip"
        & gh release download --repo meta-quest/Meta-OpenXR-SDK --pattern meta-openxr-sdk.zip --output $zip
        if ($LASTEXITCODE -ne 0) { throw "gh release download failed (is gh logged in?)" }
        Expand-Archive -Path $zip -DestinationPath $meta -Force
        Remove-Item $zip
    }
}
Write-Host "ThirdParty ready: $third"
