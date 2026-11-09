#ifndef AppVersion
  #error Pass the version from the VERSION file: ISCC /DAppVersion=<VERSION> /DAppVersionNumber=<MAJOR.MINOR.PATCH>
#endif
#ifndef AppVersionNumber
  #error Pass the numeric part of VERSION: ISCC /DAppVersionNumber=<MAJOR.MINOR.PATCH>
#endif
#ifndef SourceDir
  #define SourceDir "..\build\Harmony_artefacts\Release"
#endif
#ifndef OutputDir
  #define OutputDir "..\dist"
#endif

#define AppName "Yannosay Harmony"
#define AppExe "Harmony.exe"

[Setup]
AppId={{7C5E0B43-2F7A-4C1B-9B6E-6E1D7F4A9A11}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher=Yannosay Productions
AppPublisherURL=https://harmony.yannosay.com
AppSupportURL=https://github.com/Yannosay/harmony/issues
AppUpdatesURL=https://harmony.yannosay.com
DefaultDirName={autopf}\Yannosay Harmony
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.17763
OutputDir={#OutputDir}
OutputBaseFilename=YannosayHarmony-{#AppVersion}-Setup-x64
SetupIconFile=..\assets\brand\harmony-icon.ico
UninstallDisplayIcon={app}\{#AppExe}
UninstallDisplayName={#AppName}
WizardStyle=modern
WizardImageFile=wizard-large.bmp
WizardSmallImageFile=wizard-small.bmp
Compression=lzma2/ultra64
SolidCompression=yes
CloseApplications=force
RestartApplications=no
ChangesAssociations=yes
LicenseFile=..\LICENSE
VersionInfoVersion={#AppVersionNumber}
VersionInfoCompany=Yannosay Productions
VersionInfoDescription={#AppName} Setup
VersionInfoProductName={#AppName}

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"
Name: "associate"; Description: "Open .hmy project files with Harmony"; GroupDescription: "File associations:"

[Files]
Source: "{#SourceDir}\{#AppExe}"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\LICENSE"; DestDir: "{app}"; DestName: "LICENSE.txt"; Flags: ignoreversion
Source: "..\THIRD_PARTY_NOTICES.md"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\{#AppName}"; Filename: "{app}\{#AppExe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExe}"; Tasks: desktopicon

[Registry]
Root: HKA; Subkey: "Software\Classes\.hmy"; ValueType: string; ValueName: ""; ValueData: "YannosayHarmony.Project"; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\YannosayHarmony.Project"; ValueType: string; ValueName: ""; ValueData: "Harmony Project"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\YannosayHarmony.Project\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#AppExe},0"; Tasks: associate
Root: HKA; Subkey: "Software\Classes\YannosayHarmony.Project\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#AppExe}"" ""%1"""; Tasks: associate

[Run]
Filename: "{app}\{#AppExe}"; Description: "{cm:LaunchProgram,{#AppName}}"; Flags: nowait postinstall skipifsilent
Filename: "{app}\{#AppExe}"; Flags: nowait; Check: ShouldRelaunch

[Code]
function ShouldRelaunch: Boolean;
var
  i: Integer;
begin
  Result := False;
  for i := 1 to ParamCount do
    if CompareText(ParamStr(i), '/RELAUNCH') = 0 then
      Result := True;
end;
