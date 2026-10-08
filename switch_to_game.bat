@echo off
echo ===================================================
echo Switching ESP-IDF Target to: WHACK-A-MOLE GAME (main.c)
echo ===================================================
(
  echo idf_component_register^(SRCS "main.c" INCLUDE_DIRS "."^)
) > "%~dp0main\CMakeLists.txt"

echo Target switched! Building project...
call "%~dp0build.bat"
