param([string]$IsccPath)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
if (-not $IsccPath) { $IsccPath = Join-Path $env:LOCALAPPDATA 'Programs\Inno Setup 6\ISCC.exe' }
if (-not (Test-Path -LiteralPath $IsccPath)) { throw 'Supply -IsccPath for Inno Setup 6.' }
$testParent = Join-Path $repo 'build\installer-directory-tests'
$testRoot = Join-Path $testParent ([guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $testRoot -Force | Out-Null
$fixture = Join-Path $testRoot 'payload.txt'
Set-Content -LiteralPath $fixture -Value 'Disposable installer directory lifecycle fixture.' -Encoding ascii
$version = (Get-Content -LiteralPath (Join-Path $repo 'VERSION') -Raw).Trim()
$productionInclude = Join-Path $repo 'packaging\InstallDirectories.iss'
$legacyInclude = Join-Path $testRoot 'LegacyDirectories.iss'
@'
[UninstallDelete]
Type: files; Name: "{app}\install-state.ini"
Type: dirifempty; Name: "{app}\assets"
'@ | Set-Content -LiteralPath $legacyInclude -Encoding ascii
$source = @'
#define AppVersion "__VERSION__"
[Setup]
AppName=ThemeSwitcher directory cleanup fixture
AppVersion=1.0
DefaultDirName={localappdata}\ThemeSwitcherDirectoryFixture
PrivilegesRequired=lowest
CreateUninstallRegKey=no
DisableWelcomePage=yes
DisableDirPage=yes
DisableProgramGroupPage=yes
DisableReadyPage=yes
DirExistsWarning=no
CloseApplications=no
RestartApplications=no
OutputBaseFilename=__OUTPUT__
[Files]
Source: "__FIXTURE__"; DestDir: "{app}"; Flags: ignoreversion
#include "__INCLUDE__"
[Code]
function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  { Reproduce production's early recovery-directory creation. }
  ForceDirectories(ExpandConstant('{app}\.upgrade-recovery'));
  Result := '';
end;
procedure CurStepChanged(Step: TSetupStep);
begin
  if Step = ssPostInstall then begin
    SaveStringToFile(ExpandConstant('{app}\install-state.ini'), 'fixture', False);
    RemoveDir(ExpandConstant('{app}\.upgrade-recovery'));
  end;
end;
'@
foreach ($mode in @('legacy','fixed')) {
    $include = if ($mode -eq 'legacy') { $legacyInclude } else { $productionInclude }
    $iss = Join-Path $testRoot ($mode + '.iss')
    $rendered = $source.Replace('__VERSION__',$version).Replace('__OUTPUT__',$mode).Replace('__FIXTURE__',$fixture).Replace('__INCLUDE__',$include)
    [IO.File]::WriteAllText($iss,$rendered,[Text.UTF8Encoding]::new($true))
    & $IsccPath ("/O"+$testRoot) $iss *> (Join-Path $testRoot ($mode+'-compile.log'))
    if ($LASTEXITCODE) { throw ('Harness compilation failed: '+$iss) }
}
$results = [Collections.Generic.List[string]]::new()
foreach ($case in @('legacy-control','new-root','preexisting-empty-root','preserve-user-file')) {
    $app = Join-Path $testRoot $case
    if ($case -eq 'preexisting-empty-root') { New-Item -ItemType Directory -Path $app | Out-Null }
    $installer = Join-Path $testRoot $(if ($case -eq 'legacy-control') { 'legacy.exe' } else { 'fixed.exe' })
    $install = Start-Process -FilePath $installer -ArgumentList @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART',('/DIR="'+$app+'"'),('/LOG="'+(Join-Path $testRoot ($case+'-install.log'))+'"')) -WindowStyle Hidden -Wait -PassThru
    if ($install.ExitCode -ne 0) { throw ('Harness install failed: '+$case+' exit '+$install.ExitCode) }
    if ($case -eq 'preserve-user-file') { Set-Content -LiteralPath (Join-Path $app 'user-note.txt') -Value 'keep me' -Encoding ascii }
    $uninstall = Start-Process -FilePath (Join-Path $app 'unins000.exe') -ArgumentList @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART',('/LOG="'+(Join-Path $testRoot ($case+'-uninstall.log'))+'"')) -WindowStyle Hidden -Wait -PassThru
    if ($uninstall.ExitCode -ne 0) { throw ('Harness uninstall failed: '+$case+' exit '+$uninstall.ExitCode) }
    for ($poll=0; $poll -lt 20 -and (Test-Path -LiteralPath (Join-Path $app 'unins000.exe')); $poll++) { Start-Sleep -Milliseconds 100 }
    if ($case -eq 'legacy-control') {
        if (-not (Test-Path -LiteralPath $app) -or @(Get-ChildItem -LiteralPath $app -Force).Count -ne 0) { throw 'Legacy reproduction did not leave the expected empty root.' }
        $results.Add('PASS legacy control: old rules leave the pre-created empty install directory.')
    } elseif ($case -eq 'preserve-user-file') {
        $left = @(Get-ChildItem -LiteralPath $app -Force)
        if ($left.Count -ne 1 -or $left[0].Name -ne 'user-note.txt' -or (Get-Content -LiteralPath $left[0].FullName -Raw).Trim() -ne 'keep me') { throw 'Uninstall failed to preserve only the unrelated user file.' }
        $results.Add('PASS preserve-user-file: owned payload and empty subdirectories removed; user note retained.')
    } else {
        if (Test-Path -LiteralPath $app) { throw ('Empty installation directory remains: '+$case) }
        $results.Add('PASS '+$case+': empty installation root removed.')
    }
}
$results | Tee-Object -FilePath (Join-Path $testRoot 'RESULTS.txt')
Write-Host ('Harness artifacts and logs: '+$testRoot)
Write-Host 'Only disposable per-user fixture installers were executed. The actual ThemeSwitcher installation was not changed.'
