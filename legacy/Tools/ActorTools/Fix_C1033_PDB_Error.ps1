# Fix C1033 PDB Error - Nuclear Option
# Run this if cleaning/rebuilding doesn't work

Write-Host "================================================" -ForegroundColor Cyan
Write-Host "C1033 PDB Error - Nuclear Fix" -ForegroundColor Cyan
Write-Host "================================================" -ForegroundColor Cyan
Write-Host ""

$debugFolder = "C:\Users\James\Downloads\JStudio_Classic_11_2023_Source\JSTUDIO_CLASSIC11_2023\source\Tools\ActorTools\Debug"

Write-Host "[1] Checking for locked files..." -ForegroundColor Yellow

# Try to delete all PDB files
Get-ChildItem $debugFolder -Filter "*.pdb" -ErrorAction SilentlyContinue | ForEach-Object {
    try {
        Remove-Item $_.FullName -Force
        Write-Host "  ? Deleted: $($_.Name)" -ForegroundColor Green
    } catch {
        Write-Host "  ?? Locked: $($_.Name)" -ForegroundColor Yellow
    }
}

# Try to delete all IDB files (incremental linker database)
Get-ChildItem $debugFolder -Filter "*.idb" -ErrorAction SilentlyContinue | ForEach-Object {
    try {
        Remove-Item $_.FullName -Force
        Write-Host "  ? Deleted: $($_.Name)" -ForegroundColor Green
    } catch {
        Write-Host "  ?? Locked: $($_.Name)" -ForegroundColor Yellow
    }
}

Write-Host ""
Write-Host "[2] Checking processes..." -ForegroundColor Yellow

$processes = Get-Process | Where-Object { $_.ProcessName -match "devenv|msbuild|cl|link" }
if ($processes) {
    Write-Host "  Found Visual Studio processes:" -ForegroundColor Red
    $processes | ForEach-Object { Write-Host "    - $($_.ProcessName) (PID: $($_.Id))" -ForegroundColor Gray }
    Write-Host ""
    Write-Host "  ?? CLOSE VISUAL STUDIO COMPLETELY!" -ForegroundColor Red
    Write-Host "  Then run this script again." -ForegroundColor Yellow
} else {
    Write-Host "  ? No Visual Studio processes running" -ForegroundColor Green
}

Write-Host ""
Write-Host "================================================" -ForegroundColor Cyan
Write-Host "Next Steps:" -ForegroundColor Yellow
Write-Host "  1. Close ALL Visual Studio windows" -ForegroundColor White
Write-Host "  2. Run this script again to verify cleanup" -ForegroundColor White
Write-Host "  3. Reopen Visual Studio" -ForegroundColor White
Write-Host "  4. Build ? Clean Solution" -ForegroundColor White
Write-Host "  5. Build ? Rebuild Solution" -ForegroundColor White
Write-Host "================================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Press any key to exit..."
$null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
