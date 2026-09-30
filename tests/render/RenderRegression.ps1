<#
.SYNOPSIS
    Render regression for Genesis3D: Reborn (Modernization Roadmap, Phase 0).

.DESCRIPTION
    Renders every shot in shots.txt with G3DGameShell's -screenshot mode and compares
    each image against the baseline. Run it with -Update once on a known-good build to
    record the baseline, then run it without -Update after a renderer change.

    Images go to tests\render\out\ (and baseline\ with -Update); both are ignored by git
    because the baseline depends on the GPU and driver it was captured on.

.EXAMPLE
    .\tests\render\RenderRegression.ps1 -Update          # record the baseline
    .\tests\render\RenderRegression.ps1                  # compare against it
    .\tests\render\RenderRegression.ps1 -Only tutorial2  # one shot
#>
param(
    [ValidateSet("Debug", "Release")][string]$Config = "Debug",
    [switch]$Update,
    [string]$Only = "",
    # A pixel counts as different when any channel differs by more than this.
    [int]$Tolerance = 2,
    # A shot fails when more than this percentage of its pixels differ.
    [double]$MaxDiffPercent = 0.05,
    [int]$TimeoutSec = 300
)

$ErrorActionPreference = "Stop"
$Here = Split-Path -Parent $MyInvocation.MyCommand.Path
$Root = Resolve-Path (Join-Path $Here "..\..")
$Bin = Join-Path $Root "bin"
$Exe = Join-Path $Bin ($(if ($Config -eq "Debug") { "G3DGameShelld.exe" } else { "G3DGameShell.exe" }))
$OutDir = Join-Path $Here "out"
$BaseDir = Join-Path $Here "baseline"

if (-not (Test-Path $Exe)) { throw "Not built: $Exe" }
New-Item -ItemType Directory -Force $OutDir | Out-Null
if ($Update) { New-Item -ItemType Directory -Force $BaseDir | Out-Null }

Add-Type -AssemblyName System.Drawing
# Only plain types cross into the compiled helper: System.Drawing's assembly layout differs
# between Windows PowerShell and PowerShell 7, so all imaging stays in script.
Add-Type @"
using System;
using System.Text;
using System.Runtime.InteropServices;

public static class G3DRegress
{
    public delegate bool EnumProc(IntPtr h, IntPtr l);
    [DllImport("user32.dll")] static extern bool EnumWindows(EnumProc p, IntPtr l);
    [DllImport("user32.dll")] static extern bool EnumChildWindows(IntPtr h, EnumProc p, IntPtr l);
    [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
    [DllImport("user32.dll", CharSet = CharSet.Ansi)] static extern int GetClassName(IntPtr h, StringBuilder s, int n);
    [DllImport("user32.dll", CharSet = CharSet.Ansi)] static extern int GetWindowText(IntPtr h, StringBuilder s, int n);

    // Text of any message box / assert dialog the process is showing, or null.
    public static string DialogText(uint pid)
    {
        string found = null;
        EnumWindows((h, l) => {
            uint p; GetWindowThreadProcessId(h, out p);
            var cls = new StringBuilder(64); GetClassName(h, cls, 64);
            if (p == pid && cls.ToString() == "#32770")
            {
                var sb = new StringBuilder();
                EnumChildWindows(h, (c, l2) => {
                    var t = new StringBuilder(2048); GetWindowText(c, t, 2048);
                    if (t.Length > 0) sb.Append(t.ToString()).Append(" | ");
                    return true; }, IntPtr.Zero);
                found = sb.ToString();
            }
            return true; }, IntPtr.Zero);
        return found;
    }

    // Counts pixels where any channel differs by more than the tolerance, and fills
    // 'diff' with red for those pixels over a dimmed grey copy of the baseline.
    public static long Compare(int[] a, int[] b, int tolerance, int[] diff)
    {
        long n = 0;
        for (int i = 0; i < a.Length; i++)
        {
            int x = a[i], y = b[i];
            int dr = Math.Abs(((x >> 16) & 255) - ((y >> 16) & 255));
            int dg = Math.Abs(((x >> 8) & 255) - ((y >> 8) & 255));
            int db = Math.Abs((x & 255) - (y & 255));
            if (dr > tolerance || dg > tolerance || db > tolerance)
            {
                n++;
                diff[i] = unchecked((int)0xFFFF0000);
            }
            else
            {
                int g = (((x >> 16) & 255) + ((x >> 8) & 255) + (x & 255)) / 12;
                diff[i] = unchecked((int)0xFF000000) | (g << 16) | (g << 8) | g;
            }
        }
        return n;
    }
}
"@

$Format32 = [System.Drawing.Imaging.PixelFormat]::Format32bppRgb

function Read-Pixels([string]$Path) {
    $bmp = New-Object System.Drawing.Bitmap $Path
    try {
        $rect = New-Object System.Drawing.Rectangle 0, 0, $bmp.Width, $bmp.Height
        $data = $bmp.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::ReadOnly, $Format32)
        $px = New-Object int[] ($bmp.Width * $bmp.Height)
        for ($y = 0; $y -lt $bmp.Height; $y++) {
            [System.Runtime.InteropServices.Marshal]::Copy([IntPtr]::Add($data.Scan0, $y * $data.Stride), $px, $y * $bmp.Width, $bmp.Width)
        }
        $bmp.UnlockBits($data)
        return [pscustomobject]@{ Width = $bmp.Width; Height = $bmp.Height; Pixels = $px }
    }
    finally { $bmp.Dispose() }
}

