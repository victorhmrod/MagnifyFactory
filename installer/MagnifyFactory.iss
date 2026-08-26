; Inno Setup script for MagnifyFactory.
; Builds a per-user installer (no admin rights required) that:
;   - installs the app under %LOCALAPPDATA%\Programs\MagnifyFactory
;   - registers the Explorer "Convert with MagnifyFactory" context menu
;     entry pointing at the INSTALLED exe (not a build path)
;   - creates Start Menu / optional Desktop shortcuts
;   - cleanly removes the registry entries and files on uninstall
;
; Build with: iscc installer\MagnifyFactory.iss
; Expects a Release build already produced at build-release\ (see
; scripts\build_installer.ps1, which does both steps).

#define MyAppName "MagnifyFactory"
#define MyAppVersion "0.1.0"
#define MyAppPublisher "MagnifyFactory contributors"
#define MyAppURL "https://github.com/victorhmrod/MagnifyFactory"
#define MyAppExeName "MagnifyFactory.exe"
#define ReleaseDir "..\build-release"

[Setup]
AppId={{6F6B9D2B-6C7C-4C2C-9C7B-6D6E6C6C6F6F}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
; Per-user install: no admin/UAC prompt required.
PrivilegesRequired=lowest
DefaultDirName={autopf}\{#MyAppName}
DisableProgramGroupPage=yes
OutputDir=..\dist
OutputBaseFilename=MagnifyFactory-Setup-{#MyAppVersion}
SetupIconFile=..\resources\icon.ico
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayIcon={app}\{#MyAppExeName}

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop shortcut"; GroupDescription: "Additional icons:"; Flags: unchecked
Name: "contextmenu"; Description: "Add ""Convert with MagnifyFactory"" to the Explorer right-click menu"; GroupDescription: "Integration:"; Flags: unchecked

[Files]
Source: "{#ReleaseDir}\{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#ReleaseDir}\*.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#ReleaseDir}\platforms\*.dll"; DestDir: "{app}\platforms"; Flags: ignoreversion
Source: "..\LICENSE"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\README.md"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

; Registered under HKCU only — matches the app's own per-user context menu
; script, and needs no elevation. {app} resolves to wherever THIS install
; actually placed the exe, so it never points at a stale build path.
[Registry]
Root: HKCU; Subkey: "Software\Classes\*\shell\MagnifyFactory"; ValueType: string; ValueName: ""; ValueData: "Convert with MagnifyFactory"; Tasks: contextmenu; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\*\shell\MagnifyFactory"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\{#MyAppExeName}"""; Tasks: contextmenu
Root: HKCU; Subkey: "Software\Classes\*\shell\MagnifyFactory\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: contextmenu

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "Launch MagnifyFactory"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
Type: filesandordirs; Name: "{app}"
