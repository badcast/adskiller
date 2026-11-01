; AdsKiller - Windows per-user installer (Inno Setup 6.3+)
;
; Installs into the current user's profile, no administrator rights required:
;   %LOCALAPPDATA%\Programs\AdsKiller   (e.g. C:\Users\<name>\AppData\Local\Programs\AdsKiller)
;
; Build (all values are overridable from the command line):
;   ISCC.exe /DAppVersion=1.3.14 /DSourceDir=..\build\deploy /DOutputName=Adskiller-Setup installer\adskiller.iss

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#ifndef SourceDir
  #define SourceDir "..\build\deploy"
#endif
#ifndef OutputName
  #define OutputName "Adskiller-Setup"
#endif

#define AppName "AdsKiller"
#define AppExe "adskiller.exe"
#define AppUpdateExe "update.exe"

[Setup]
; Keep this GUID constant between releases: it lets a new installer upgrade the old one in place.
AppId={{7A1C5E0B-3F2D-4B6E-9A41-5C8D2E7F1A93}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher=imister.kz
AppPublisherURL=https://imister.tech
AppSupportURL=https://imister.tech
VersionInfoVersion={#AppVersion}

; Per-user install: no UAC prompt, files live in the user's profile and stay writable for the updater.
PrivilegesRequired=lowest
DefaultDirName={localappdata}\Programs\{#AppName}
UsePreviousAppDir=yes
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
DisableDirPage=auto

ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible

OutputDir=..\build\installer
OutputBaseFilename={#OutputName}
SetupIconFile=..\Resources\res\icon.ico
UninstallDisplayIcon={app}\{#AppExe}
LicenseFile=..\LICENSE

Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern

; Close a running copy during install/upgrade instead of failing on locked files.
CloseApplications=yes
RestartApplications=no
SetupMutex=AdsKillerSetupMutex

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "russian"; MessagesFile: "compiler:Languages\Russian.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\{#AppName}"; Filename: "{app}\{#AppExe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExe}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#AppExe}"; Description: "{cm:LaunchProgram,{#AppName}}"; Flags: nowait postinstall skipifsilent

[UninstallRun]
; Make sure nothing keeps the files locked while uninstalling.
Filename: "{sys}\taskkill.exe"; Parameters: "/F /IM {#AppExe} /IM {#AppUpdateExe}"; Flags: runhidden; RunOnceId: "KillAdsKiller"

[UninstallDelete]
; Files created at run time (updater leftovers, caches) live next to the program.
Type: filesandordirs; Name: "{app}"
