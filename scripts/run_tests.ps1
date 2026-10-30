$ErrorActionPreference = 'Stop'
Set-Location (Join-Path $PSScriptRoot '..')
$python = Join-Path (Get-Location) '.venv\Scripts\python.exe'
if (-not (Test-Path $python)) {
    throw 'Python environment not found. Follow the installation instructions in README.md.'
}
& $python -m pytest tests -v --tb=short
exit $LASTEXITCODE
