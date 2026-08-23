@echo off
setlocal

rem STT_Bridge 빌드 스크립트. 필요한 것:
rem   - Visual Studio(MSVC, C++23 지원 버전) - vcvars64.bat 경로를 아래서 맞게 고칠 것
rem   - Vulkan SDK (asr.hpp가 무조건 vulkan.hpp를 include함) - https://vulkan.lunarg.com/
rem   - D:\WITH_OUT_ProtoType\Tools\nvigi_pack_1_6_0\ (NVIDIA NVIGI SDK 압축 해제본)
rem
rem 빌드 후 STT_Bridge.exe를 Tools\STT_Bridge\ 로 복사하고, 그 폴더에 이미 있는
rem DLL(bin\x64\Release\*)과 data\nvigi.models\ (whisper-small.gguf 포함)를 그대로 재사용하면 된다.

set VCVARS="C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvars64.bat"
set VULKAN_INC="C:\VulkanSDK\1.4.350.0\Include"
set NVIGI_ROOT=D:\WITH_OUT_ProtoType\Tools\nvigi_pack_1_6_0
set SRC=%~dp0main.cpp
set OUT=%~dp0STT_Bridge.exe

call %VCVARS%
cl /std:c++latest /EHsc /utf-8 ^
    /I "%NVIGI_ROOT%\include" ^
    /I "%NVIGI_ROOT%\nvigi_core\include" ^
    /I "%NVIGI_ROOT%\source\samples\shared" ^
    /I %VULKAN_INC% ^
    "%SRC%" /Fe:"%OUT%" ^
    /link d3d12.lib dxgi.lib user32.lib ws2_32.lib

echo.
echo 빌드 완료: %OUT%
echo Tools\STT_Bridge\ 로 복사해서 기존 DLL/D3D12/data 폴더와 함께 쓰면 됩니다.
pause
