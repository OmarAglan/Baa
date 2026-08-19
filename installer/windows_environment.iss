const
  ECO_ENV_SYSTEM_SUBKEY = 'SYSTEM\CurrentControlSet\Control\Session Manager\Environment';
  ECO_ENV_USER_SUBKEY = 'Environment';
  ECO_HWND_BROADCAST = $FFFF;
  ECO_WM_SETTINGCHANGE = $001A;
  ECO_SMTO_ABORTIFHUNG = $0002;

function EcoSendMessageTimeout(hWnd: Integer; Msg: Integer; wParam: Integer;
  lParam: string; fuFlags, uTimeout: Integer; var lpdwResult: Integer): Integer;
  external 'SendMessageTimeoutW@user32.dll stdcall';

function EcoNormalizePath(const Value: string): string;
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

function EcoPathContains(const PathValue: string; const Directory: string): Boolean;
var
  Items: TArrayOfString;
  Item, Wanted: string;
  Index: Integer;
begin
  Result := False;
  Wanted := EcoNormalizePath(Directory);
  Items := StringSplit(PathValue, [';'], stExcludeEmpty);
  for Index := 0 to GetArrayLength(Items) - 1 do
  begin
    Item := Items[Index];
    if (Trim(Item) = Trim(Directory)) or
       (EcoNormalizePath(Item) = Wanted) then
    begin
      Result := True;
      Exit;
    end;
  end;
end;

function EcoAppendPath(const PathValue: string; const Directory: string): string;
begin
  if (Trim(Directory) = '') or EcoPathContains(PathValue, Directory) then
    Result := PathValue
  else if Trim(PathValue) = '' then
    Result := Directory
  else if PathValue[Length(PathValue)] = ';' then
    Result := PathValue + Directory
  else
    Result := PathValue + ';' + Directory;
end;

function EcoRemovePath(const PathValue: string; const Directory: string): string;
var
  Items, KeptItems: TArrayOfString;
  Item, Wanted: string;
  Index, KeptCount: Integer;
begin
  Wanted := EcoNormalizePath(Directory);
  Items := StringSplit(PathValue, [';'], stExcludeEmpty);
  KeptCount := 0;
  SetArrayLength(KeptItems, GetArrayLength(Items));
  for Index := 0 to GetArrayLength(Items) - 1 do
  begin
    Item := Items[Index];
    Item := Trim(Item);
    if (Item <> '') and
       (Trim(Item) <> Trim(Directory)) and
       (EcoNormalizePath(Item) <> Wanted) then
    begin
      KeptItems[KeptCount] := Item;
      KeptCount := KeptCount + 1;
    end;
  end;
  SetArrayLength(KeptItems, KeptCount);
  Result := StringJoin(';', KeptItems);
end;

procedure EcoEnvironmentRoot(var Root: Integer; var Subkey: string);
begin
  if IsAdminInstallMode then
  begin
    Root := HKLM;
    Subkey := ECO_ENV_SYSTEM_SUBKEY;
  end
  else
  begin
    Root := HKCU;
    Subkey := ECO_ENV_USER_SUBKEY;
  end;
end;

function EcoEnsurePathContains(const Directory: string): Boolean;
var
  Root: Integer;
  Subkey, CurrentValue, UpdatedValue: string;
begin
  Result := False;
  EcoEnvironmentRoot(Root, Subkey);
  CurrentValue := '';
  RegQueryStringValue(Root, Subkey, 'Path', CurrentValue);
  UpdatedValue := EcoAppendPath(CurrentValue, Directory);
  if UpdatedValue <> CurrentValue then
  begin
    RegWriteExpandStringValue(Root, Subkey, 'Path', UpdatedValue);
    Result := True;
  end;
end;

function EcoEnsurePathRemoved(const Directory: string): Boolean;
var
  Root: Integer;
  Subkey, CurrentValue, UpdatedValue: string;
begin
  Result := False;
  EcoEnvironmentRoot(Root, Subkey);
  CurrentValue := '';
  if not RegQueryStringValue(Root, Subkey, 'Path', CurrentValue) then
    Exit;
  UpdatedValue := EcoRemovePath(CurrentValue, Directory);
  if UpdatedValue <> CurrentValue then
  begin
    RegWriteExpandStringValue(Root, Subkey, 'Path', UpdatedValue);
    Result := True;
  end;
end;

procedure EcoSetOwnedEnvironment(const Name: string; const Value: string);
var
  Root: Integer;
  Subkey: string;
begin
  EcoEnvironmentRoot(Root, Subkey);
  RegWriteExpandStringValue(Root, Subkey, Name, Value);
end;

procedure EcoDeleteOwnedEnvironment(const Name: string; const ExpectedValue: string);
var
  Root: Integer;
  Subkey, CurrentValue: string;
begin
  EcoEnvironmentRoot(Root, Subkey);
  if RegQueryStringValue(Root, Subkey, Name, CurrentValue) and
     (EcoNormalizePath(CurrentValue) = EcoNormalizePath(ExpectedValue)) then
    RegDeleteValue(Root, Subkey, Name);
end;

procedure EcoBroadcastEnvironmentChange;
var
  MessageResult: Integer;
begin
  EcoSendMessageTimeout(ECO_HWND_BROADCAST, ECO_WM_SETTINGCHANGE, 0,
    'Environment', ECO_SMTO_ABORTIFHUNG, 5000, MessageResult);
end;
