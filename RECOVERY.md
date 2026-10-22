# 源码恢复记录

恢复日期：2026-10-22。

## 来源

- 原程序：作者提供目录内的 `ThemeSwitcher.exe`，77,824 字节。
- 程序类型：.NET Framework 4 系列托管 Windows GUI 程序。
- 入口：`ThemeSwitcher.Main(string[] args)`。
- 原安装脚本：从作者目录直接复制，未修改。
- 图标：恢复时保留了原图标；2026-10-23 按作者要求换成 Windows 自带的显示器月亮图标，与深色右键菜单项使用的系统图标一致。来源见 `assets/README.md`。

原始文件的 SHA-256 摘要记录于 `docs/original-sha256.txt`。

## 还原依据

读取程序集元数据、P/Invoke 签名、所有方法的 IL 及异常处理区域；没有执行原程序的切换逻辑。

恢复保留的行为：

1. 参数为空直接退出；首个参数通过 `ToLower()` 转为小写。
2. `dark` 对应整数 `0`，`light` 对应整数 `1`；其他参数退出，多余参数忽略。
3. 以可写方式打开当前用户已有的 `Themes\Personalize` 键。
4. 若键存在，写入两项 DWORD 主题设置。
5. 释放注册表句柄，再向 `0xffff` 广播 `0x001a`，字符串为 `ImmersiveColorSet`，标志 `2`，超时 `5000` 毫秒。
6. 捕获注册表操作与通知过程中产生的 `System.Exception`，静默退出。

原始 IL 文本保存在 `docs/original-il.txt`，可用于逐项审阅。

## 验证范围

验证结果见 `docs/verification.txt`。构建验证不会运行深色或浅色切换，也不会安装右键菜单。恢复后的二进制包含新的编译时间、模块标识等信息，不承诺与原 EXE 逐字节一致。

没有恢复原始注释、排版、局部变量名或历史版本。本次公开发布的代码采用 MIT 许可证。

## 发布文件

按作者要求，首次发布直接采用作者原先编译的 `ThemeSwitcher.exe` 和 `ThemeSwitcherSetup.exe`，不使用恢复源码生成的二进制替代。待发布文件暂存于 `dist/`，两者的 SHA-256 均与恢复时记录的原文件摘要一致。

原始二进制中的图标保持不变。README 展示图标以及供后续源码构建使用的 `MyIcon.ico` 使用新选定的 Windows 月亮图标。
