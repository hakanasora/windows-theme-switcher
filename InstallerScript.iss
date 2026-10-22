; Inno Setup Script for Theme Switcher

[Setup]
; 安装包的基本信息
AppName=桌面主题切换器 (Theme Switcher)
AppVersion=1.0
AppPublisher=Hakana Sora
SetupIconFile=MyIcon.ico
ArchitecturesAllowed=x64
MinVersion=10.0
PrivilegesRequired=admin
DefaultDirName={autopf}\ThemeSwitcher
DefaultGroupName=主题切换器
UninstallDisplayIcon={app}\ThemeSwitcher.exe
SolidCompression=yes
WizardStyle=modern
OutputBaseFilename=ThemeSwitcherSetup

[Files]
; 需要打包的文件：将我们准备好的 .exe 文件打包进去
; Source: "源文件路径"; DestDir: "安装到目标电脑的路径"
Source: "ThemeSwitcher.exe"; DestDir: "{app}"

[Registry]
; 这是核心部分：在这里写入所有需要的注册表项
; Root: 根键; Subkey: 子键; ValueType:值的类型; ValueName:值的名称; ValueData:值的数据; Flags: 标志

; --- 创建浅色模式菜单项 ---
Root: HKCR; Subkey: "DesktopBackground\Shell\1_LightMode"; ValueType: string; ValueName: ""; ValueData: "切换到浅色模式"; Flags: uninsdeletekey
Root: HKCR; Subkey: "DesktopBackground\Shell\1_LightMode"; ValueType: string; ValueName: "Icon"; ValueData: "shell32.dll,15"
Root: HKCR; Subkey: "DesktopBackground\Shell\1_LightMode\command"; ValueType: string; ValueName: ""; ValueData: """{app}\ThemeSwitcher.exe"" light"

; --- 创建深色模式菜单项 ---
Root: HKCR; Subkey: "DesktopBackground\shell\2_DarkMode"; ValueType: string; ValueName: ""; ValueData: "切换到深色模式"; Flags: uninsdeletekey
Root: HKCR; Subkey: "DesktopBackground\shell\2_DarkMode"; ValueType: string; ValueName: "Icon"; ValueData: "imageres.dll,96"
Root: HKCR; Subkey: "DesktopBackground\shell\2_DarkMode\command"; ValueType: string; ValueName: ""; ValueData: """{app}\ThemeSwitcher.exe"" dark"

; --- 创建顶部分隔符 ---
Root: HKCR; Subkey: "DesktopBackground\shell\0_SeparatorTop"; Flags: uninsdeletekey
Root: HKCR; Subkey: "DesktopBackground\shell\0_SeparatorTop"; ValueType: dword; ValueName: "CommandFlags"; ValueData: "8"

; --- 创建底部分隔符 ---
Root: HKCR; Subkey: "DesktopBackground\shell\3_SeparatorBottom"; Flags: uninsdeletekey
Root: HKCR; Subkey: "DesktopBackground\shell\3_SeparatorBottom"; ValueType: dword; ValueName: "CommandFlags"; ValueData: "8"