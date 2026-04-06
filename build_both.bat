@echo off
set CM=C:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe
"%CM%" --build C:\Dev\basic\build_vs18 --config Debug
"%CM%" --build C:\Dev\basic\build_vs18 --config Release