function Write-Pixels([int[]]$Pixels, [int]$Width, [int]$Height, [string]$Path) {
    $bmp = New-Object System.Drawing.Bitmap $Width, $Height, $Format32
    try {
        $rect = New-Object System.Drawing.Rectangle 0, 0, $Width, $Height
        $data = $bmp.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::WriteOnly, $Format32)
        for ($y = 0; $y -lt $Height; $y++) {
            [System.Runtime.InteropServices.Marshal]::Copy($Pixels, $y * $Width, [IntPtr]::Add($data.Scan0, $y * $data.Stride), $Width)
        }
        $bmp.UnlockBits($data)
        $bmp.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png)
    }
    finally { $bmp.Dispose() }
}

# Returns the number of differing pixels, or -1 when the sizes differ.
function Compare-Images([string]$Baseline, [string]$Current, [string]$DiffPath) {
    $a = Read-Pixels $Baseline
    $b = Read-Pixels $Current
    if ($a.Width -ne $b.Width -or $a.Height -ne $b.Height) { return -1 }
    $diff = New-Object int[] $a.Pixels.Length
    $n = [G3DRegress]::Compare($a.Pixels, $b.Pixels, $Tolerance, $diff)
    if ($n -gt 0) { Write-Pixels $diff $a.Width $a.Height $DiffPath }
    return $n
}

