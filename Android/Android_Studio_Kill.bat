@echo off
setlocal EnableExtensions

rem Android Studio / Gradle generated-file cleaner.
rem Keep source code, Gradle Wrapper, local.properties and project settings intact.

cd /d "%~dp0"

echo.
echo [Clean] Project: %CD%
echo.

call :CheckAndroidStudioProject || goto :ErrorExit

if exist "gradlew.bat" (
    echo [Clean] Stopping Gradle daemon...
    call "gradlew.bat" --stop >nul 2>nul
)

call :RemoveDir ".gradle" || goto :ErrorExit
call :RemoveDir ".kotlin" || goto :ErrorExit
call :RemoveDir ".idea\caches" || goto :ErrorExit

for /d /r "%CD%" %%D in (build .cxx .externalNativeBuild) do (
    if exist "%%D" call :RemoveDir "%%D" || goto :ErrorExit
)

echo.
echo [Clean] Done.
exit /b 0

:CheckAndroidStudioProject
set "HAS_SETTINGS="
set "HAS_ROOT_BUILD="
set "HAS_ANDROID_MANIFEST="

if exist "settings.gradle" set "HAS_SETTINGS=1"
if exist "settings.gradle.kts" set "HAS_SETTINGS=1"
if exist "build.gradle" set "HAS_ROOT_BUILD=1"
if exist "build.gradle.kts" set "HAS_ROOT_BUILD=1"

if not defined HAS_SETTINGS (
    echo [Error] settings.gradle or settings.gradle.kts was not found.
    echo [Error] Please run this script from an Android Studio project root.
    exit /b 1
)

if not defined HAS_ROOT_BUILD (
    echo [Error] build.gradle or build.gradle.kts was not found.
    echo [Error] Please run this script from an Android Studio project root.
    exit /b 1
)

for /r "%CD%" %%F in (AndroidManifest.xml) do (
    set "HAS_ANDROID_MANIFEST=1"
    goto :FoundAndroidManifest
)

:FoundAndroidManifest
if not defined HAS_ANDROID_MANIFEST (
    echo [Error] AndroidManifest.xml was not found.
    echo [Error] This does not look like an Android Studio Android project.
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
