; Script generated by the Inno Setup Script Wizard.
; SEE THE DOCUMENTATION FOR DETAILS ON CREATING INNO SETUP SCRIPT FILES!

#define MyAppName "Gryptonite"
#define MyAppVersion "3.1.1"
#define MyAppPublisher "Rapstallion"
#define MyAppURL "https://github.com/karagog/Gryptonite"
#define MyAppExeName "gryptonite.exe"

#define TopDir "..\.."

[Setup]
; NOTE: The value of AppId uniquely identifies this application.
; Do not use the same AppId value in installers for other applications.
; (To generate a new GUID, click Tools | Generate GUID inside the IDE.)
AppId={{60E0B876-8466-4046-B96F-F40B00515DFD}
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
LicenseFile={#TopDir}\LICENSE

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "quicklaunchicon"; Description: "{cm:CreateQuickLaunchIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked; OnlyBelowVersion: 0,6.1
Name: "associate"; Description: "&Associate .gdb and .GPdb files"; GroupDescription: "Files:";

[Files]
; License files
Source: "{#TopDir}\LICENSE"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#TopDir}\NOTICE"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#TopDir}\installers\LICENSE_CRYPTOPP"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#TopDir}\installers\LICENSE_QT"; DestDir: "{app}"; Flags: ignoreversion

; Application executables and shared libraries
Source: "{#TopDir}\bin\{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#TopDir}\bin\grypto_transforms.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#TopDir}\bin\grypto_rng.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#TopDir}\bin\grypto_legacy_plugin.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#TopDir}\bin\grypto_core.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#TopDir}\bin\grypto_ui.dll"; DestDir: "{app}"; Flags: ignoreversion

; GUtil shared libraries
Source: "{#TopDir}\bin\GUtil.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#TopDir}\bin\GUtilAboutPlugin.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#TopDir}\bin\GUtilCryptoPP.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#TopDir}\bin\GUtilQt.dll"; DestDir: "{app}"; Flags: ignoreversion

; Qt shared libraries
Source: "{#TopDir}\bin\Qt5Core.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#TopDir}\bin\Qt5Gui.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#TopDir}\bin\Qt5Widgets.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#TopDir}\bin\Qt5Sql.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#TopDir}\bin\Qt5Svg.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#TopDir}\bin\Qt5Xml.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#TopDir}\bin\Qt5Network.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#TopDir}\bin\Qt5WinExtras.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#TopDir}\bin\ssleay32.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#TopDir}\bin\libeay32.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#TopDir}\bin\libgcc_s_dw2-1.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#TopDir}\bin\libstdc++-6.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#TopDir}\bin\libwinpthread-1.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#TopDir}\bin\icudt52.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#TopDir}\bin\icuin52.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#TopDir}\bin\icuuc52.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#TopDir}\bin\platforms\qwindows.dll"; DestDir: "{app}/platforms"; Flags: ignoreversion
Source: "{#TopDir}\bin\sqldrivers\qsqlite.dll"; DestDir: "{app}/sqldrivers"; Flags: ignoreversion
Source: "{#TopDir}\bin\accessible\*"; DestDir: "{app}/accessible"; Flags: ignoreversion
Source: "{#TopDir}\bin\imageformats\*"; DestDir: "{app}/imageformats"; Flags: ignoreversion
Source: "{#TopDir}\bin\iconengines\*"; DestDir: "{app}/iconengines"; Flags: ignoreversion
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

