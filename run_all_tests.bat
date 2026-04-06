@echo off
set EXE=build_vs18/cli/Debug/p9b.exe
echo.
bash tests/review/run_tests.sh %EXE%
echo.
bash tests/phase2/run_tests.sh %EXE%
echo.
bash tests/phase3/run_tests.sh %EXE%
echo.
bash tests/phase4/run_tests.sh %EXE%
echo.
bash tests/phase5/run_tests.sh %EXE%
echo.
bash tests/phase6/run_tests.sh %EXE%
echo.
bash tests/phase7/run_tests.sh %EXE%
echo.
bash tests/phase8/run_tests.sh %EXE%
echo.
bash tests/phase9/run_tests.sh %EXE%
