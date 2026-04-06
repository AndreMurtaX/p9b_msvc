@echo off
setlocal enabledelayedexpansion
set EXE=C:\Dev\basic\build_vs18\cli\Release\p9b.exe
set DIR=C:\Dev\basic\tests\undef
set PASS=0
set FAIL=0

echo ============================================================
echo Plan9Basic - Undefined Variable Detection Tests
echo ============================================================
echo.

:: ---- Tests that MUST produce a parse error ----
for %%F in (t01 t02 t03 t04 t11 t14 t18) do (
    set FILE=%DIR%\%%F_*.bas
    for %%G in (%DIR%\%%F_*.bas) do (
        set OUT=
        for /f "delims=" %%L in ('""%EXE%" "%%G" 2>&1"') do set OUT=%%L
        echo !OUT! | findstr /i "error" >nul 2>&1
        if !errorlevel!==0 (
            echo [PASS] %%~nG  -^>  !OUT!
            set /a PASS+=1
        ) else (
            echo [FAIL] %%~nG  expected error, got: !OUT!
            set /a FAIL+=1
        )
    )
)

echo.
:: ---- Tests that MUST succeed ----
for %%F in (t05 t06 t07 t09 t10 t12 t13 t15 t16 t17) do (
    for %%G in (%DIR%\%%F_*.bas) do (
        set OUT=
        for /f "delims=" %%L in ('""%EXE%" "%%G" 2>&1"') do (
            echo !OUT! | findstr /i "error" >nul 2>&1
            if !errorlevel!==0 (
                set OUT=%%L
            ) else (
                if "!OUT!"=="" set OUT=%%L
            )
        )
        "%EXE%" "%%G" >nul 2>&1
        if !errorlevel!==0 (
            echo [PASS] %%~nG
            set /a PASS+=1
        ) else (
            echo [FAIL] %%~nG  unexpected error
            set /a FAIL+=1
        )
    )
)

echo.
echo ============================================================
echo Results: !PASS! passed, !FAIL! failed
echo ============================================================
endlocal
