// Shared presentation only. Each installer retains its own installation logic.
// Canonical copy: Baa-Developer-Kit/installer/windows_wizard.iss

var
  EcoLogMemo: TNewMemo;
  EcoLogHeading: TNewStaticText;
  EcoOpenLogButton: TNewButton;
  EcoLastProgress: Integer;
  EcoDetailLogPath: string;

function EcoText(const ArabicText, EnglishText: string): string;
begin
  if ActiveLanguage = 'arabic' then Result := ArabicText
  else Result := EnglishText;
end;

procedure EcoWizardLog(const Level, MessageText: string);
var
  LevelText: string;
begin
  Log('[' + Level + '] ' + MessageText);
  if Level = 'error' then LevelText := EcoText('خطأ', 'ERROR')
  else if Level = 'success' then LevelText := EcoText('تم', 'OK')
  else LevelText := EcoText('جاري', 'INFO');
  if EcoLogMemo <> nil then
  begin
    EcoLogMemo.Lines.Add(GetDateTimeString('hh:nn:ss', ':', ':') +
      '  [' + LevelText + ']  ' + MessageText);
    while EcoLogMemo.Lines.Count > 250 do EcoLogMemo.Lines.Delete(0);
    EcoLogMemo.SelStart := Length(EcoLogMemo.Text);
    EcoLogMemo.SelLength := 0;
    SendMessage(EcoLogMemo.Handle, $00B7, 0, 0);
  end;
end;

procedure EcoOpenLog(Sender: TObject);
var
  LogPath: string;
  ErrorCode: Integer;
begin
  LogPath := EcoDetailLogPath;
  if (LogPath = '') or not FileExists(LogPath) then
    LogPath := ExpandConstant('{log}');
  if (LogPath <> '') and FileExists(LogPath) then
  begin
    if not ShellExec('open', LogPath, '', '', SW_SHOWNORMAL,
      ewNoWait, ErrorCode) then
      MsgBox(EcoText('تعذر فتح السجل: ', 'Could not open log: ') + LogPath,
        mbError, MB_OK);
  end
  else
    MsgBox(EcoText('لم يُنشأ ملف السجل بعد.', 'The log file is not available yet.'),
      mbInformation, MB_OK);
end;

procedure EcoInstallFailed(const MessageText: string);
begin
  EcoWizardLog('error', MessageText);
  RaiseException(MessageText);
end;

<event('CurStepChanged')>
procedure EcoWizardStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then
    EcoWizardLog('info', EcoText('إعداد البيئة والتحقق من جاهزية البرنامج.',
      'Configuring the environment and checking the application.'));
end;

<event('InitializeWizard')>
procedure EcoInitializeWizard;
begin
  EcoLastProgress := -1;
  WizardForm.WelcomeLabel1.Font.Size := 20;
  WizardForm.AdjustLabelHeight(WizardForm.WelcomeLabel1);
  WizardForm.WelcomeLabel2.Top := WizardForm.WelcomeLabel1.Top +
    WizardForm.WelcomeLabel1.Height + ScaleY(18);
  WizardForm.WelcomeLabel2.Height := WizardForm.WelcomePage.Height -
    WizardForm.WelcomeLabel2.Top - ScaleY(24);
  WizardForm.FinishedHeadingLabel.Font.Size := 18;
  WizardForm.AdjustLabelHeight(WizardForm.FinishedHeadingLabel);
  WizardForm.FinishedLabel.Top := WizardForm.FinishedHeadingLabel.Top +
    WizardForm.FinishedHeadingLabel.Height + ScaleY(18);
  WizardForm.FinishedLabel.Height := WizardForm.FinishedPage.Height -
    WizardForm.FinishedLabel.Top - ScaleY(80);

  EcoOpenLogButton := TNewButton.Create(WizardForm);
  EcoOpenLogButton.Parent := WizardForm;
  EcoOpenLogButton.SetBounds(ScaleX(12), WizardForm.NextButton.Top,
    ScaleX(100), WizardForm.NextButton.Height);
  EcoOpenLogButton.Caption := EcoText('فتح السجل', 'Open log');
  EcoOpenLogButton.OnClick := @EcoOpenLog;
  EcoOpenLogButton.Visible := False;

  EcoLogHeading := TNewStaticText.Create(WizardForm);
  EcoLogHeading.Parent := WizardForm.InstallingPage;
  EcoLogHeading.AutoSize := False;
  EcoLogHeading.SetBounds(WizardForm.ProgressGauge.Left,
    WizardForm.ProgressGauge.Top + WizardForm.ProgressGauge.Height + ScaleY(18),
    WizardForm.ProgressGauge.Width, ScaleY(22));
  EcoLogHeading.Caption := EcoText('تفاصيل التثبيت', 'Installation details');
  EcoLogHeading.Font.Style := [fsBold];

  EcoLogMemo := TNewMemo.Create(WizardForm);
  EcoLogMemo.Parent := WizardForm.InstallingPage;
  EcoLogMemo.SetBounds(EcoLogHeading.Left, EcoLogHeading.Top + ScaleY(26),
    WizardForm.ProgressGauge.Width,
    WizardForm.InstallingPage.ClientHeight - EcoLogHeading.Top - ScaleY(34));
  EcoLogMemo.ReadOnly := True;
  EcoLogMemo.WordWrap := True;
  EcoLogMemo.ScrollBars := ssVertical;
  EcoLogMemo.Font.Name := 'Segoe UI';
  EcoLogMemo.Font.Size := 10;
  EcoWizardLog('info', EcoText('جاهز لبدء التثبيت.', 'Ready to install.'));
end;

<event('CurPageChanged')>
procedure EcoWizardPageChanged(CurPageID: Integer);
begin
  EcoOpenLogButton.Visible := (CurPageID = wpInstalling) or (CurPageID = wpFinished);
  if CurPageID = wpInstalling then
    EcoWizardLog('info', EcoText('نسخ الملفات وإعداد البرنامج.',
      'Copying files and configuring the application.'));
  if CurPageID = wpFinished then
    EcoWizardLog('success', EcoText('اكتمل التثبيت. يمكنك فتح السجل الكامل.',
      'Installation complete. The full log is available.'));
end;

<event('CurInstallProgressChanged')>
procedure EcoWizardProgress(CurProgress, MaxProgress: Integer);
var
  Percent: Integer;
begin
  if MaxProgress <= 0 then Exit;
  Percent := Round(100.0 * CurProgress / MaxProgress);
  if (Percent div 10) <> EcoLastProgress then
  begin
    EcoLastProgress := Percent div 10;
    EcoWizardLog('info', EcoText('تقدم نسخ الملفات: ', 'File progress: ') +
      IntToStr(Percent) + '%');
  end;
end;
