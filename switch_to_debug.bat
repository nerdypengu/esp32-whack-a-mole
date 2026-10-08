@echo off
echo ===================================================
echo Switching ESP-IDF Target to: BUTTON DIAGNOSTIC TOOL (main_button_debug.c)
echo ===================================================
(
  echo idf_component_register^(SRCS "main_button_debug.c" INCLUDE_DIRS "."^)
) > "%~dp0main\CMakeLists.txt"

echo Target switched! Building project...
call "%~dp0build.bat"
