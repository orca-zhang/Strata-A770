@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
call "C:\Program Files (x86)\Intel\oneAPI\compiler\2025.3\env\vars.bat"
call "C:\Program Files (x86)\Intel\oneAPI\mkl\2025.3\env\vars.bat"
set "CPATH=C:\Program Files\LevelZeroSDK\1.33.1\include;%CPATH%"
set "LIB=C:\Program Files\LevelZeroSDK\1.33.1\lib\x64;%LIB%"
cd /d "g:\GitHub\Strata\build-sycl-a770-deepopt"
ninja %*
