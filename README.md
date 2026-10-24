<p align="center"><img src="assets/windows-moon.png" width="128" alt="Windows Theme Switcher 应用图标"></p>

# Windows Theme Switcher 2.0 · 深浅色切换器

通过桌面右键菜单，快速切换 Windows 深色与浅色模式。

当前分支为 **2.0 开发验证版**，尚未公开发行。正式发布前需要完成菜单、覆盖升级和跨系统联调。

**[下载已发布的稳定版](https://github.com/hakanasora/windows-theme-switcher/releases/latest)** · [开发说明](DEVELOPMENT.md) · [验证步骤](docs/TESTING.md)

## 为什么开发

不同的使用环境下，用户对颜色模式的偏好也会有所变化。有些用户习惯白天使用浅色模式，夜间使用深色模式，并希望根据需要随时切换。

Windows 11 的颜色模式选项位于「设置」中，切换时需要打开设置并进入相应页面。对于经常调整颜色模式的用户，这一操作略显繁琐。

Windows Theme Switcher 将这项操作放入桌面右键菜单。**在桌面空白处点击右键，选择浅色或深色模式即可完成切换。** 切换效果与 Windows「设置」中的颜色模式选项一致。

## 功能

- **动态切换项**：浅色模式下显示“切换到深色模式”及深色图标；深色模式下显示相反操作及浅色图标。传统和现代菜单使用相同交互。
- **无独立常驻程序**：没有主界面、托盘或开机启动项；菜单组件在 Windows 宿主中维护事件通知和内存缓存。
- **命令行支持**：保留 `light`、`dark`，增加 `toggle`；主程序仅在执行切换时短时运行。
- **覆盖升级**：安装器接续 1.0 的安装标识和安装目录；原型的真实升级及失败恢复仍需按验证步骤确认。

## 原版菜单示意

以下是 1.0 的真实截图；2.0 改为单个动态命令，实机截图将在菜单联调后更新。

<img src="docs/context-menu-light.png" alt="安装后，桌面右键菜单中出现切换到浅色模式和切换到深色模式两个选项" width="460">

## 安装与使用

2.0 面向原生 x64 系统：Windows 10 及较早 Windows 11 使用传统菜单；Windows 11 22H2 及以上使用现代桌面菜单注册，安装需要管理员权限。

开发安装包由 `build.ps1 -Installer -DevelopmentCertificate` 生成，位于 `dist/v2-development/`。它只适用于明确授权的测试环境，现代菜单测试前需要手动信任所附开发公钥证书。安装器不会导入证书或开启开发者模式，详细步骤见[验证文档](docs/TESTING.md)。

安装后，在桌面空白处点击右键，选择与当前模式相反的切换项。如果 Windows 将扩展放入“应用扩展”子菜单，可在“个性化 → 上下文菜单”中开启主菜单显示。程序不强制修改该偏好，也不指定系统菜单中的固定行号。

部分应用可能需要重新打开，才能应用新的颜色模式。

### 卸载与清理

如需移除工具，可在 Windows 的已安装应用列表中卸载。**卸载会同时移除程序文件及其添加的右键菜单项**，无需手动删除文件或清理菜单。

2.0 卸载流程包含身份包和传统注册项的清理。原型阶段需验证实际清理结果；如果菜单组件仍被占用，安装器会提示重试，不强行终止资源管理器。

卸载不会改变当前的颜色模式。

### 命令行调用

在 `ThemeSwitcher.exe` 所在目录中运行：

```powershell
# 浅色模式
.\ThemeSwitcher.exe light

# 深色模式
.\ThemeSwitcher.exe dark

# 切换到当前系统模式的另一侧
.\ThemeSwitcher.exe toggle
```

模式参数不区分大小写。不带参数或传入不支持的模式时，程序直接退出。

## 从源码构建

### 编译程序

需要 MSVC C++ x64 工具、Windows SDK 和 CMake（可通过 Visual Studio Build Tools 2022 安装）。2.0 使用 C++20 和静态运行库，不要求最终用户额外安装 .NET 或 Visual C++ 运行库。

在项目根目录运行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1
```

脚本默认运行自动测试。编译结果位于 `build/native/artifacts/`；菜单 DLL 位于其版本化的 `components/2.0.0/` 子目录。`src/ThemeSwitcher.cs` 和对应项目文件仅保留作 1.0 恢复源码参考，不参与 2.0 构建。

### 生成安装包

安装 Inno Setup 6 后，在项目根目录生成开发签名安装包：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Installer -DevelopmentCertificate
```

如果无法自动找到 Inno Setup，可手动指定编译器路径：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Installer -DevelopmentCertificate -IsccPath "C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
```

安装包输出至 `dist/v2-development/ThemeSwitcherSetup.exe`，旁边包含公钥证书及指纹。开发私钥保存在当前用户的个人证书存储中且不可导出；构建脚本不修改证书信任。

## 工作原理

程序修改当前用户注册表中的两项 DWORD 设置：

```text
HKEY_CURRENT_USER\SOFTWARE\Microsoft\Windows\CurrentVersion\Themes\Personalize
```

| 设置项 | 作用 | 浅色模式 | 深色模式 |
| --- | --- | --- | --- |
| `AppsUseLightTheme` | 支持主题跟随的应用 | `1` | `0` |
| `SystemUsesLightTheme` | Windows 系统颜色模式 | `1` | `0` |

写入后，通过 `SendMessageTimeout` 广播 `WM_SETTINGCHANGE`，通知系统和应用刷新主题。

程序沿用原有的静默执行方式：设置路径不存在时不会创建，操作异常时不会弹窗提示。

动态菜单按 `SystemUsesLightTheme` 决定目标模式，切换时将两项设置统一。状态由注册表事件通知更新缓存，没有定时轮询；缓存有效时菜单回调只读内存。首次加载或监听失效恢复时会重新读取状态。不能读取状态时，命令禁用。

## 项目结构

```text
.
├── assets/                     # 应用图标与首页展示图标
├── src/
│   └── native/                 # 2.0 原生核心、缓存、菜单及部署组件
├── tests/                      # 核心、通知、生命周期和回调性能检查
├── packaging/                  # 身份包清单和安装脚本
├── tools/                      # 构建、签名与打包入口
├── docs/                       # 截图和验证记录
├── InstallerScript.iss         # Inno Setup 安装脚本
├── MyIcon.ico                  # 程序与安装包图标
├── build.ps1                   # 编译与打包脚本
├── CMakeLists.txt              # 原生构建定义
├── LICENSE                     # MIT 代码许可证
├── RECOVERY.md                 # 源码恢复记录
└── README.md
```

## 源码说明

1.0 的 C# 源码根据作者保留的 `ThemeSwitcher.exe` 恢复，并通过方法 IL 一致性验证。2.0 在此基础上改用原生实现；1.0 的 IL 验证记录不等于对 2.0 的验证。

恢复过程与验证范围见 [RECOVERY.md](RECOVERY.md)。

已发布的 1.0 主程序和安装包保持原样。2.0 当前的验证状态见 `docs/TESTING.md`。

## 许可证

本项目代码采用 [MIT 许可证](LICENSE)。Windows 系统图标不包含在该代码许可范围内。
