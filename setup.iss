; -- Baa Compiler Setup Script (Inno Setup)
; -- Version 0.3.2.4
;
; Build instructions:
;   1. Place a MinGW-w64 GCC distribution in a folder called "gcc/"
;      at the project root (next to this file).  Only the "bin/" and
;      "lib/" (and optionally "libexec/") subdirectories are needed.
;      You can use the helper script:  scripts\prepare_gcc_bundle.ps1
;   2. Build baa.exe with CMake (cmake --build build).
;   3. Compile this script with Inno Setup 6+.

[Setup]
; Product Name
AppName=Baa Compiler
AppVersion=0.3.2.4
; Publisher Name
AppPublisher=Omar Aglan
AppPublisherURL=https://github.com/OmarAglan/Baa
AppSupportURL=https://github.com/OmarAglan/Baa
AppUpdatesURL=https://github.com/OmarAglan/Baa

; Default installation directory
DefaultDirName={autopf}\Baa Compiler
DefaultGroupName=Baa Compiler
AllowNoIcons=yes
; Output installer filename
OutputBaseFilename=baa_setup
; Ensure this path is correct
SetupIconFile=resources\icon.ico
Compression=lzma
SolidCompression=yes
WizardStyle=modern

; Uninstaller information
UninstallDisplayIcon={app}\baa.exe
UninstallDisplayName=Baa Compiler 0.3.2.4
PrivilegesRequired=admin

[Files]
; Baa compiler
Source: "build\baa.exe"; DestDir: "{app}"; Flags: ignoreversion

; Bundled MinGW-w64 GCC toolchain
; The gcc\ folder should contain at minimum: bin\gcc.exe, bin\as.exe, bin\ld.exe
; plus the supporting libs and libexec needed by GCC.
Source: "gcc\*"; DestDir: "{app}\gcc"; Flags: ignoreversion recursesubdirs createallsubdirs

; Resources, docs, metadata
Source: "resources\*"; DestDir: "{app}\resources"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "README.md"; DestDir: "{app}"; Flags: ignoreversion
;Source: "LICENSE.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "CHANGELOG.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "docs\*"; DestDir: "{app}\docs"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\Baa Compiler"; Filename: "{app}\baa.exe"
Name: "{group}\Documentation"; Filename: "{app}\docs\USER_GUIDE.md"
Name: "{group}\{cm:UninstallProgram,Baa Compiler}"; Filename: "{uninstallexe}"

[Registry]
; Add Baa and the bundled GCC to the system PATH
Root: HKLM; Subkey: "SYSTEM\CurrentControlSet\Control\Session Manager\Environment"; ValueType: expandsz; ValueName: "Path"; ValueData: "{olddata};{app}"; Check: Not PathContainsDir(ExpandConstant('{app}'))
Root: HKLM; Subkey: "SYSTEM\CurrentControlSet\Control\Session Manager\Environment"; ValueType: expandsz; ValueName: "Path"; ValueData: "{olddata};{app}\gcc\bin"; Check: Not PathContainsDir(ExpandConstant('{app}\gcc\bin'))

[Run]
; Open documentation (Optional)
Filename: "{app}\docs\USER_GUIDE.md"; Description: "Open Baa User Guide"; Flags: postinstall shellexec skipifsilent unchecked

[Code]
// Check if a directory already exists in the system PATH
function PathContainsDir(Dir: string): Boolean;
var
  PathValue: string;
begin
  Result := False;
  if not RegQueryStringValue(HKLM,
       'SYSTEM\CurrentControlSet\Control\Session Manager\Environment',
       'Path', PathValue) then
    Exit;
  // Case-insensitive search
  if Pos(Lowercase(Dir), Lowercase(PathValue)) > 0 then
    Result := True;
end;

// Verify that gcc.exe was actually deployed
function GccBundlePresent: Boolean;
begin
  Result := FileExists(ExpandConstant('{app}\gcc\bin\gcc.exe'));
end;

procedure CurStepChanged(CurStep: TSetupStep);
var
  Msg: string;
begin
  if CurStep = ssPostInstall then
  begin
    if GccBundlePresent then
    begin
      Msg := 'Baa Compiler installed successfully!' + #13#13
             + 'A bundled GCC toolchain has been installed to:' + #13
             + ExpandConstant('{app}\gcc') + #13#13
             + 'Please restart your terminal for the PATH changes to take effect.';
      MsgBox(Msg, mbInformation, MB_OK);
    end
    else
    begin
      Msg := 'Baa Compiler installed, but the GCC toolchain was NOT found!' + #13#13
             + 'The compiler requires MinGW-w64 GCC to assemble and link programs.' + #13
             + 'Please install MinGW-w64 manually and ensure gcc.exe is on your PATH.';
      MsgBox(Msg, mbError, MB_OK);
    end;
  end;
end;