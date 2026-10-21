@echo off
setlocal
if defined VSINSTALLDIR goto compile
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
 echo Install Visual Studio Build Tools with Desktop development with C++.
 exit /b 1
)
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "DESK_VS=%%i"
if not defined DESK_VS exit /b 1
call "%DESK_VS%\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64
if errorlevel 1 exit /b 1
:compile
cd /d "%~dp0"
rc /nologo /fo app.res app.rc
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /EHsc /W4 /utf-8 /MT /O2 /DUNICODE /D_UNICODE /DNOMINMAX src\main.cpp app.res /Fe:TorrentDesk.exe /Fo:main.obj /link user32.lib gdi32.lib comdlg32.lib shell32.lib comctl32.lib uxtheme.lib ole32.lib bcrypt.lib winhttp.lib ws2_32.lib /SUBSYSTEM:WINDOWS /MANIFEST:NO
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /EHsc /W4 /utf-8 /MT /DUNICODE /D_UNICODE /DNOMINMAX src\tests.cpp /Fe:TorrentDeskTests.exe /Fo:tests.obj /link bcrypt.lib winhttp.lib ws2_32.lib
if errorlevel 1 exit /b 1
TorrentDeskTests.exe "%~dp0."
