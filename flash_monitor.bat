@echo off
setlocal
set COM_PORT=%1
if "%COM_PORT%"=="" set /p COM_PORT=Enter COM port e.g. COM3: 
set IDF_PATH=C:\Users\eugen\esp-idf
set PATH=C:\Users\eugen\.espressif\python_env\idf5.4_py3.11_env\Scripts;C:\Users\eugen\esp-idf\tools;C:\Users\eugen\.espressif\tools\openocd-esp32\v0.12.0-esp32-20241016\bin;C:\Users\eugen\.espressif\tools\cmake\3.30.2\bin;C:\Users\eugen\.espressif\tools\ninja\1.12.1;C:\Users\eugen\.espressif\tools\idf-exe\1.0.3;C:\Users\eugen\.espressif\tools\ccache\4.10.2;C:\Users\eugen\.espressif\tools\dfu-util\0.11\bin;C:\Users\eugen\.espressif\tools\esp-rom-elfs\20241011;C:\Windows\System32;C:\Windows;C:\Windows\System32\WindowsPowerShell\v1.0;%PATH%
call "%IDF_PATH%\export.bat"
cd /d "%~dp0"

echo ---------------------------------------------------
echo Flashing app and starting monitor on %COM_PORT%...
echo (Press Ctrl+] or Ctrl+C to exit monitor)
echo ---------------------------------------------------
idf.py -p %COM_PORT% -b 115200 flash monitor

echo.
echo ---------------------------------------------------
echo Monitor stopped! Automatically wiping OLED screen...
echo ---------------------------------------------------
python -m esptool --chip esp32 -p %COM_PORT% -b 115200 --before default_reset --after hard_reset write_flash 0x1000 build\bootloader\bootloader.bin 0x8000 build\partition_table\partition-table.bin 0x10000 clear_oled.bin

echo.
echo ===================================================
echo SUCCESS! OLED display screen wiped 100%% black.
echo ===================================================
endlocal
