# DirectX 12 Driver - Build and Test Script
# Run this after building to verify the driver

Write-Host "============================================" -ForegroundColor Cyan
Write-Host "DirectX 12 Driver - Build Verification" -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan
Write-Host ""

$SolutionDir = "C:\Users\James\Downloads\JStudio_Classic_11_2023_Source\JSTUDIO_CLASSIC11_2023"
$BinDir = "$SolutionDir\bin"
$DllPath = "$BinDir\Direct3D12Driver.dll"

# Check if DLL exists
if (Test-Path $DllPath) {
    Write-Host "[OK] Direct3D12Driver.dll found" -ForegroundColor Green
    
    # Get file info
    $dll = Get-Item $DllPath
    Write-Host "    Location: $($dll.FullName)" -ForegroundColor Gray
    Write-Host "    Size: $($dll.Length) bytes" -ForegroundColor Gray
    Write-Host "    Modified: $($dll.LastWriteTime)" -ForegroundColor Gray
    Write-Host ""
    
    # Check dependencies
    Write-Host "[CHECK] Verifying dependencies..." -ForegroundColor Yellow
    
    $requiredDlls = @("d3d12.dll", "dxgi.dll", "d3dcompiler_47.dll")
    $systemPath = "$env:SystemRoot\System32"
    
    foreach ($required in $requiredDlls) {
        if (Test-Path "$systemPath\$required") {
            Write-Host "    [OK] $required found" -ForegroundColor Green
        } else {
            Write-Host "    [WARN] $required not found" -ForegroundColor Red
        }
    }
    Write-Host ""
    
    # Check for log file (if driver was run)
    $logPath = "$BinDir\Direct3D12Driver.log"
    if (Test-Path $logPath) {
        Write-Host "[INFO] Log file exists - showing last 10 lines:" -ForegroundColor Cyan
        Get-Content $logPath -Tail 10 | ForEach-Object {
            Write-Host "    $_" -ForegroundColor Gray
        }
        Write-Host ""
    }
    
    # Success message
    Write-Host "============================================" -ForegroundColor Cyan
    Write-Host "[SUCCESS] Driver built successfully!" -ForegroundColor Green
    Write-Host "============================================" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "Next Steps:" -ForegroundColor Yellow
    Write-Host "1. Run a Jet3D application (e.g., jMinApp.exe)" -ForegroundColor White
    Write-Host "2. Select 'DirectX 12 Driver' from the driver list" -ForegroundColor White
    Write-Host "3. You should see a dark blue clear screen" -ForegroundColor White
    Write-Host "4. Check Direct3D12Driver.log for details" -ForegroundColor White
    Write-Host ""
    
} else {
    Write-Host "[ERROR] Direct3D12Driver.dll not found!" -ForegroundColor Red
    Write-Host "Expected location: $DllPath" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "Build the project first:" -ForegroundColor Yellow
    Write-Host "1. Open Visual Studio" -ForegroundColor White
    Write-Host "2. Load the JStudio solution" -ForegroundColor White
    Write-Host "3. Build the Direct3D12Driver project" -ForegroundColor White
    Write-Host ""
}

Write-Host "============================================" -ForegroundColor Cyan
