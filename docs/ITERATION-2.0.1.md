# 2.0.1 本机验证迭代

本轮交付是单个无签名验证安装包，尚未公开发布。原 1.0 发行文件和 main 分支保持不变。

## 已合并

- 独立无签名身份 `HakanaSora.ThemeSwitcher.UnsignedDevelopment`。部署助手原生调用 AllowUnsigned + ExternalLocation；不运行 PowerShell、不导入证书、不启用开发者模式。
- 新增短时运行的 ThemeSwitcher.MenuSetup.exe。安装器通过 ExecAsOriginalUser，以原交互用户身份首次初始化一级显示，已有明确开关值时不覆盖。没有对应内部接口的系统保留默认位置。
- 在同一组件版本目录中部署菜单设置助手，纳入备份、占用检查和受控清理；移除旧开发身份时可恢复原包。
- 保留无界面、无托盘、无启动项、无独立常驻服务及事件缓存架构。

## 本轮已执行检查

- 完整构建与 Inno Setup 打包通过。
- NativeCoreCacheShell、DeploymentIsolation、MenuPreferencePreservation 三组 CTest 全部通过。
- 缓存回调组 P95 0.0001 毫秒，仅代表组件微基准。
- 本轮无签名身份包预检退出码 0。
- 尚未注册本轮身份时，菜单初始化助手返回 22（目标包未注册），未修改其他扩展。
- 用户已手动卸载原开发版；独立 UnsignedProbe 包已通过 Remove-AppxPackage 移除，后续截图确认两个旧入口消失。

## 本轮待用户实机验收

从普通桌面双击安装器，允许 UAC。确认无需证书或命令步骤、首次自动一级显示、只有一个动态入口；验证切换与状态，再测试关闭一级显示后的覆盖重装是否保留选择，最后检查卸载。

先前独立实验已验证无签名包能注册并正常切换，且用户从普通 PowerShell 调用内部接口能将实验项移入二级并恢复一级。这不等于本轮安装器整合已经实测通过。

## 已知限制

- 传统菜单独立分组尚未修复；仍请求前后分隔线，但当前 Beta 不按预期绘制。本轮没有引入可能造成重复入口的第二套传统注册。
- 内部 WinRT 菜单配置接口不是公开兼容承诺；缺失时采用系统默认位置。
- 微软将 AllowUnsigned 定位为测试用途。正式发行方案、稳定版/Windows 10、标准账户及跨用户部署、原始 1.0 升级和失败恢复尚需验证。
- 工具环境与用户桌面环境曾读到不同偏好状态，原因未确定。本轮安装效果以用户正常启动后的实际菜单为准。

来源：https://learn.microsoft.com/en-us/windows/msix/package/unsigned-package

原用户上下文执行：https://jrsoftware.org/ishelp/topic_isxfunc_execasoriginaluser.htm
