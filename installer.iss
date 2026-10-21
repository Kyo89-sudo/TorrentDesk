#ifndef AppExe
#define AppExe "TorrentDesk.exe"
#endif
[Setup]
AppId={{7C30D651-BE07-48BF-AC56-28D599D6A2E5}
AppName=TorrentDesk
AppVersion=0.3.0
DefaultDirName={localappdata}\Programs\TorrentDesk
DefaultGroupName=TorrentDesk
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=dist
OutputBaseFilename=TorrentDesk-0.3.0-Setup-x64
SetupIconFile=assets\torrentdesk.ico
Compression=lzma2
SolidCompression=yes
UninstallDisplayIcon={app}\TorrentDesk.exe
[Files]
Source: "{#AppExe}"; DestDir: "{app}"; Flags: ignoreversion
Source: "README.it.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "LICENSE"; DestDir: "{app}"; Flags: ignoreversion
Source: "NOTICE"; DestDir: "{app}"; Flags: ignoreversion
Source: "vendor\LICENSE.MIT"; DestDir: "{app}"; DestName: "LICENSE-json.txt"; Flags: ignoreversion
Source: "profiles\*.client"; DestDir: "{app}\profiles"; Flags: ignoreversion
Source: "languages\*.json"; DestDir: "{app}\languages"; Flags: ignoreversion
Source: "docs\*.html"; DestDir: "{app}\docs"; Flags: ignoreversion
Source: "assets\flow-logo.png"; DestDir: "{app}\assets"; Flags: ignoreversion
[Icons]
Name: "{group}\TorrentDesk"; Filename: "{app}\TorrentDesk.exe"
[Languages]
Name: "italian"; MessagesFile: "compiler:Languages\Italian.isl"

Name: "english"; MessagesFile: "compiler:Default.isl"
