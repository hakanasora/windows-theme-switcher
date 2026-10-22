# 图标来源

- `windows-moon.png`：Windows 本地 `imageres.dll` 图标索引 `96` 所对应的原始 256 × 256 PNG，与安装脚本中深色菜单项的 `imageres.dll,96` 一致。
- 根目录的 `MyIcon.ico`：同一系统图标的完整 ICO，包含 16、20、24、32、40、48、64 和 256 像素尺寸，供后续从源码构建程序与安装包时使用。首次发布沿用作者的原始二进制，内部图标保持不变。
- `theme-switcher.svg`：README 首页展示用的圆角方形底板，内部嵌入上述原始图标。
- `theme-switcher.png`：从展示用 SVG 渲染的 PNG，供 GitHub README 稳定显示。

本次使用的原始资源位于 Windows 的 `SystemResources/imageres.dll.mun`，图标组 ID 为 `101`。图标本体未重绘。Windows 系统图标不属于本项目的 MIT 代码许可范围。
