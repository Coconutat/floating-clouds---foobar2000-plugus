# floating clouds - foobar2000 plugus  
**语言**: [ENGLISH](README.md) | **简体中文**
***
Foobar2000 SDK Version: SDK-2025-03-07  
Windows Template Library: WTL10_01_Release    
***

一个 Foobar2000 插件，在桌面或游戏时在显示器某区域显示音乐播放器的悬浮 UI 叠加层。支持专辑封面、歌曲名称、歌手显示和简易播放控制。

## 特性

- **悬浮叠加** — 独立窗口，始终保持在桌面或全屏游戏之上
- **3 种核心风格** — Mini、小方块 Mini、小方块（含播放控制按钮）
- **5 种扩展风格** — 极简线、专辑焦点、进度环、可视化、歌词行
- **全局热键** — 切换拖动模式、显示/隐藏、循环切换风格（全屏游戏可用）
- **点击穿透** — 按钮可点击，其余区域穿透
- **HUD 视觉风格** — 半透明暗色面板，大圆角
- **Direct2D 渲染** — 硬件加速，流畅动画
- **系统托盘** — 右键菜单切换风格
- **偏好设置页** — 位于 foobar2000 组件设置

## 快速开始

1. 从 [Releases](https://github.com/Coconutat/floating-clouds---foobar2000-plugus/releases) 下载最新版本
2. 将 `foo_floating_clouds.dll` 复制到 foobar2000 的 `components` 目录
3. 重启 foobar2000 — 悬浮窗口会自动显示
4. 按 `Scroll Lock` 切换拖动模式，`Ctrl+Alt+F` 隐藏/显示

## 构建

需要 Visual Studio 2019+，C++17 支持，foobar2000 SDK（已包含在仓库中）。

```
git clone <repo>
# 在 Visual Studio 中打开 foo_floating_clouds/foo_floating_clouds.vcxproj
# 构建 → foo_floating_clouds.dll
```

## 许可证

详见 [LICENSE](LICENSE)。  

