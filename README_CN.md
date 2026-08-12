# floating clouds - foobar2000 plugus  
**语言**: [ENGLISH](README.md) | **简体中文**
***
Foobar2000 SDK Version: SDK-2025-03-07  
Windows Template Library: WTL10_01_Release    
***

一个 Foobar2000 插件，在桌面或游戏时在显示器某区域显示音乐播放器的悬浮 UI 叠加层。支持专辑封面、歌曲名称、歌手显示和简易播放控制。

## 组件

本仓库包含三个相互独立的 foobar2000 组件：

| 组件 | DLL | 功能 |
| --- | --- | --- |
| **悬浮云 Floating Clouds** | `foo_floating_clouds.dll` | 本文档主体——悬浮播放信息叠加层（见下文） |
| **歌单整理 Playlist Organizer** | `foo_playlist_organizer.dll` | 把活动歌单按专辑艺人整理为多个 A-Z 排序且锁定的艺人歌单 — [README](floating_clouds_organizing_playlists/README_CN.md) |
| **Apple Music 标签 Apple Music Tags** | `foo_floating_clouds_tags.dll` | 按地区获取 Apple Music 标签并写入选中的曲目 — [README](floating_clouds_tags/README_CN.md) |

## 特性

- **悬浮叠加** — 独立置顶窗口，始终保持在桌面或全屏游戏之上
- **6 种风格** — 极简、完整、专辑焦点、进度环、可视化、歌词行
- **Material 3（MD3）设计** — 深色 `surface-container` 圆角卡片 + 柔和投影，四角逐像素真正透明
- **逐像素 alpha 渲染** — UpdateLayeredWindow 呈现路径（驱动无关，规避 AMD DirectComposition 闪烁），含均匀 alpha 回退
- **60fps 流畅动画** — 进度、淡入淡出、按钮状态层、显隐均基于时间缓动（帧率无关）
- **全局热键（可自定义）** — 切换拖动模式、显示/隐藏、循环切换风格（全屏游戏可用）
- **点击穿透** — 按钮可点击，其余区域穿透；拖动模式可移动窗口
- **系统托盘** — 右键菜单切换风格与显隐
- **播放列表面板** — ☰ 按钮打开两级面板（播放列表 → 曲目）
- **歌词** — 内嵌 LRC/纯文本歌词，随播放同步
- **实时可视化** — 平滑 FFT 频谱柱
- **偏好设置页** — 热键、透明度、默认风格、自动隐藏、界面语言、调试日志

## 快速开始

1. 从 [Releases](https://github.com/Coconutat/floating-clouds---foobar2000-plugus/releases) 下载最新版本
2. 将 `foo_floating_clouds.fb2k-component` 安装（或将 `foo_floating_clouds.dll` 复制）到 foobar2000 的 `components` 目录
3. 重启 foobar2000 — 悬浮窗口会自动显示
4. 默认热键：`Ctrl+Alt+D` 切换拖动模式、`Ctrl+Alt+F` 隐藏/显示、`Ctrl+Alt+S` 切换样式

> 所有热键都可在 `文件 > 偏好设置 > 组件 > Floating Clouds` 中自定义：点击热键输入框，
> 再按下新的组合键即可（组合键需包含 Ctrl/Alt/Shift/Win）。

## 截图预览

<table>
  <tr>
    <td align="center"><img src="imgs/style-full.png" width="220" alt="完整"/><br/><b>完整 Full</b></td>
    <td align="center"><img src="imgs/style-minimal.png" width="220" alt="极简"/><br/><b>极简 Minimal</b></td>
    <td align="center"><img src="imgs/style-album-focus.png" width="220" alt="专辑焦点"/><br/><b>专辑焦点 Album Focus</b></td>
  </tr>
  <tr>
    <td align="center"><img src="imgs/style-progress-ring.png" width="220" alt="进度环"/><br/><b>进度环 Progress Ring</b></td>
    <td align="center"><img src="imgs/style-visualizer.png" width="220" alt="可视化"/><br/><b>可视化 Visualizer</b></td>
    <td align="center"><img src="imgs/style-lyrics-line.png" width="220" alt="歌词行"/><br/><b>歌词行 Lyrics Line</b></td>
  </tr>
</table>

## 风格

| 风格 | 说明 |
| --- | --- |
| 极简 Minimal | 单行：播放图标 + 歌名·歌手 + 细进度条，几乎无遮挡，适合全屏游戏 |
| 完整 Full | 小封面 + 歌名/歌手 + 进度条 + 控制按钮 |
| 专辑焦点 Album Focus | 大封面为主视觉，信息与控制在下方 |
| 进度环 Progress Ring | 缩略封面被圆形进度环环绕 |
| 可视化 Visualizer | 实时 FFT 频谱柱 + 曲目信息，游戏 HUD 风格 |
| 歌词行 Lyrics Line | 当前同步歌词行，游戏时余光可见 |

## 系统要求

- foobar2000 v2.0 及以上（Windows 10 64 位）

## 构建

需要 Visual Studio 2019+、C++17 支持，以及 foobar2000 SDK。

foobar2000 SDK **不随本仓库分发**。请从 <https://www.foobar2000.org/SDK> 下载匹配版本（本仓库使用 2025-03-07），解压到仓库根目录的 `SDK/` 文件夹下——工程文件按 `..\SDK\...` 路径引用。使用时遵守其自身许可（`SDK/sdk-license.txt`）。

```
git clone <repo>
powershell -ExecutionPolicy Bypass -File build.ps1            # Release / x64 -> foo_floating_clouds.dll
powershell -ExecutionPolicy Bypass -File build.ps1 -Package   # 并打包 dist\foo_floating_clouds.fb2k-component
```

`build.ps1` 还支持 `-Configuration Debug`、`-Platform Win32`、`-Clean`，以及
`-Deploy -Foobar2000Dir "<foobar2000 根目录>"`。也可在 Visual Studio 中直接打开
`foo_floating_clouds/foo_floating_clouds.vcxproj` 构建。

## 许可证

详见 [LICENSE](LICENSE)。  

