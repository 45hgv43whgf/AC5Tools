@echo off
setlocal

set "PROJECT_DIR=%~dp0"
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"

if defined VCToolsInstallDir goto Build
if not exist "%VSWHERE%" goto NoVisualStudio
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALL=%%i"
if not defined VSINSTALL goto NoVisualStudio
call "%VSINSTALL%\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b %errorlevel%
goto Build

:NoVisualStudio
echo Error: Visual Studio C++ tools not found. Run this from a Developer Command Prompt or install VS Build Tools.
exit /b 1

:Build

if not exist "%PROJECT_DIR%bin" mkdir "%PROJECT_DIR%bin"
if not exist "%PROJECT_DIR%obj" mkdir "%PROJECT_DIR%obj"

rc /nologo /fo "%PROJECT_DIR%obj\AC5Tools.res" "%PROJECT_DIR%AC5Tools.rc"
if errorlevel 1 exit /b %errorlevel%

cl /nologo /O2 /LD /EHsc /MT ^
 /I"%PROJECT_DIR%third_party\imgui" ^
 /I"%PROJECT_DIR%third_party\minhook\include" ^
 /I"%PROJECT_DIR%third_party\minhook\src" ^
 /Fo"%PROJECT_DIR%obj\\" ^
 /Fe:"%PROJECT_DIR%bin\AC5Tools.asi" ^
 "%PROJECT_DIR%AC5Tools.cpp" ^
 "%PROJECT_DIR%third_party\imgui\imgui.cpp" ^
 "%PROJECT_DIR%third_party\imgui\imgui_draw.cpp" ^
 "%PROJECT_DIR%third_party\imgui\imgui_tables.cpp" ^
 "%PROJECT_DIR%third_party\imgui\imgui_widgets.cpp" ^
 "%PROJECT_DIR%third_party\imgui\backends\imgui_impl_dx11.cpp" ^
 "%PROJECT_DIR%third_party\imgui\backends\imgui_impl_win32.cpp" ^
 "%PROJECT_DIR%third_party\minhook\src\buffer.c" ^
 "%PROJECT_DIR%third_party\minhook\src\hook.c" ^
 "%PROJECT_DIR%third_party\minhook\src\trampoline.c" ^
 "%PROJECT_DIR%third_party\minhook\src\hde\hde64.c" ^
 "%PROJECT_DIR%obj\AC5Tools.res" ^
 /link /DLL /NOLOGO user32.lib gdi32.lib d3d11.lib dxgi.lib dinput8.lib dxguid.lib
if errorlevel 1 exit /b %errorlevel%

call "%PROJECT_DIR%package_release.bat" "%PROJECT_DIR%bin\AC5Tools.asi"
