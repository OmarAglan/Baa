// Shared migration guard for one product that may have been installed once per
// machine and once per user. A normal all-users upgrade removes the current
// user's legacy copy before writing the machine-wide registration.

function EcoMigrateOppositeInstall(const ProductName,
  UninstallKey: string): string;
var
  OtherRoot: Integer;
  UninstallCommand, Uninstaller: string;
  ExitCode: Integer;
begin
  Result := '';
  ExitCode := -1;
  if IsAdminInstallMode then
    OtherRoot := HKCU
  else
    OtherRoot := HKLM;

  if not RegKeyExists(OtherRoot, UninstallKey) then
    Exit;

  if not IsAdminInstallMode then
  begin
    Result := 'يوجد تثبيت سابق لكل المستخدمين من ' + ProductName +
      '. شغّل المثبت بصلاحية المدير للترقية بدل إنشاء نسخة ثانية.';
    Exit;
  end;

  if not RegQueryStringValue(OtherRoot, UninstallKey,
    'UninstallString', UninstallCommand) then
  begin
    Log('Removing stale opposite-scope uninstall registration for ' +
      ProductName + '.');
    RegDeleteKeyIncludingSubkeys(OtherRoot, UninstallKey);
    Exit;
  end;

  Uninstaller := RemoveQuotes(Trim(UninstallCommand));
  if not FileExists(Uninstaller) then
  begin
    Log('Removing orphaned opposite-scope uninstall registration for ' +
      ProductName + ': ' + Uninstaller);
    RegDeleteKeyIncludingSubkeys(OtherRoot, UninstallKey);
    Exit;
  end;

  Log('Removing opposite-scope installation for ' + ProductName + ': ' +
    Uninstaller);
  if not Exec(Uninstaller,
    '/VERYSILENT /SUPPRESSMSGBOXES /NORESTART',
    ExtractFileDir(Uninstaller), SW_HIDE, ewWaitUntilTerminated, ExitCode) or
    (ExitCode <> 0) then
  begin
    Result := 'تعذر إزالة التثبيت القديم من ' + ProductName +
      ' (رمز الخروج ' + IntToStr(ExitCode) +
      '). أزله من قائمة التطبيقات ثم أعد المحاولة.';
    Exit;
  end;

  if RegKeyExists(OtherRoot, UninstallKey) then
    Result := 'بقي قيد تثبيت قديم من ' + ProductName +
      ' بعد تشغيل الإزالة. أعد تشغيل النظام ثم حاول مرة أخرى.';
end;
