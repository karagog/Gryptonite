; Script generated by the Inno Setup Script Wizard.
; SEE THE DOCUMENTATION FOR DETAILS ON CREATING INNO SETUP SCRIPT FILES!

#define MyAppName "Gryptonite"
#define MyAppVersion "3.0.0"
#define MyAppPublisher "Rapstallion"
#define MyAppURL "https://github.com/karagog/Gryptonite"
#define MyAppExeName "gryptonite.exe"

#define TopDir "..\.."
#define QtPath "C:\Qt\5.3\mingw482_32"

[Setup]
; NOTE: The value of AppId uniquely identifies this application.
; Do not use the same AppId value in installers for other applications.
; (To generate a new GUID, click Tools | Generate GUID inside the IDE.)
AppId={{9F2CE13A-FF14-4147-AB50-437FC8AE234F}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
;AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={pf}\{#MyAppName}
DefaultGroupName={#MyAppName}
OutputDir=.
OutputBaseFilename={#MyAppName}_{#MyAppVersion}_setup
Compression=lzma
SolidCompression=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "quicklaunchicon"; Description: "{cm:CreateQuickLaunchIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked; OnlyBelowVersion: 0,6.1
Name: "associate"; Description: "&Associate .gdb and .GPdb files"; GroupDescription: "Files:";

[Files]
; Application executables and shared libraries
Source: "{#TopDir}\bin\{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#TopDir}\bin\grypto_transforms.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#TopDir}\lib\grypto_core.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#TopDir}\lib\grypto_legacy.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#TopDir}\lib\grypto_ui.dll"; DestDir: "{app}"; Flags: ignoreversion

; GUtil shared libraries
Source: "{#TopDir}\gutil\lib\GUtil.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#TopDir}\gutil\lib\GUtilAboutPlugin.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#TopDir}\gutil\lib\GUtilCryptoPP.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#TopDir}\gutil\lib\GUtilQt.dll"; DestDir: "{app}"; Flags: ignoreversion

; Qt shared libraries
Source: "{#QtPath}\bin\Qt5Core.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#QtPath}\bin\Qt5Gui.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#QtPath}\bin\Qt5Widgets.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#QtPath}\bin\Qt5Sql.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#QtPath}\bin\Qt5Xml.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#QtPath}\bin\Qt5WinExtras.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#QtPath}\bin\libgcc_s_dw2-1.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#QtPath}\bin\libstdc++-6.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#QtPath}\bin\libwinpthread-1.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#QtPath}\bin\icudt52.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#QtPath}\bin\icuin52.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#QtPath}\bin\icuuc52.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#QtPath}\plugins\platforms\qwindows.dll"; DestDir: "{app}/platforms"; Flags: ignoreversion
; NOTE: Don't use "Flags: ignoreversion" on any shared system files

[Registry]
; Create a handler for regular database files
Root: HKCR; Subkey: "Gryptonite.gdb"; ValueType: string; ValueData: "Gryptonite Database"; Flags: uninsdeletekey;
Root: HKCR; Subkey: "Gryptonite.gdb\DefaultIcon"; ValueType: string; ValueData: "{app}\{#MyAppExeName}"; Flags: uninsdeletekey;
Root: HKCR; Subkey: "Gryptonite.gdb\shell\Open\Command"; ValueType: string; ValueData: "{app}\{#MyAppExeName} ""%1"" "; Flags: uninsdeletekey;

; These file types are only associated if the "associate" task was selected (the default)
Root: HKCR; Subkey: ".gdb"; ValueType: string; ValueData: "Gryptonite.gdb"; Flags: uninsdeletekey; Tasks: associate
Root: HKCR; Subkey: ".GPdb"; ValueType: string; ValueData: "Gryptonite.gdb"; Flags: uninsdeletekey; Tasks: associate

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{commondesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon
Name: "{userappdata}\Microsoft\Internet Explorer\Quick Launch\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: quicklaunchicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent
