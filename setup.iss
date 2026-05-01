; -- Baa Compiler Setup Script (Inno Setup)
; -- Version 0.5.4
;
; Build instructions:
;   1. Place a MinGW-w64 GCC distribution in a folder called "gcc/"
;      at the project root (next to this file). Only "bin/" and "lib/"
;      (and optionally "libexec/") are required.
;   2. Build baa.exe with CMake (cmake --build build).
;   3. Compile this script with Inno Setup 6+.

#define MyAppId "{{E4B6D77C-6C22-4E2D-8F9D-61D34A26B0D1}"
#define MyAppName "Baa Compiler"
#define MyAppVersion "0.5.4"
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

DefaultDirName={autopf}\Baa Compiler
DefaultGroupName=Baa Compiler
AllowNoIcons=yes
OutputBaseFilename=baa_setup_{#MyAppVersion}_x64
SetupIconFile=resources\icon.ico
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern

ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
ChangesEnvironment=yes
SetupLogging=yes
UsePreviousAppDir=yes
UsePreviousGroup=yes
UsePreviousLanguage=yes
UsePreviousTasks=yes

UninstallDisplayIcon={app}\{#MyAppExeName}
UninstallDisplayName={#MyAppName} {#MyAppVersion}
PrivilegesRequired=admin

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "arabic"; MessagesFile: "compiler:Languages\Arabic.isl"

[Tasks]
Name: "path_baa"; Description: "Add Baa compiler to PATH"; GroupDescription: "Environment:"; Flags: checkedonce
Name: "path_gcc"; Description: "Add bundled GCC toolchain to PATH"; GroupDescription: "Environment:"; Flags: checkedonce
Name: "env_vars"; Description: "Set BAA_HOME and BAA_STDLIB variables"; GroupDescription: "Environment:"; Flags: checkedonce
Name: "assoc_baa"; Description: "Associate .baa/.baahd files"; GroupDescription: "Integration:"; Flags: unchecked
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
; Compiler binaries
Source: "build\baa.exe"; DestDir: "{app}"; Flags: ignoreversion

; Bundled MinGW-w64 GCC toolchain
Source: "gcc\*"; DestDir: "{app}\gcc"; Flags: ignoreversion recursesubdirs createallsubdirs

; Bundled standard library
Source: "stdlib\*"; DestDir: "{app}\stdlib"; Flags: ignoreversion recursesubdirs createallsubdirs

; Developer resources
Source: "examples\*"; DestDir: "{app}\examples"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "resources\*"; DestDir: "{app}\resources"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "docs\*"; DestDir: "{app}\docs"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "CHANGELOG.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "ROADMAP.md"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\Baa Compiler"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\User Guide"; Filename: "{app}\docs\USER_GUIDE.md"
Name: "{group}\Examples Folder"; Filename: "{app}\examples"
Name: "{group}\Changelog"; Filename: "{app}\CHANGELOG.md"
Name: "{group}\{cm:UninstallProgram,Baa Compiler}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\Baa Compiler"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Registry]
; File associations (optional)
Root: HKLM; Subkey: "Software\Classes\.baa"; ValueType: string; ValueName: ""; ValueData: "Baa.Source"; Tasks: assoc_baa; Flags: uninsdeletevalue
Root: HKLM; Subkey: "Software\Classes\.baahd"; ValueType: string; ValueName: ""; ValueData: "Baa.Header"; Tasks: assoc_baa; Flags: uninsdeletevalue

Root: HKLM; Subkey: "Software\Classes\Baa.Source"; ValueType: string; ValueName: ""; ValueData: "Baa Source File"; Tasks: assoc_baa; Flags: uninsdeletekey
Root: HKLM; Subkey: "Software\Classes\Baa.Source\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: assoc_baa; Flags: uninsdeletekey
Root: HKLM; Subkey: "Software\Classes\Baa.Source\shell\open"; ValueType: string; ValueName: ""; ValueData: "Edit"; Tasks: assoc_baa; Flags: uninsdeletekey
Root: HKLM; Subkey: "Software\Classes\Baa.Source\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """notepad.exe"" ""%1"""; Tasks: assoc_baa; Flags: uninsdeletekey
Root: HKLM; Subkey: "Software\Classes\Baa.Source\shell\compile"; ValueType: string; ValueName: ""; ValueData: "Compile with Baa"; Tasks: assoc_baa; Flags: uninsdeletekey
Root: HKLM; Subkey: "Software\Classes\Baa.Source\shell\compile\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: assoc_baa; Flags: uninsdeletekey

Root: HKLM; Subkey: "Software\Classes\Baa.Header"; ValueType: string; ValueName: ""; ValueData: "Baa Header File"; Tasks: assoc_baa; Flags: uninsdeletekey
Root: HKLM; Subkey: "Software\Classes\Baa.Header\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: assoc_baa; Flags: uninsdeletekey
Root: HKLM; Subkey: "Software\Classes\Baa.Header\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """notepad.exe"" ""%1"""; Tasks: assoc_baa; Flags: uninsdeletekey

[Run]
Filename: "{app}\docs\USER_GUIDE.md"; Description: "Open Baa User Guide"; Flags: postinstall shellexec skipifsilent unchecked
Filename: "{app}\examples"; Description: "Open Examples Folder"; Flags: postinstall shellexec skipifsilent unchecked

[Code]
const
  ENV_SUBKEY_SYSTEM = 'SYSTEM\CurrentControlSet\Control\Session Manager\Environment';
  ENV_SUBKEY_USER = 'Environment';
  BAA_HWND_BROADCAST = $FFFF;
  BAA_WM_SETTINGCHANGE = $001A;
  BAA_SMTO_ABORTIFHUNG = $0002;

function BaaSendMessageTimeout(hWnd: Integer; Msg: Integer; wParam: Integer; lParam: string;
  fuFlags, uTimeout: Integer; var lpdwResult: Integer): Integer;
  external 'SendMessageTimeoutW@user32.dll stdcall';

function NormalizePathForCompare(const Value: string): string;
var
  S: string;
begin
  S := Trim(Value);
  StringChangeEx(S, '"', '', True);
  StringChangeEx(S, '/', '\', True);
  while (Length(S) > 0) and (S[Length(S)] = '\') do
    Delete(S, Length(S), 1);
  Result := Lowercase(S);
end;

function PathListContainsDir(const PathValue: string; const Dir: string): Boolean;
var
  Remaining, Item, Wanted: string;
  SepPos: Integer;
begin
  Result := False;
  Wanted := NormalizePathForCompare(Dir);
  Remaining := PathValue;

  while Remaining <> '' do
  begin
    SepPos := Pos(';', Remaining);
    if SepPos > 0 then
    begin
      Item := Copy(Remaining, 1, SepPos - 1);
      Delete(Remaining, 1, SepPos);
    end
    else
    begin
      Item := Remaining;
      Remaining := '';
    end;

    if NormalizePathForCompare(Item) = Wanted then
    begin
      Result := True;
      Exit;
    end;
  end;
end;

function PathListAppendDir(const PathValue: string; const Dir: string): string;
begin
  if Trim(Dir) = '' then
  begin
    Result := PathValue;
    Exit;
  end;

  if PathListContainsDir(PathValue, Dir) then
  begin
    Result := PathValue;
    Exit;
  end;

  if Trim(PathValue) = '' then
    Result := Dir
  else
    Result := PathValue + ';' + Dir;
end;

function PathListRemoveDir(const PathValue: string; const Dir: string): string;
var
  Remaining, Item, OutValue, Wanted: string;
  SepPos: Integer;
begin
  OutValue := '';
  Wanted := NormalizePathForCompare(Dir);
  Remaining := PathValue;

  while Remaining <> '' do
  begin
    SepPos := Pos(';', Remaining);
    if SepPos > 0 then
    begin
      Item := Copy(Remaining, 1, SepPos - 1);
      Delete(Remaining, 1, SepPos);
    end
    else
    begin
      Item := Remaining;
      Remaining := '';
    end;

    Item := Trim(Item);
    if (Item <> '') and (NormalizePathForCompare(Item) <> Wanted) then
    begin
      if OutValue = '' then
        OutValue := Item
      else
        OutValue := OutValue + ';' + Item;
    end;
  end;

  Result := OutValue;
end;

procedure EnsurePathContains(Root: Integer; const Subkey: string; const Dir: string);
var
  CurrentPath, UpdatedPath: string;
begin
  CurrentPath := '';
  RegQueryStringValue(Root, Subkey, 'Path', CurrentPath);
  UpdatedPath := PathListAppendDir(CurrentPath, Dir);
  if UpdatedPath <> CurrentPath then
    RegWriteExpandStringValue(Root, Subkey, 'Path', UpdatedPath);
end;

procedure EnsurePathRemoved(Root: Integer; const Subkey: string; const Dir: string);
var
  CurrentPath, UpdatedPath: string;
begin
  CurrentPath := '';
  if not RegQueryStringValue(Root, Subkey, 'Path', CurrentPath) then
    Exit;

  UpdatedPath := PathListRemoveDir(CurrentPath, Dir);
  if UpdatedPath <> CurrentPath then
    RegWriteExpandStringValue(Root, Subkey, 'Path', UpdatedPath);
end;

procedure SetEnvVar(Root: Integer; const Subkey: string; const Name: string; const Value: string);
begin
  RegWriteExpandStringValue(Root, Subkey, Name, Value);
end;

procedure DeleteEnvVarIfMatches(Root: Integer; const Subkey: string; const Name: string; const ExpectedValue: string);
var
  CurrentValue: string;
begin
  if not RegQueryStringValue(Root, Subkey, Name, CurrentValue) then
    Exit;

  if NormalizePathForCompare(CurrentValue) = NormalizePathForCompare(ExpectedValue) then
    RegDeleteValue(Root, Subkey, Name);
end;

procedure BroadcastEnvironmentChange;
var
  MsgResult: Integer;
begin
  BaaSendMessageTimeout(BAA_HWND_BROADCAST, BAA_WM_SETTINGCHANGE, 0, 'Environment',
    BAA_SMTO_ABORTIFHUNG, 5000, MsgResult);
end;

function CompilerPresent: Boolean;
begin
  Result := FileExists(ExpandConstant('{app}\{#MyAppExeName}'));
end;

function GccBundlePresent: Boolean;
begin
  Result :=
    FileExists(ExpandConstant('{app}\gcc\bin\gcc.exe')) and
    FileExists(ExpandConstant('{app}\gcc\bin\as.exe')) and
    FileExists(ExpandConstant('{app}\gcc\bin\ld.exe'));
end;

function StdlibBundlePresent: Boolean;
begin
  Result := FileExists(ExpandConstant('{app}\stdlib\baalib.baahd'));
end;

procedure ApplyInstallEnvironment;
begin
  if WizardIsTaskSelected('path_baa') then
    EnsurePathContains(HKLM, ENV_SUBKEY_SYSTEM, ExpandConstant('{app}'));

  if WizardIsTaskSelected('path_gcc') then
    EnsurePathContains(HKLM, ENV_SUBKEY_SYSTEM, ExpandConstant('{app}\gcc\bin'));

  if WizardIsTaskSelected('env_vars') then
  begin
    SetEnvVar(HKLM, ENV_SUBKEY_SYSTEM, 'BAA_HOME', ExpandConstant('{app}'));
    SetEnvVar(HKLM, ENV_SUBKEY_SYSTEM, 'BAA_STDLIB', ExpandConstant('{app}\stdlib'));
  end;
end;

procedure CleanupUninstallEnvironment;
begin
  EnsurePathRemoved(HKLM, ENV_SUBKEY_SYSTEM, ExpandConstant('{app}'));
  EnsurePathRemoved(HKLM, ENV_SUBKEY_SYSTEM, ExpandConstant('{app}\gcc\bin'));
  EnsurePathRemoved(HKCU, ENV_SUBKEY_USER, ExpandConstant('{app}'));
  EnsurePathRemoved(HKCU, ENV_SUBKEY_USER, ExpandConstant('{app}\gcc\bin'));

  DeleteEnvVarIfMatches(HKLM, ENV_SUBKEY_SYSTEM, 'BAA_HOME', ExpandConstant('{app}'));
  DeleteEnvVarIfMatches(HKLM, ENV_SUBKEY_SYSTEM, 'BAA_STDLIB', ExpandConstant('{app}\stdlib'));
  DeleteEnvVarIfMatches(HKCU, ENV_SUBKEY_USER, 'BAA_HOME', ExpandConstant('{app}'));
  DeleteEnvVarIfMatches(HKCU, ENV_SUBKEY_USER, 'BAA_STDLIB', ExpandConstant('{app}\stdlib'));
end;

procedure ShowInstallSummary;
var
  Msg: string;
begin
  Msg :=
    'Baa Compiler installed successfully!' + #13#13 +
    'Compiler: ' + ExpandConstant('{app}\{#MyAppExeName}') + #13 +
    'Toolchain: ' + ExpandConstant('{app}\gcc') + #13 +
    'Stdlib: ' + ExpandConstant('{app}\stdlib') + #13#13;

  if WizardIsTaskSelected('path_baa') or
     WizardIsTaskSelected('path_gcc') or
     WizardIsTaskSelected('env_vars') then
  begin
    Msg := Msg +
      'Environment variables were updated. Open a new terminal session to use the new settings.' + #13#13;
  end;

  if not CompilerPresent or not GccBundlePresent or not StdlibBundlePresent then
  begin
    Msg := Msg +
      'Warning: One or more expected components are missing. Re-run installer as Administrator or verify package integrity.';
    MsgBox(Msg, mbError, MB_OK);
  end
  else
  begin
    MsgBox(Msg, mbInformation, MB_OK);
  end;
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then
  begin
    ApplyInstallEnvironment;
    BroadcastEnvironmentChange;
    ShowInstallSummary;
  end;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usPostUninstall then
  begin
    CleanupUninstallEnvironment;
    BroadcastEnvironmentChange;
  end;
end;