function Invoke-Shot($Shot) {
    $bmp = Join-Path $OutDir "$($Shot.Name).bmp"
    $png = Join-Path $OutDir "$($Shot.Name).png"
    Remove-Item $bmp, $png -ErrorAction SilentlyContinue

    $shellArgs = @("-screenshot", "`"$bmp`"", "-size", $Shot.Width, $Shot.Height, "-frames", $Shot.Frames)
    if ($Shot.Level) { $shellArgs += @("-level", "`"$($Shot.Level)`"") }
    if ($Shot.Camera) { $shellArgs += @("-camera") + $Shot.Camera }

    # Extra column: NAME=value sets an environment variable for this shot, anything else
    # is passed to the shell (e.g. -dlight).
    $saved = @{}
    foreach ($a in $Shot.Extra) {
        if ($a -match '^([A-Za-z_][A-Za-z0-9_]*)=(.*)$') {
            $saved[$Matches[1]] = [Environment]::GetEnvironmentVariable($Matches[1])
            [Environment]::SetEnvironmentVariable($Matches[1], $Matches[2])
        } else {
            $shellArgs += $a
        }
    }
    try {
        $proc = Start-Process -FilePath $Exe -ArgumentList $shellArgs -WorkingDirectory $Bin -PassThru
    } finally {
        foreach ($k in $saved.Keys) { [Environment]::SetEnvironmentVariable($k, $saved[$k]) }
    }
    $deadline = (Get-Date).AddSeconds($TimeoutSec)
    while (-not $proc.HasExited) {
        Start-Sleep -Milliseconds 500
        $dialog = [G3DRegress]::DialogText([uint32]$proc.Id)
        if ($dialog) { Stop-Process -Id $proc.Id -Force; return "dialog: $dialog" }
        if ((Get-Date) -gt $deadline) { Stop-Process -Id $proc.Id -Force; return "timed out after $TimeoutSec s" }
        $proc.Refresh()
    }
    if ($proc.ExitCode -ne 0) { return "exit code $($proc.ExitCode)" }
    if (-not (Test-Path $bmp)) { return "no screenshot written" }

    $img = New-Object System.Drawing.Bitmap $bmp
    $img.Save($png, [System.Drawing.Imaging.ImageFormat]::Png)
    $img.Dispose()
    Remove-Item $bmp
    return $null
}

# shots.txt: name | level (blank = the startup script's level) | camera "x y z yaw pitch" (blank = origin)
#            | extra (optional): shell arguments and NAME=value environment variables
$shots = foreach ($line in Get-Content (Join-Path $Here "shots.txt")) {
    $line = $line.Trim()
    if (-not $line -or $line.StartsWith("#")) { continue }
    $f = $line.Split("|") | ForEach-Object { $_.Trim() }
    [pscustomobject]@{
        Name = $f[0]
        Level = $(if ($f.Count -gt 1 -and $f[1]) { $f[1] } else { $null })
        Camera = $(if ($f.Count -gt 2 -and $f[2]) { $f[2] -split "\s+" } else { $null })
        Extra = $(if ($f.Count -gt 3 -and $f[3]) { $f[3] -split "\s+" } else { @() })
        Width = 640; Height = 480; Frames = 30
    }
}
if ($Only) { $shots = $shots | Where-Object { $_.Name -like "*$Only*" } }

# The PBR shots use generated sample materials (tests/render/pbr/make_pbr_sample.py).
if (($shots | Where-Object { $_.Extra -match "PBRSample" }) -and
    -not (Test-Path (Join-Path $Bin "GlobalMaterials\PBRSample"))) {
    & python (Join-Path $Here "pbr\make_pbr_sample.py")
    if ($LASTEXITCODE -ne 0) { throw "make_pbr_sample.py failed (needs Python with Pillow and NumPy)" }
}

$failed = 0
foreach ($shot in $shots) {
    $started = Get-Date
    $err = Invoke-Shot $shot
    $secs = [int]((Get-Date) - $started).TotalSeconds
    $png = Join-Path $OutDir "$($shot.Name).png"
    $base = Join-Path $BaseDir "$($shot.Name).png"

    if ($err) {
        Write-Host ("FAIL  {0,-24} {1}" -f $shot.Name, $err) -ForegroundColor Red
        $failed++
    }
    elseif ($Update) {
        Copy-Item $png $base -Force
        Write-Host ("SAVED {0,-24} baseline ({1}s)" -f $shot.Name, $secs)
    }
    elseif (-not (Test-Path $base)) {
        Write-Host ("NEW   {0,-24} no baseline; run with -Update" -f $shot.Name) -ForegroundColor Yellow
    }
    else {
        $diffPath = Join-Path $OutDir "$($shot.Name).diff.png"
        Remove-Item $diffPath -ErrorAction SilentlyContinue
        $n = Compare-Images $base $png $diffPath
        $pct = if ($n -lt 0) { 100.0 } else { 100.0 * $n / ($shot.Width * $shot.Height) }
        if ($n -lt 0 -or $pct -gt $MaxDiffPercent) {
            Write-Host ("FAIL  {0,-24} {1:N3}% pixels differ, see {2}" -f $shot.Name, $pct, $diffPath) -ForegroundColor Red
            $failed++
        }
        else {
            Write-Host ("PASS  {0,-24} {1:N3}% pixels differ ({2}s)" -f $shot.Name, $pct, $secs) -ForegroundColor Green
        }
    }
}

Write-Host ""
Write-Host "$(@($shots).Count - $failed) of $(@($shots).Count) shots passed."
exit $(if ($failed) { 1 } else { 0 })
