param(
    [string]$ZigPath = "",
    [switch]$SkipTests,
    [string]$OutputPath = ""
)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$native = Join-Path $root 'native_c'
$work = Join-Path $root '.work\c-build'
$testDir = Join-Path $root '.work\c-tests'
$dist = Join-Path $native 'dist'
$env:ZIG_GLOBAL_CACHE_DIR = Join-Path $root '.work\zig-global-cache'

if ($ZigPath) {
    if (-not (Test-Path -LiteralPath $ZigPath -PathType Leaf)) { throw "Zig compiler not found: $ZigPath" }
    $zig = (Resolve-Path -LiteralPath $ZigPath).Path
} else {
    $command = Get-Command zig -ErrorAction SilentlyContinue
    if ($command) {
        $zig = $command.Source
    } else {
        $localZig = Join-Path $root '.work\tools\zig-0.17.0\zig.exe'
        if (-not (Test-Path -LiteralPath $localZig -PathType Leaf)) {
            throw 'Zig is required for the native C build (0.17.0 was tested). Install Zig or pass -ZigPath C:\path\to\zig.exe.'
        }
        $zig = $localZig
    }
}

New-Item -ItemType Directory -Force -Path $work, $testDir, $dist | Out-Null
$target = 'x86_64-windows-gnu'
$include = Join-Path $native 'src'

if (-not $SkipTests) {
    Write-Host 'Building localization catalog tests...'
    $i18nTest = Join-Path $testDir 'test_i18n.exe'
    & $zig cc -target $target -std=c11 -O2 -Wall -Wextra -I $include `
        (Join-Path $native 'src\i18n.c') (Join-Path $native 'tests\test_i18n.c') -o $i18nTest
    if ($LASTEXITCODE -ne 0) { throw 'Could not build native localization tests.' }
    & $i18nTest
    if ($LASTEXITCODE -ne 0) { throw 'Native localization tests failed.' }

    Write-Host 'Building offline parser tests...'
    $coreTest = Join-Path $testDir 'test_core.exe'
    & $zig cc -target $target -std=c11 -O2 -Wall -Wextra -Wno-macro-redefined -I $include `
        (Join-Path $native 'src\core.c') (Join-Path $native 'tests\test_core.c') -o $coreTest
    if ($LASTEXITCODE -ne 0) { throw 'Could not build native core tests.' }
    & $coreTest (Join-Path $root 'tests\fixtures\sample_rada_page.html')
    if ($LASTEXITCODE -ne 0) { throw 'Native parser tests failed.' }

    Write-Host 'Building settings/history integration tests...'
    $integrationTest = Join-Path $testDir 'test_settings_history.exe'
    & $zig cc -target $target -std=c11 -O2 -Wall -Wextra -Wno-macro-redefined -I $include `
        (Join-Path $native 'src\settings.c') (Join-Path $native 'src\history.c') `
        (Join-Path $native 'src\reader_store.c') `
        (Join-Path $native 'src\sqlite_api.c') (Join-Path $native 'tests\test_settings_history.c') `
        -lshell32 -o $integrationTest
    if ($LASTEXITCODE -ne 0) { throw 'Could not build settings/history integration tests.' }
    & $integrationTest
    if ($LASTEXITCODE -ne 0) { throw 'Settings/history integration tests failed.' }

    Write-Host 'Building local-document reader tests...'
    $localTest = Join-Path $testDir 'test_local_document.exe'
    & $zig cc -target $target -std=c11 -O2 -Wall -Wextra -Wno-macro-redefined -municode -I $include `
        (Join-Path $native 'src\core.c') (Join-Path $native 'src\local_document.c') `
        (Join-Path $native 'tests\test_local_document.c') -o $localTest
    if ($LASTEXITCODE -ne 0) { throw 'Could not build local-document tests.' }
    & $localTest
    if ($LASTEXITCODE -ne 0) { throw 'Local-document tests failed.' }

    Write-Host 'Building history CSV export tests...'
    $csvTest = Join-Path $testDir 'test_history_export.exe'
    & $zig cc -target $target -std=c11 -O2 -Wall -Wextra -Wno-macro-redefined -I $include `
        (Join-Path $native 'src\history_export.c') (Join-Path $native 'tests\test_history_export.c') -o $csvTest
    if ($LASTEXITCODE -ne 0) { throw 'Could not build history CSV export tests.' }
    & $csvTest
    if ($LASTEXITCODE -ne 0) { throw 'History CSV export tests failed.' }

    $pdfPrinter = Get-CimInstance Win32_Printer -Filter "Name='Microsoft Print to PDF'" -ErrorAction SilentlyContinue
    if ($pdfPrinter) {
        Write-Host 'Testing the installed Microsoft Print to PDF driver...'
        $pdfTest = Join-Path $testDir 'test_pdf_export.exe'
        $pdfTestFile = Join-Path $testDir 'pdf-render-test.pdf'
        & $zig cc -target $target -std=c11 -O2 -Wall -Wextra -Wno-macro-redefined -municode -I $include `
            (Join-Path $native 'src\pdf_export.c') (Join-Path $native 'tests\test_pdf_export.c') -lgdi32 -o $pdfTest
        if ($LASTEXITCODE -ne 0) { throw 'Could not build PDF-export tests.' }
        & $pdfTest $pdfTestFile
        if ($LASTEXITCODE -ne 0) { throw 'PDF-export driver test failed.' }
    } else {
        Write-Host 'Microsoft Print to PDF is unavailable; PDF driver test skipped. PDF export will report the missing driver at runtime.'
    }
}

Write-Host 'Compiling Windows icon resources...'
$resource = Join-Path $work 'app.res'
Push-Location $native
try {
    & $zig rc /nologo /fo $resource 'resources\app.rc'
    if ($LASTEXITCODE -ne 0) { throw 'Could not compile Windows icon resources.' }
} finally {
    Pop-Location
}

Write-Host 'Building optimized native Windows GUI...'
if ($OutputPath) {
    if ([IO.Path]::IsPathRooted($OutputPath)) { $exe = [IO.Path]::GetFullPath($OutputPath) }
    else { $exe = [IO.Path]::GetFullPath((Join-Path $root $OutputPath)) }
} else {
    $exe = Join-Path $dist 'ZakonDownloader.exe'
}
$outputDir = Split-Path -Parent $exe
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null
$runningTarget = Get-Process -Name ZakonDownloader -ErrorAction SilentlyContinue |
    Where-Object { $_.Path -and [IO.Path]::GetFullPath($_.Path) -ieq $exe }
if ($runningTarget) { throw "Refusing to overwrite the running application: $exe. Use -OutputPath .work\candidate\ZakonDownloader.exe instead." }
$sources = @(Get-ChildItem (Join-Path $native 'src') -Filter '*.c' | Sort-Object Name | ForEach-Object { $_.FullName })
& $zig cc -target $target -std=c11 -O2 -municode '-Wl,--subsystem,windows' -Wno-macro-redefined `
    -I $include @sources $resource -luser32 -lgdi32 -lcomctl32 -lshell32 -lole32 -lwinhttp -o $exe
if ($LASTEXITCODE -ne 0) { throw 'Native Windows GUI link failed.' }

$pdb = Join-Path $outputDir 'ZakonDownloader.pdb'
if (Test-Path -LiteralPath $pdb) { Remove-Item -LiteralPath $pdb -Force }

$item = Get-Item -LiteralPath $exe
Write-Host "Build complete: $($item.FullName)"
Write-Host "Size: $([math]::Round($item.Length / 1MB, 2)) MB"
