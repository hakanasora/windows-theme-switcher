# 2.0 开发说明

## 构建

Windows 原生 x64，Visual Studio Build Tools 2022 的 MSVC x64、CMake/Ninja、Windows 11 SDK 26100；打包另需 Inno Setup 6。普通用户运行安装后的程序无需这些开发工具。

```powershell
.\build.ps1
.\build.ps1 -Installer -DevelopmentCertificate
.\build.ps1 -Installer -UnsignedPrototype
```

第一条命令构建并运行测试，不注册菜单、不修改实际主题。第二条复用或创建个人证书存储中的不可导出开发私钥，签署稀疏身份包并生成验证安装包，但不会信任证书或执行安装。

第三条是本轮 2.0.1 无签名验证路线，不创建或导出证书。隔离身份为 `HakanaSora.ThemeSwitcher.UnsignedDevelopment`，Publisher 必须精确匹配源码白名单并带 unsigned OID。部署采用已验证的提升权限 Add + AllowUnsigned + ExternalLocation，不把旧的签名验证失败降级成无签名安装。

正式包构建接收 `-CertificateThumbprint` 和 `-Publisher`，证书必须已在当前用户的 `My` 存储中，且 Subject 与 Publisher 相符。开发包身份为 `HakanaSora.ThemeSwitcher.Development`，正式身份为 `HakanaSora.ThemeSwitcher`；不得混用开发身份作为正式发布链。正式签名、时间戳及分发方案确定前不发布 2.0。

## 运行结构

- `ThemeCore`：读取模式、构建目标状态、写入两项设置并在部分写入失败时恢复原值。
- `ThemeSwitcher.exe`：无窗口的短时执行入口。参数为 `light`、`dark`、`toggle`；空参数无操作。退出码：0 成功，2 参数错误，3 状态未知，4 写入失败，5 恢复失败，6 系统通知失败。
- `ThemeSwitcherShell.dll`：同一个 COM 命令提供动态文字、图标、状态及点击动作。每个命令对象捕获一份快照；展开菜单时不创建主程序进程。
- `ThemeSwitcher.Deploy.exe`：安装器内部的同步部署助手，调用 Windows 部署 API，不通过 PowerShell 注册菜单。它不是常驻程序，不创建服务或任务。
- `ThemeSwitcher.MenuSetup.exe`：安装时以原登录用户身份短时运行，首次初始化一级菜单偏好；已有明确选择时保持原值。内部 WinRT 接口经 QueryInterface 验证，系统缺少此接口时返回 20，由安装器记录并保留系统默认位置。此工具不会在右键菜单展开时运行。

缓存按加载模块共享。首次订阅后读取状态；通知回调重新订阅并发布新快照。缓存命中使用单次原子读取，未知状态不猜测。没有定时器或轮询线程。

Windows 可能保留命令对象/工厂，因此组件可能在宿主中保持加载。`DllCanUnloadNow` 在无活动 COM 对象时取消等待、排空回调并释放缓存；不永久锁住 DLL，也不在 `DllMain` 中等待工作线程。

## 菜单注册

固定 CLSID 为 `{09A260C0-DBA6-4B6D-8D2D-CF3A8C32BA76}`。Windows 11 22H2 及以上只使用包清单中的 `desktop10:ItemType Type="DesktopBackground"`；更早系统仅使用传统 `DesktopBackground` 注册。

不同时注册现代和传统两套命令，避免重复。新式注册在“显示更多选项”中的可见性、Shell 对命令对象的复用和分隔线表现，均列入实机发布门槛，不能用组件单元测试替代。

## 安装迁移

`AppId` 固定沿用 `桌面主题切换器 (Theme Switcher)`，保留 1.0 的管理员和 32 位安装器模式；原生 DLL 及其传统 COM 注册使用 x64。安装路径由 Inno Setup 从旧安装恢复。

升级先检查占用和签名信任，备份旧主程序、安装状态、卸载文件和受控版本目录。注册新菜单成功后才删除经命令路径校验的 1.0 菜单项；发生错误时恢复备份并恢复旧菜单注册。

2.0.1 无签名路线的预检验证身份与无签名包结构，不要求证书信任；签名路线仍保留原有信任检查。若原安装保存着另一开发身份包，则在新注册成功后移除旧身份，失败时尝试恢复原包。

自动一级显示采用 `ExecAsOriginalUser`，不依赖工具进程所在环境。实验已证明相同诊断 EXE 在工具环境和正常桌面环境读到不同结果，因此安装效果必须由用户从正常桌面启动安装器验证，不能以工具环境中的读回成功代替实测。安装器主动不请求 Explorer 重启。

传统菜单前后分隔标志仍由同一个 IExplorerCommand 返回，但当前 Beta 实测没有形成独立分组；本轮尚未解决，不添加会产生重复入口的第二套处理程序。

组件目录包含产品归属标记，清理时只删除标记匹配目录中的固定文件名，不递归清空用户选定的安装目录。安装和卸载均不自动杀死资源管理器，不导入证书，不开启开发者模式。

目前安装事务的真实失败恢复、多用户包注册及卸载必须实测；在这些测试完成前，安装包仅为开发原型。

升级备份位于安装目录下的 `.upgrade-recovery`。恢复失败时保留备份并明确提示；再次运行会拒绝覆盖已有恢复目录。正常完成后只清理备份中的已知文件。首次安装拒绝非空目录，升级固定沿用已记录的安装路径。

安装器使用 `UninstallLogMode=overwrite`，避免继承 1.0 中按通用菜单键名执行的卸载动作；旧卸载文件也纳入恢复备份。该迁移选择必须通过原始 1.0 安装包的实机测试。

版本统一读取仓库根目录 `VERSION`。编译环境使用 MSVC 英文诊断，确保 Ninja 能正确解析头文件依赖；不更改 Windows 的显示语言。

## 2.0.2 空目录清理修复

PrepareToInstall 为 `.upgrade-recovery` 提前创建 `{app}`，使默认目录跟踪把安装根目录视为原有目录。2.0.2 使用共享的 `packaging/InstallDirectories.iss`，对四个自有目录记录 `uninsalwaysuninstall`，最后通过 `dirifempty` 清理安装根目录。没有递归删除或目录通配符。安装中止时，也仅尝试移除本次新建且已空的根目录。

`tools/Test-InstallerDirectories.ps1` 构建并静默运行隔离的低权限 Inno 安装夹具。旧规则对照能复现空根目录残留；共用生产目录规则的修复夹具验证新根目录、预先存在空根目录的移除，以及额外用户文件的保留。夹具不会注册菜单或触碰真实安装。
