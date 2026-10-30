$ErrorActionPreference = 'Stop'
Set-Location (Join-Path $PSScriptRoot '..')
$python = Join-Path (Get-Location) '.venv\Scripts\python.exe'
if (-not (Test-Path $python)) {
    throw 'Python environment not found. Follow the installation instructions in README.md.'
}
& $python -m PyInstaller build_exe.spec --clean --noconfirm
exit $LASTEXITCODE
