@echo off
setlocal EnableExtensions

rem DevEco Studio / HarmonyOS generated-file cleaner.
rem Keep source code, project configuration, oh_modules and local.properties intact.

cd /d "%~dp0"

echo.
echo [Clean] Project: %CD%
echo.

call :CheckDevEcoProject || goto :ErrorExit

call :RemoveDir ".hvigor" || goto :ErrorExit
call :RemoveDir ".appanalyzer" || goto :ErrorExit
call :RemoveDir ".cxx" || goto :ErrorExit

for /d /r "%CD%" %%D in (build .test) do (
    if exist "%%D" call :RemoveDir "%%D" || goto :ErrorExit
)

echo.
echo [Clean] Done.
exit /b 0

:CheckDevEcoProject
set "HAS_BUILD_PROFILE="
set "HAS_HVIGOR_FILE="
set "HAS_PACKAGE="
set "HAS_APP_SCOPE="
set "HAS_MODULE_JSON="

if exist "build-profile.json5" set "HAS_BUILD_PROFILE=1"
if exist "hvigorfile.ts" set "HAS_HVIGOR_FILE=1"
if exist "oh-package.json5" set "HAS_PACKAGE=1"
if exist "AppScope\app.json5" set "HAS_APP_SCOPE=1"

if not defined HAS_BUILD_PROFILE (
    echo [Error] build-profile.json5 was not found.
    echo [Error] Please run this script from a DevEco Studio project root.
    exit /b 1
)

if not defined HAS_HVIGOR_FILE (
    echo [Error] hvigorfile.ts was not found.
    echo [Error] Please run this script from a DevEco Studio project root.
    exit /b 1
)

if not defined HAS_PACKAGE (
    echo [Error] oh-package.json5 was not found.
    echo [Error] Please run this script from a DevEco Studio project root.
    exit /b 1
)

if not defined HAS_APP_SCOPE (
    echo [Error] AppScope\app.json5 was not found.
    echo [Error] This does not look like a HarmonyOS DevEco Studio app project.
    exit /b 1
)

for /r "%CD%" %%F in (module.json5) do (
    set "HAS_MODULE_JSON=1"
    goto :FoundModuleJson
)

:FoundModuleJson
if not defined HAS_MODULE_JSON (
    echo [Error] module.json5 was not found.
    echo [Error] This does not look like a HarmonyOS DevEco Studio app project.
    exit /b 1
)

exit /b 0

:RemoveDir
if exist "%~1\" (
    echo [Delete dir] %~1
    rmdir /s /q "%~1"
    if exist "%~1\" (
        echo [Error] Failed to delete directory: %~1
        exit /b 1
    )
)
exit /b 0

:ErrorExit
echo.
echo [Clean] Failed. Check the error above.
pause
exit /b 1
