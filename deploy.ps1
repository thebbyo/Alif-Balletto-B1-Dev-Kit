param(
    [Parameter(Mandatory=$true, HelpMessage="The path to the project you want to build and flash (e.g. C:\alif-workspace\blink)")]
    [string]$ProjectDir,

    [string]$ComPort = "COM3"
)

# Convert relative paths to absolute
$ProjectDir = (Resolve-Path $ProjectDir).Path

# Define critical paths
$Workspace = "C:\alif-workspace"
$ZephyrDir = "$Workspace\zephyrproject\zephyr"
$SetoolsDir = "$Workspace\alif_setools\app-release-exec"
$PadScript = "$Workspace\blink\scripts\pad_firmware.py"

if (-Not (Test-Path $ProjectDir)) {
    Write-Host "[!] Error: Project folder not found: $ProjectDir" -ForegroundColor Red
    exit 1
}

Write-Host "======================================================" -ForegroundColor Cyan
Write-Host " 1. BUILDING PROJECT: $ProjectDir" -ForegroundColor Cyan
Write-Host "======================================================" -ForegroundColor Cyan

Set-Location $ZephyrDir
west build -p always -b balletto_b1_dk/ab1c1f4m51820ph0 "$ProjectDir" -d "$ProjectDir\build"
if ($LASTEXITCODE -ne 0) { Write-Host "[!] Build Failed!" -ForegroundColor Red; exit 1 }

Write-Host "`n======================================================" -ForegroundColor Cyan
Write-Host " 2. PADDING FIRMWARE " -ForegroundColor Cyan
Write-Host "======================================================" -ForegroundColor Cyan

python $PadScript "$ProjectDir\build\zephyr\zephyr.bin"
if ($LASTEXITCODE -ne 0) { Write-Host "[!] Padding Failed!" -ForegroundColor Red; exit 1 }

# Copy padded binary to the Security Toolkit using the generic 'he_app.bin' name
Copy-Item "$ProjectDir\build\zephyr\zephyr.bin" -Destination "$SetoolsDir\build\images\he_app.bin" -Force

Write-Host "`n======================================================" -ForegroundColor Cyan
Write-Host " 3. GENERATING TOC PACKAGE " -ForegroundColor Cyan
Write-Host "======================================================" -ForegroundColor Cyan

Set-Location $SetoolsDir
.\app-gen-toc.exe -f build\config\zephyr_mram_cfg.json
if ($LASTEXITCODE -ne 0) { Write-Host "[!] TOC Generation Failed!" -ForegroundColor Red; exit 1 }

Write-Host "`n======================================================" -ForegroundColor Cyan
Write-Host " 4. FLASHING TO BOARD OVER $ComPort " -ForegroundColor Cyan
Write-Host "======================================================" -ForegroundColor Cyan

Write-Host "`n*** IMPORTANT ***" -ForegroundColor Yellow
Write-Host "The flasher will start now. When you see it hang or say 'Waiting...', " -ForegroundColor Yellow
Write-Host "please press the physical RESET button on the board!" -ForegroundColor Yellow
Write-Host "*****************`n" -ForegroundColor Yellow

.\app-write-mram.exe -nr -p -c $ComPort
if ($LASTEXITCODE -ne 0) { Write-Host "[!] Flashing Failed! Did you press RESET?" -ForegroundColor Red; exit 1 }

Write-Host "`n======================================================" -ForegroundColor Green
Write-Host " FLASHING COMPLETE! " -ForegroundColor Green
Write-Host "======================================================" -ForegroundColor Green

Write-Host "`nPlease press the RESET button one more time to boot the new firmware!`n" -ForegroundColor Yellow
Start-Sleep -Seconds 2

if ($ProjectDir -match "ocr-classifier") {
    Write-Host "Launching OCR Real-Time Streamer GUI..." -ForegroundColor Magenta
    python $Workspace\ocr_streamer.py
} elseif ($ProjectDir -match "iris-classifier") {
    Write-Host "Launching Iris Real-Time Simulator GUI..." -ForegroundColor Magenta
    python $Workspace\iris_simulator.py
} elseif ($ProjectDir -match "uart-cli-led") {
    Write-Host "Launching Interactive UART Terminal..." -ForegroundColor Magenta
    python $Workspace\interactive_com3.py
} else {
    Write-Host "Deployment finished. Press the RESET button on your board to run the app!"
}
