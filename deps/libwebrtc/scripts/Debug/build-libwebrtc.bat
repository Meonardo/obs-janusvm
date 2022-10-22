SET OUT_PATH=%~1

call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvars64.bat"

set root=D:\File\WebRTC\LibWebRTC\src\libwebrtc\scripts
cd /D %root%

set DEPOT_TOOLS_WIN_TOOLCHAIN=0
set GYP_MSVS_VERSION=2019
set GYP_GENERATORS=ninja,msvs-ninja
set GYP_MSVS_OVERRIDE_PATH=C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional

set VAR_1=--scheme debug
set VAR_2=--output_path %OUT_PATH%

python build-libwebrtc-win.py %VAR_1% %VAR_2%