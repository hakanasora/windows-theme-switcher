#ifndef BuildRoot
  #error Build with build.ps1 -Installer; BuildRoot is required.
#endif
#ifndef RepoRoot
  #error RepoRoot is required.
#endif
#ifndef DevelopmentBuild
  #define DevelopmentBuild 1
#endif
#ifndef AppVersion
  #error AppVersion is required.
#endif
#ifndef UnsignedPrototype
  #define UnsignedPrototype 0
#endif

[Setup]
AppId=桌面主题切换器 (Theme Switcher)
AppName=桌面主题切换器 (Theme Switcher)
AppVersion={#AppVersion}
#if DevelopmentBuild
AppVerName=Windows Theme Switcher {#AppVersion} 本机验证版
#else
AppVerName=Windows Theme Switcher 2.0
#endif
AppPublisher=Hakana Sora
SetupIconFile={#RepoRoot}\MyIcon.ico
ArchitecturesAllowed=x64os
; Keep the original 32-bit INSTALL MODE; payloads and COM registrations are x64.
MinVersion=10.0
PrivilegesRequired=admin
DefaultDirName={autopf}\ThemeSwitcher
DefaultGroupName=主题切换器
UsePreviousAppDir=yes
; A fresh log avoids retaining v1's generic uninsdeletekey actions. Old owned
; menu keys/components are migrated explicitly; the previous log is backed up.
UninstallLogMode=overwrite
UninstallDisplayIcon={app}\ThemeSwitcher.exe
DisableWelcomePage=yes
DisableDirPage=auto
DisableProgramGroupPage=yes
DisableReadyPage=yes
CloseApplications=no
RestartApplications=no
ChangesAssociations=yes
SolidCompression=yes
WizardStyle=modern
OutputBaseFilename=ThemeSwitcherSetup
VersionInfoVersion={#AppVersion}.0
SetupLogging=yes

[Files]
Source: "{#BuildRoot}\ThemeSwitcher.Deploy.exe"; Flags: dontcopy
Source: "{#BuildRoot}\components\{#AppVersion}\identity.msix"; Flags: dontcopy
Source: "{#BuildRoot}\ThemeSwitcher.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#BuildRoot}\components\{#AppVersion}\ThemeSwitcherShell.dll"; DestDir: "{app}\components\{#AppVersion}"; Flags: ignoreversion
Source: "{#BuildRoot}\components\{#AppVersion}\ThemeSwitcher.Deploy.exe"; DestDir: "{app}\components\{#AppVersion}"; Flags: ignoreversion
Source: "{#BuildRoot}\components\{#AppVersion}\ThemeSwitcher.MenuSetup.exe"; DestDir: "{app}\components\{#AppVersion}"; Flags: ignoreversion
Source: "{#BuildRoot}\components\{#AppVersion}\identity.msix"; DestDir: "{app}\components\{#AppVersion}"; Flags: ignoreversion; Check: ModernWindows
Source: "{#BuildRoot}\components\{#AppVersion}\owner.txt"; DestDir: "{app}\components\{#AppVersion}"; Flags: ignoreversion
Source: "{#RepoRoot}\assets\windows-moon.png"; DestDir: "{app}\assets"; Flags: ignoreversion
Source: "{#RepoRoot}\LICENSE"; DestDir: "{app}"; Flags: ignoreversion

[UninstallDelete]
Type: files; Name: "{app}\install-state.ini"
Type: dirifempty; Name: "{app}\components\{#AppVersion}"
Type: dirifempty; Name: "{app}\components"
Type: dirifempty; Name: "{app}\assets"

[Code]
var
  Helper, BackupDir: String;
  BackedUp, DeploymentAttempted, Committed, RestoreFailed: Boolean;
  OldDisplayName, OldDisplayVersion: String;
  HadOldEntry: Boolean;

function ModernWindows: Boolean;
var Version: TWindowsVersion;
begin
  GetWindowsVersionEx(Version);
  Result := (Version.Major >= 10) and (Version.Build >= 22621);
end;

function Quote(const Value: String): String;
begin
  Result := '"' + Value + '"';
end;

function RunDeploy(const Arguments: String; var Detail: String): Boolean;
var Code: Integer; ErrorFile: String;
begin
  ErrorFile := ExpandConstant('{tmp}\theme-switcher-deployment-error.txt');
  DeleteFile(ErrorFile);
  Result := Exec(Helper, Arguments + ' --error-file ' + Quote(ErrorFile), '', SW_HIDE, ewWaitUntilTerminated, Code);
  Result := Result and (Code = 0);
  if not Result then begin
    Detail := '菜单部署未完成。错误代码：' + IntToStr(Code);
    if FileExists(ErrorFile) then Detail := GetIniString('Error', 'Message', Detail, ErrorFile);
    Log(Detail);
  end;
end;

procedure BackupFile(const Relative: String);
var Source, Target: String;
begin
  Source := ExpandConstant('{app}\') + Relative;
  Target := BackupDir + '\' + Relative;
  if FileExists(Source) then begin
    ForceDirectories(ExtractFileDir(Target));
    if not CopyFile(Source, Target, False) then RaiseException('无法备份旧版本文件：' + Relative);
  end;
end;

procedure RestoreFile(const Relative: String);
var Source, Target: String;
begin
  Source := BackupDir + '\' + Relative;
  Target := ExpandConstant('{app}\') + Relative;
  if FileExists(Source) then begin
    ForceDirectories(ExtractFileDir(Target));
    if not CopyFile(Source, Target, False) then begin
      RestoreFailed := True;
      Log('恢复旧文件失败：' + Relative);
    end;
  end;
end;

procedure BackupOldComponents;
var FindRec: TFindRec; Relative: String; Marker: AnsiString;
begin
  if FindFirst(ExpandConstant('{app}\components\*'), FindRec) then begin
    try
      repeat
        if ((FindRec.Attributes and FILE_ATTRIBUTE_DIRECTORY) <> 0) and (FindRec.Name <> '.') and (FindRec.Name <> '..') then begin
          Relative := 'components\' + FindRec.Name;
          if LoadStringFromFile(ExpandConstant('{app}\') + Relative + '\owner.txt', Marker) and (Trim(Marker) = 'HakanaSora.ThemeSwitcher') then begin
            BackupFile(Relative + '\owner.txt');
            BackupFile(Relative + '\ThemeSwitcherShell.dll');
            BackupFile(Relative + '\ThemeSwitcher.Deploy.exe');
            BackupFile(Relative + '\ThemeSwitcher.MenuSetup.exe');
            BackupFile(Relative + '\identity.msix');
          end;
        end;
      until not FindNext(FindRec);
    finally FindClose(FindRec); end;
  end;
end;

procedure RestoreOldComponents;
var FindRec: TFindRec; Relative: String;
begin
  if FindFirst(BackupDir + '\components\*', FindRec) then begin
    try
      repeat
        if ((FindRec.Attributes and FILE_ATTRIBUTE_DIRECTORY) <> 0) and (FindRec.Name <> '.') and (FindRec.Name <> '..') then begin
          Relative := 'components\' + FindRec.Name;
          RestoreFile(Relative + '\owner.txt');
          RestoreFile(Relative + '\ThemeSwitcherShell.dll');
          RestoreFile(Relative + '\ThemeSwitcher.Deploy.exe');
          RestoreFile(Relative + '\ThemeSwitcher.MenuSetup.exe');
          RestoreFile(Relative + '\identity.msix');
        end;
      until not FindNext(FindRec);
    finally FindClose(FindRec); end;
  end;
end;

procedure CleanBackup;
var FindRec: TFindRec; ComponentDir: String;
begin
  { Remove only the known backup payloads; never recursively delete this tree. }
  if FindFirst(BackupDir + '\components\*', FindRec) then begin
    try
      repeat
        if ((FindRec.Attributes and FILE_ATTRIBUTE_DIRECTORY) <> 0) and (FindRec.Name <> '.') and (FindRec.Name <> '..') then begin
          ComponentDir := BackupDir + '\components\' + FindRec.Name;
          DeleteFile(ComponentDir + '\owner.txt');
          DeleteFile(ComponentDir + '\ThemeSwitcherShell.dll');
          DeleteFile(ComponentDir + '\ThemeSwitcher.Deploy.exe');
          DeleteFile(ComponentDir + '\ThemeSwitcher.MenuSetup.exe');
          DeleteFile(ComponentDir + '\identity.msix');
          RemoveDir(ComponentDir);
        end;
      until not FindNext(FindRec);
    finally FindClose(FindRec); end;
  end;
  RemoveDir(BackupDir + '\components');
  DeleteFile(BackupDir + '\assets\windows-moon.png');
  RemoveDir(BackupDir + '\assets');
  DeleteFile(BackupDir + '\ThemeSwitcher.exe');
  DeleteFile(BackupDir + '\install-state.ini');
  DeleteFile(BackupDir + '\LICENSE');
  DeleteFile(BackupDir + '\unins000.exe');
  DeleteFile(BackupDir + '\unins000.dat');
  DeleteFile(BackupDir + '\v1.ini');
  if not RemoveDir(BackupDir) then
    MsgBox('备份目录未能完全清理，请保留并检查：' + BackupDir, mbInformation, MB_OK);
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
var Detail, PreviousDir: String; FindRec: TFindRec; HasFiles: Boolean;
begin
  Result := '';
  { Refuse to overwrite another application's files or move an existing install. }
  if RegQueryStringValue(HKLM32, 'Software\Microsoft\Windows\CurrentVersion\Uninstall\桌面主题切换器 (Theme Switcher)_is1', 'InstallLocation', PreviousDir) then begin
    if CompareText(RemoveBackslashUnlessRoot(PreviousDir), RemoveBackslashUnlessRoot(ExpandConstant('{app}'))) <> 0 then begin
      Result := '升级必须使用原安装目录：' + PreviousDir; Exit;
    end;
  end else if DirExists(ExpandConstant('{app}')) then begin
    HasFiles := False;
    if FindFirst(ExpandConstant('{app}\*'), FindRec) then begin
      try
        repeat
          if (FindRec.Name <> '.') and (FindRec.Name <> '..') then HasFiles := True;
        until not FindNext(FindRec);
      finally FindClose(FindRec); end;
    end;
    if HasFiles then begin Result := '首次安装请选择空目录，以免覆盖其他文件。'; Exit; end;
  end;
  ExtractTemporaryFile('ThemeSwitcher.Deploy.exe');
  Helper := ExpandConstant('{tmp}\ThemeSwitcher.Deploy.exe');
  if not RunDeploy('preflight ' + Quote(ExpandConstant('{app}')), Detail) then begin Result := Detail; Exit; end;
  if ModernWindows then begin
    ExtractTemporaryFile('identity.msix');
    if not RunDeploy('verify-package ' + Quote(ExpandConstant('{tmp}\identity.msix')), Detail) then begin Result := Detail; Exit; end;
  end;
  BackupDir := ExpandConstant('{app}\.upgrade-recovery');
  if DirExists(BackupDir) then begin
    Result := '发现上次升级保留的恢复文件，请先检查该目录，避免覆盖恢复材料：' + BackupDir;
    Exit;
  end;
  if not ForceDirectories(BackupDir) then begin Result := '无法创建升级备份目录：' + BackupDir; Exit; end;
  BackupFile('ThemeSwitcher.exe'); BackupFile('install-state.ini');
  BackupFile('LICENSE'); BackupFile('assets\windows-moon.png');
  BackupFile('unins000.exe'); BackupFile('unins000.dat');
  BackupOldComponents;
  HadOldEntry := RegQueryStringValue(HKLM32, 'Software\Microsoft\Windows\CurrentVersion\Uninstall\桌面主题切换器 (Theme Switcher)_is1', 'DisplayName', OldDisplayName);
  RegQueryStringValue(HKLM32, 'Software\Microsoft\Windows\CurrentVersion\Uninstall\桌面主题切换器 (Theme Switcher)_is1', 'DisplayVersion', OldDisplayVersion);
  if not RunDeploy('backup-v1 ' + Quote(ExpandConstant('{app}')) + ' ' + Quote(BackupDir + '\v1.ini'), Detail) then begin Result := Detail; Exit; end;
  BackedUp := True;
end;

procedure CurStepChanged(CurStep: TSetupStep);
var Detail, MenuHelper, MenuPackage: String; Code: Integer;
begin
  if CurStep = ssPostInstall then begin
    DeploymentAttempted := True;
    if ModernWindows then begin
      if not RunDeploy('install-modern ' + Quote(ExpandConstant('{app}')) + ' ' + Quote(ExpandConstant('{app}\components\{#AppVersion}\identity.msix')), Detail) then RaiseException(Detail);
    end else begin
      if not RunDeploy('install-legacy ' + Quote(ExpandConstant('{app}')), Detail) then RaiseException(Detail);
    end;
    if not RunDeploy('cleanup-old ' + Quote(ExpandConstant('{app}')), Detail) then RaiseException(Detail);
    if ModernWindows then begin
      MenuHelper := ExpandConstant('{app}\components\{#AppVersion}\ThemeSwitcher.MenuSetup.exe');
      MenuPackage := ExpandConstant('{app}\components\{#AppVersion}\identity.msix');
      { Apply the preference to the original interactive user, not an alternate
        administrator account used at the UAC prompt. Existing choices survive. }
      if not ExecAsOriginalUser(MenuHelper, 'initialize ' + Quote(MenuPackage), '', SW_HIDE, ewWaitUntilTerminated, Code) then
        RaiseException('无法为当前登录用户初始化菜单显示。错误代码：' + IntToStr(Code));
      if Code = 20 then Log('当前系统没有对应的菜单个性化接口，使用系统默认显示位置。')
      else if Code <> 0 then RaiseException('无法为当前登录用户初始化菜单显示。错误代码：' + IntToStr(Code));
    end;
    Committed := True;
  end;
end;

procedure DeinitializeSetup;
var Detail: String;
begin
  if BackedUp and not Committed then begin
    RestoreOldComponents;
    RestoreFile('ThemeSwitcher.exe');
    if not FileExists(BackupDir + '\ThemeSwitcher.exe') then DeleteFile(ExpandConstant('{app}\ThemeSwitcher.exe'));
    if FileExists(BackupDir + '\install-state.ini') then RestoreFile('install-state.ini')
    else DeleteFile(ExpandConstant('{app}\install-state.ini'));
    if DeploymentAttempted then begin
      if not RestoreFailed then begin
        if not RunDeploy('rollback ' + Quote(ExpandConstant('{app}')), Detail) then RestoreFailed := True;
        if not RunDeploy('restore-v1 ' + Quote(ExpandConstant('{app}')) + ' ' + Quote(BackupDir + '\v1.ini'), Detail) then RestoreFailed := True;
      end;
    end;
    if not RestoreFailed then
      if not RunDeploy('discard-version ' + Quote(ExpandConstant('{app}')), Detail) then RestoreFailed := True;
    if FileExists(BackupDir + '\LICENSE') then RestoreFile('LICENSE') else DeleteFile(ExpandConstant('{app}\LICENSE'));
    if FileExists(BackupDir + '\assets\windows-moon.png') then RestoreFile('assets\windows-moon.png') else DeleteFile(ExpandConstant('{app}\assets\windows-moon.png'));
    RemoveDir(ExpandConstant('{app}\assets'));
    RestoreFile('unins000.exe'); RestoreFile('unins000.dat');
    if HadOldEntry then begin
      RegWriteStringValue(HKLM32, 'Software\Microsoft\Windows\CurrentVersion\Uninstall\桌面主题切换器 (Theme Switcher)_is1', 'DisplayName', OldDisplayName);
      RegWriteStringValue(HKLM32, 'Software\Microsoft\Windows\CurrentVersion\Uninstall\桌面主题切换器 (Theme Switcher)_is1', 'DisplayVersion', OldDisplayVersion);
    end;
  end;
  if BackedUp then begin
    if RestoreFailed then
      MsgBox('升级未完成，自动恢复也未能全部完成。恢复文件已保留，请勿删除：' + BackupDir, mbError, MB_OK)
    else CleanBackup;
  end;
end;

function InitializeUninstall: Boolean;
var Detail: String;
begin
  { Run a temporary copy so the installed helper itself never blocks cleanup. }
  Helper := ExpandConstant('{tmp}\ThemeSwitcher.Deploy.exe');
  Result := CopyFile(ExpandConstant('{app}\components\{#AppVersion}\ThemeSwitcher.Deploy.exe'), Helper, False);
  if not Result then begin MsgBox('无法准备卸载组件，请重新运行当前版本的安装包修复。', mbError, MB_OK); Exit; end;
  Result := RunDeploy('preflight ' + Quote(ExpandConstant('{app}')), Detail);
  if not Result then MsgBox(Detail, mbError, MB_OK);
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var Detail: String;
begin
  if CurUninstallStep = usUninstall then begin
    if not RunDeploy('uninstall ' + Quote(ExpandConstant('{app}')), Detail) then RaiseException(Detail);
  end;
end;
