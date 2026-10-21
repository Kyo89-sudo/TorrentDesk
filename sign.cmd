@echo off
rem Firma locale di TorrentDesk con un certificato di firma del codice (token USB o HSM cloud).
rem Uso:   sign.cmd IMPRONTA_SHA1 [URL_TIMESTAMP]
rem        oppure imposta SIGN_THUMBPRINT (e facoltativamente SIGN_TIMESTAMP) e lancia sign.cmd
rem Esegui prima build.cmd (o la build CMake). Servono Windows SDK (signtool), Inno Setup 6 e Python 3.
rem Risultato in dist\: exe firmato dentro setup e portable, setup firmato, SHA256SUMS.txt.
setlocal EnableDelayedExpansion
cd /d "%~dp0"

set "THUMB=%~1"
if not defined THUMB set "THUMB=%SIGN_THUMBPRINT%"
set "THUMB=%THUMB: =%"
if not defined THUMB (
  echo Indica l'impronta SHA1 del certificato: sign.cmd IMPRONTA_SHA1 [URL_TIMESTAMP]
  echo Per trovarla: certutil -user -store My   ^(riga "Hash cert.^(sha1^)"^)
  exit /b 1
)
set "TSA=%~2"
if not defined TSA set "TSA=%SIGN_TIMESTAMP%"
if not defined TSA set "TSA=http://time.certum.pl"

rem --- signtool dal Windows SDK (versione piu recente)
set "SIGNTOOL="
for /f "delims=" %%i in ('where signtool 2^>nul') do if not defined SIGNTOOL set "SIGNTOOL=%%i"
if not defined SIGNTOOL (
  for /f "delims=" %%d in ('dir /b /ad /o-n "%ProgramFiles(x86)%\Windows Kits\10\bin\10.*" 2^>nul') do (
    if not defined SIGNTOOL if exist "%ProgramFiles(x86)%\Windows Kits\10\bin\%%d\x64\signtool.exe" set "SIGNTOOL=%ProgramFiles(x86)%\Windows Kits\10\bin\%%d\x64\signtool.exe"
  )
)
if not defined SIGNTOOL (
  echo signtool.exe non trovato. Installa il Windows SDK ^(componente "Windows SDK Signing Tools"^).
  exit /b 1
)

rem --- Inno Setup
set "ISCC=%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe"
if not exist "%ISCC%" set "ISCC=%ProgramFiles%\Inno Setup 6\ISCC.exe"
if not exist "%ISCC%" (
  echo Inno Setup 6 non trovato ^(ISCC.exe^).
  exit /b 1
)
where python >nul 2>nul || (echo Python 3 non trovato nel PATH. & exit /b 1)

rem --- eseguibile da firmare
if not exist TorrentDesk.exe if exist build\Release\TorrentDesk.exe copy /y build\Release\TorrentDesk.exe TorrentDesk.exe >nul
if not exist TorrentDesk.exe (
  echo TorrentDesk.exe non trovato: esegui prima build.cmd.
  exit /b 1
)

echo.
echo [1/4] Firma di TorrentDesk.exe ^(il token puo chiedere il PIN^)...
"%SIGNTOOL%" sign /sha1 %THUMB% /fd SHA256 /tr "%TSA%" /td SHA256 /d "TorrentDesk" TorrentDesk.exe
if errorlevel 1 exit /b 1
"%SIGNTOOL%" verify /pa /q TorrentDesk.exe
if errorlevel 1 (echo Verifica firma fallita. & exit /b 1)

echo [2/4] Installer con l'eseguibile firmato...
"%ISCC%" /Qp installer.iss
if errorlevel 1 exit /b 1
set "SETUP="
for %%f in (dist\TorrentDesk-*-Setup-x64.exe) do set "SETUP=%%f"
if not defined SETUP (echo Setup non generato. & exit /b 1)

echo [3/4] Firma di %SETUP%...
"%SIGNTOOL%" sign /sha1 %THUMB% /fd SHA256 /tr "%TSA%" /td SHA256 /d "TorrentDesk Setup" "%SETUP%"
if errorlevel 1 exit /b 1
"%SIGNTOOL%" verify /pa /q "%SETUP%"
if errorlevel 1 (echo Verifica firma setup fallita. & exit /b 1)

echo [4/4] Portable, sorgenti e SHA256SUMS...
python tools\package.py
if errorlevel 1 exit /b 1
powershell -NoProfile -Command "Get-ChildItem dist -File | Where-Object { $_.Name -like '*-Setup-x64.exe' -or $_.Name -like '*-portable-x64.zip' } | ForEach-Object { '{0}  {1}' -f (Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLower(), $_.Name } | Set-Content -Encoding ascii dist\SHA256SUMS.txt; Get-Content dist\SHA256SUMS.txt"
if errorlevel 1 exit /b 1

echo.
echo Fatto. File pronti per la release in %~dp0dist
endlocal
