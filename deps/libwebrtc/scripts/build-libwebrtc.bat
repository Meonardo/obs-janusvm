SET ROOT_DIR=%1
SET SCHEME=%~2
SET OUTPUT_PATH=%~3

if "%SCHEME%" == "Debug" (
    SET SCHEME="debug" 
) else ( 
    SET SCHEME="release"
)

call "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"

cd /D %ROOT_DIR%

set DEPOT_TOOLS_WIN_TOOLCHAIN=0
set GYP_MSVS_VERSION=2022
set GYP_GENERATORS=ninja,msvs-ninja
set GYP_MSVS_OVERRIDE_PATH=C:\Program Files\Microsoft Visual Studio\2022\Professional

set VAR_1=--scheme %SCHEME%
set VAR_2=--output_path %OUTPUT_PATH%

python build-libwebrtc-win.py %VAR_1% %VAR_2%