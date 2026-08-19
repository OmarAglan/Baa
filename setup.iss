; مثبت مصرّف باء المستقل لويندوز.
; نظم اعتماد مستقل يحل من PATH. الرابط الخاص يبقى داخل مجلد باء ولا يدخل PATH.

#define MyAppId "{{E4B6D77C-6C22-4E2D-8F9D-61D34A26B0D1}"
#define MyAppName "مصرّف باء"
#ifndef MyAppVersion
  #define MyAppVersion "0.6.0"
#endif
#ifndef BaaBinaryDir
  #define BaaBinaryDir "build"
#endif
#ifndef BaaToolchainDir
  #define BaaToolchainDir "gcc"
#endif
#define MyAppPublisher "Omar Aglan"
#define MyAppURL "https://github.com/OmarAglan/Baa"
#define MyAppExeName "baa.exe"

[Setup]
AppId={#MyAppId}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={autopf}\Baa
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
AllowNoIcons=yes
OutputDir=dist\installer
OutputBaseFilename=baa-setup-{#MyAppVersion}-x64
SetupIconFile=resources\icon.ico
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
PrivilegesRequired=admin
PrivilegesRequiredOverridesAllowed=dialog commandline
ChangesEnvironment=yes
SetupLogging=yes
UsePreviousAppDir=yes
UsePreviousLanguage=yes
UninstallDisplayIcon={app}\{#MyAppExeName}
UninstallDisplayName={#MyAppName} {#MyAppVersion}

[Languages]
Name: "arabic"; MessagesFile: "compiler:Languages\Arabic.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "{#BaaBinaryDir}\baa.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#BaaBinaryDir}\libbaa_runtime.a"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#BaaToolchainDir}\*"; DestDir: "{app}\gcc"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "stdlib\*"; DestDir: "{app}\stdlib"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "examples\*"; DestDir: "{app}\examples"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "resources\*"; DestDir: "{app}\resources"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "docs\*"; DestDir: "{app}\docs"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "targets\*"; DestDir: "{app}\targets"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "CHANGELOG.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "ROADMAP.md"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\باء\دليل باء"; Filename: "{app}\docs\USER_GUIDE.md"
Name: "{autoprograms}\باء\أمثلة باء"; Filename: "{app}\examples"
Name: "{autoprograms}\باء\إزالة باء"; Filename: "{uninstallexe}"

[Run]
Filename: "{app}\{#MyAppExeName}"; Parameters: "--version"; Description: "التحقق من إصدار باء"; Flags: postinstall skipifsilent unchecked runhidden

[Code]
#include "installer\windows_environment.iss"

const
  BAA_INSTALLER_KEY = 'Software\BaaEcosystem\Baa';
  BAA_PATH_OWNED_VALUE = 'PathOwned';
  BAA_HOME_OWNED_VALUE = 'HomeOwned';
  BAA_STDLIB_OWNED_VALUE = 'StdlibOwned';
  NAZM_UNINSTALL_KEY = 'Software\Microsoft\Windows\CurrentVersion\Uninstall\{8D3D57AE-41CF-4B8A-95E9-270E4564E2A1}_is1';

procedure BaaRegistryRoot(var Root: Integer);
begin
  if IsAdminInstallMode then
    Root := HKLM
  else
    Root := HKCU;
end;

function BaaOwnedValue(const Name: string): Boolean;
var
  Root: Integer;
  Value: Cardinal;
begin
  BaaRegistryRoot(Root);
  Result := RegQueryDWordValue(Root, BAA_INSTALLER_KEY, Name, Value) and
    (Value = 1);
end;

procedure BaaSetOwnedValue(const Name: string; const Owned: Boolean);
var
  Root: Integer;
begin
  BaaRegistryRoot(Root);
  if Owned then
    RegWriteDWordValue(Root, BAA_INSTALLER_KEY, Name, 1)
  else
    RegDeleteValue(Root, BAA_INSTALLER_KEY, Name);
end;

procedure EnsureBaaOwnedEnvironment(const Name, Value, Marker: string);
var
  Root: Integer;
  Subkey, CurrentValue: string;
begin
  EcoEnvironmentRoot(Root, Subkey);
  CurrentValue := '';
  RegQueryStringValue(Root, Subkey, Name, CurrentValue);
  if (CurrentValue = '') or BaaOwnedValue(Marker) then
  begin
    EcoSetOwnedEnvironment(Name, Value);
    BaaSetOwnedValue(Marker, True);
  end;
end;

procedure ApplyBaaEnvironment;
begin
  if EcoEnsurePathContains(ExpandConstant('{app}')) then
    BaaSetOwnedValue(BAA_PATH_OWNED_VALUE, True);
  EnsureBaaOwnedEnvironment('BAA_HOME', ExpandConstant('{app}'),
    BAA_HOME_OWNED_VALUE);
  EnsureBaaOwnedEnvironment('BAA_STDLIB', ExpandConstant('{app}\stdlib'),
    BAA_STDLIB_OWNED_VALUE);
end;

function NazmAtInstallRoot(const Root: Integer): Boolean;
var
  InstallLocation: string;
begin
  Result := RegQueryStringValue(Root, NAZM_UNINSTALL_KEY, 'InstallLocation',
    InstallLocation) and
    FileExists(AddBackslash(InstallLocation) + 'bin\نظم.exe');
end;

function NazmAvailable: Boolean;
begin
  Result :=
    (FileSearch('نظم.exe', GetEnv('PATH')) <> '') or
    NazmAtInstallRoot(HKCU) or NazmAtInstallRoot(HKLM);
end;

function RunBaaHealthProbe: Boolean;
var
  ExitCode: Integer;
begin
  Result :=
    FileExists(ExpandConstant('{app}\{#MyAppExeName}')) and
    FileExists(ExpandConstant('{app}\libbaa_runtime.a')) and
    FileExists(ExpandConstant('{app}\stdlib\baalib.baahd')) and
    FileExists(ExpandConstant('{app}\gcc\bin\gcc.exe')) and
    FileExists(ExpandConstant('{app}\gcc\bin\ld.exe')) and
    FileExists(ExpandConstant('{app}\gcc\BAA-TOOLCHAIN-MANIFEST.txt')) and
    Exec(ExpandConstant('{app}\{#MyAppExeName}'), '--version',
      ExpandConstant('{app}'), SW_HIDE, ewWaitUntilTerminated, ExitCode) and
    (ExitCode = 0);
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then
  begin
    ApplyBaaEnvironment;
    EcoBroadcastEnvironmentChange;
    if not RunBaaHealthProbe then
      RaiseException('فشل فحص مكونات باء أو رابطه الخاص بعد التثبيت.');
    if not WizardSilent then
    begin
      if NazmAvailable then
        MsgBox('اكتمل تثبيت باء. افتح طرفية جديدة لاستخدام baa.',
          mbInformation, MB_OK)
      else
        MsgBox('اكتمل تثبيت باء، لكن نظم غير مثبت أو غير ظاهر في PATH. ثبّت نظم قبل بناء البرامج.',
          mbInformation, MB_OK);
    end;
  end;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  Root: Integer;
begin
  if CurUninstallStep = usPostUninstall then
  begin
    if BaaOwnedValue(BAA_PATH_OWNED_VALUE) then
      EcoEnsurePathRemoved(ExpandConstant('{app}'));
    if BaaOwnedValue(BAA_HOME_OWNED_VALUE) then
      EcoDeleteOwnedEnvironment('BAA_HOME', ExpandConstant('{app}'));
    if BaaOwnedValue(BAA_STDLIB_OWNED_VALUE) then
      EcoDeleteOwnedEnvironment('BAA_STDLIB', ExpandConstant('{app}\stdlib'));
    BaaRegistryRoot(Root);
    RegDeleteKeyIncludingSubkeys(Root, BAA_INSTALLER_KEY);
    EcoBroadcastEnvironmentChange;
  end;
end;
