; PrepareToInstall creates {app} for recovery files before normal installation.
; Record every owned directory even if it already exists at that point.
[Dirs]
Name: "{app}"; Flags: uninsalwaysuninstall
Name: "{app}\components"; Flags: uninsalwaysuninstall
Name: "{app}\components\{#AppVersion}"; Flags: uninsalwaysuninstall
Name: "{app}\assets"; Flags: uninsalwaysuninstall

[UninstallDelete]
Type: files; Name: "{app}\install-state.ini"
Type: dirifempty; Name: "{app}\components\{#AppVersion}"
Type: dirifempty; Name: "{app}\components"
Type: dirifempty; Name: "{app}\assets"
Type: dirifempty; Name: "{app}"
