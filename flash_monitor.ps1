param(
    [string]$ComPort
)

if (-not $ComPort) {
    $ComPort = Read-Host "Enter COM port (e.g. COM3)"
}

$env:IDF_PATH = "C:\Users\eugen\esp-idf"
$env:PATH = "C:\Users\eugen\.espressif\python_env\idf5.4_py3.11_env\Scripts;C:\Users\eugen\esp-idf\tools;C:\Users\eugen\.espressif\tools\openocd-esp32\v0.12.0-esp32-20241016\bin;C:\Users\eugen\.espressif\tools\cmake\3.30.2\bin;C:\Users\eugen\.espressif\tools\ninja\1.12.1;C:\Users\eugen\.espressif\tools\idf-exe\1.0.3;C:\Users\eugen\.espressif\tools\ccache\4.10.2;C:\Users\eugen\.espressif\tools\dfu-util\0.11\bin;C:\Users\eugen\.espressif\tools\esp-rom-elfs\20241011;C:\Windows\System32;C:\Windows;C:\Windows\System32\WindowsPowerShell\v1.0;$env:PATH"

cmd.exe /c "call `"$env:IDF_PATH\export.bat`" && idf.py -p $ComPort flash monitor"

Write-Host "Monitor stopped. Automatically wiping OLED screen..." -ForegroundColor Yellow
cmd.exe /c "call `"$env:IDF_PATH\export.bat`" && python -m esptool --chip esp32 -p $ComPort -b 115200 --before default_reset --after hard_reset write_flash 0x1000 build\bootloader\bootloader.bin 0x8000 build\partition_table\partition-table.bin 0x10000 clear_oled.bin"
Write-Host "OLED display screen wiped 100% black!" -ForegroundColor Green
