@echo off
setlocal
set IDF_PATH=C:\Users\eugen\esp-idf
set PATH=C:\Users\eugen\.espressif\python_env\idf5.4_py3.11_env\Scripts;C:\Users\eugen\esp-idf\tools;C:\Users\eugen\.espressif\tools\openocd-esp32\v0.12.0-esp32-20241016\bin;C:\Users\eugen\.espressif\tools\cmake\3.30.2\bin;C:\Users\eugen\.espressif\tools\ninja\1.12.1;C:\Users\eugen\.espressif\tools\idf-exe\1.0.3;C:\Users\eugen\.espressif\tools\ccache\4.10.2;C:\Users\eugen\.espressif\tools\dfu-util\0.11\bin;C:\Users\eugen\.espressif\tools\esp-rom-elfs\20241011;C:\Windows\System32;C:\Windows;C:\Windows\System32\WindowsPowerShell\v1.0;%PATH%
call "%IDF_PATH%\export.bat"
cd /d "%~dp0"
idf.py build
endlocal
