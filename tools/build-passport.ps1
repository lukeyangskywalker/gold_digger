param([string]$IdfPath = $env:IDF_PATH)
# Builds the Passport firmware and the merged image. Never flashes.
$ErrorActionPreference = 'Stop'
if (-not $IdfPath) { throw 'Activate ESP-IDF 5.5.3 or pass -IdfPath.' }
$root = Split-Path -Parent $PSScriptRoot
Push-Location $root
try {
    $version = & python "$IdfPath/tools/idf.py" --version
    if ($version -notmatch 'v5\.5\.3\b') { throw "Expected ESP-IDF 5.5.3, got $version" }
    & python "$IdfPath/tools/idf.py" -B build/passport build
    if ($LASTEXITCODE) { throw 'Firmware build failed' }
    & python "$IdfPath/tools/idf.py" -B build/passport merge-bin -o GoldDigger-full.bin
    if ($LASTEXITCODE) { throw 'Merge failed' }
    Get-Item build/passport/GoldDigger.bin, build/passport/GoldDigger-full.bin | Select-Object Name, Length
} finally { Pop-Location }
