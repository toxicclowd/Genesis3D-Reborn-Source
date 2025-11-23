# Fix C1041 PDB File Locking Error for AStudio Project
# This script adds the /FS compiler flag to prevent parallel compilation conflicts

Write-Host "================================================" -ForegroundColor Cyan
Write-Host "Fix C1041 PDB Error - AStudio Project" -ForegroundColor Cyan
Write-Host "================================================" -ForegroundColor Cyan
Write-Host ""

$projectFile = "C:\Users\James\Downloads\JStudio_Classic_11_2023_Source\JSTUDIO_CLASSIC11_2023\source\Tools\ActorTools\AStudio.vcxproj"
$backupFile = $projectFile + ".backup"

# Check if file exists
if (-not (Test-Path $projectFile)) {
    Write-Host "[ERROR] Project file not found:" -ForegroundColor Red
    Write-Host "  $projectFile" -ForegroundColor Yellow
    exit 1
}

Write-Host "[INFO] Project file found" -ForegroundColor Green
Write-Host "  Location: $projectFile" -ForegroundColor Gray
Write-Host ""

# Create backup
Write-Host "[BACKUP] Creating backup..." -ForegroundColor Yellow
try {
    Copy-Item $projectFile $backupFile -Force
    Write-Host "  Backup saved: $backupFile" -ForegroundColor Green
} catch {
    Write-Host "[ERROR] Failed to create backup: $_" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "[FIX] Adding /FS compiler flag..." -ForegroundColor Yellow

try {
    # Read the file content
    $content = Get-Content $projectFile -Raw
    
    # Check if /FS is already present
    if ($content -match '/FS') {
        Write-Host "[INFO] /FS flag already present in project file" -ForegroundColor Green
        Write-Host "  No changes needed!" -ForegroundColor Green
        Write-Host ""
        Write-Host "Press any key to exit..."
        $null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
        exit 0
    }
    
    # Find the Debug configuration ClCompile section
    $pattern = '(<ItemDefinitionGroup Condition="''\$\(Configuration\)\|\$\(Platform\)\)==''Debug\|Win32''">\s*<ClCompile>.*?<CompileAs>Default</CompileAs>)'
    
    if ($content -match $pattern) {
        # Add /FS flag after CompileAs
        $replacement = '$1' + "`n      <AdditionalOptions>/FS %(AdditionalOptions)</AdditionalOptions>"
        $newContent = $content -replace $pattern, $replacement
        
        # Write the modified content back
        Set-Content $projectFile $newContent -NoNewline
        
        Write-Host "[SUCCESS] /FS flag added successfully!" -ForegroundColor Green
        Write-Host ""
        Write-Host "Changes made:" -ForegroundColor Cyan
        Write-Host "  - Added: <AdditionalOptions>/FS %(AdditionalOptions)</AdditionalOptions>" -ForegroundColor White
        Write-Host "  - Location: Debug|Win32 configuration" -ForegroundColor White
        Write-Host ""
        Write-Host "[NEXT STEPS]" -ForegroundColor Yellow
        Write-Host "  1. Reload the AStudio project in Visual Studio" -ForegroundColor White
        Write-Host "  2. Rebuild the solution" -ForegroundColor White
        Write-Host "  3. The C1041 error should be resolved" -ForegroundColor White
        Write-Host ""
        Write-Host "[INFO] Original file backed up to:" -ForegroundColor Cyan
        Write-Host "  $backupFile" -ForegroundColor Gray
        
    } else {
        Write-Host "[WARNING] Could not find the expected Debug configuration section" -ForegroundColor Yellow
        Write-Host ""
        Write-Host "Manual fix required:" -ForegroundColor Yellow
        Write-Host "  1. Open AStudio.vcxproj in Visual Studio" -ForegroundColor White
        Write-Host "  2. Right-click project -> Properties" -ForegroundColor White
        Write-Host "  3. C/C++ -> Command Line" -ForegroundColor White
        Write-Host "  4. Add: /FS" -ForegroundColor White
        Write-Host "  5. Click OK and rebuild" -ForegroundColor White
    }
    
} catch {
    Write-Host "[ERROR] Failed to modify file: $_" -ForegroundColor Red
    Write-Host ""
    Write-Host "Restoring from backup..." -ForegroundColor Yellow
    Copy-Item $backupFile $projectFile -Force
    Write-Host "  Backup restored successfully" -ForegroundColor Green
    exit 1
}

Write-Host ""
Write-Host "================================================" -ForegroundColor Cyan
Write-Host "Script completed!" -ForegroundColor Green
Write-Host "================================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Press any key to exit..."
$null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
