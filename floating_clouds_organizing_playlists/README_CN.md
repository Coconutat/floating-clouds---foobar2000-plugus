# floating_clouds_organizing_playlists — 歌单整理

**Language**: [ENGLISH](README.md) | **简体中文**

一个 foobar2000 组件：把活动歌单按**专辑艺人**整理为多个自动生成的、A-Z 排序且**锁定**的艺人歌单，并为原声带、合辑、未知艺人提供专门的归类。

## 功能

- **按专辑艺人整理**：每首曲目被路由到恰好一个以其 `%album artist%` 命名的目标歌单
- **智能归类**：专辑名含 "soundtrack" → 固定 `Soundtrack` 歌单；`Various Artists` 合辑 → 固定 `Various Artists` 歌单；艺人缺失 → `Unknown Artist`
- **A-Z 排序**：目标歌单按字母序生成；整理时也会把整个歌单列表重新排序
- **目标歌单锁定**：生成的歌单带锁（🔒），不会因误操作被编辑、重排、重命名或删除，但双击仍可播放
- **幂等重建**：重复整理会在原位清空重填——无残留、无重复
- **确定性路由**：单一归属——每首曲目始终只进一个歌单
- **所有歌单 A-Z 排序**：独立的命令，也会对你已有的歌单重新排序
- **界面 EN / 中文** + 深色模式

## 快速开始

1. 安装 `foo_playlist_organizer.fb2k-component`（`文件 > 偏好设置 > 组件 > 安装…`，或把 DLL 拷进 `components` 目录）
2. 重启 foobar2000
3. 右键歌单标签页（或歌单内空白处）→ **按专辑艺人整理此歌单**，或 `文件 > 歌单 > 歌单整理器` → **按专辑艺人整理活动歌单**
4. 目标歌单即被创建（或重建）、A-Z 排序并锁定

> 想对整张歌单列表（含你自己已有的歌单）重新排序：`文件 > 歌单 > 歌单整理器` → **所有歌单按 A-Z 排序**。它只排序名称，绝不创建或改动歌单。

## 路由规则

每首曲目按优先级判定：

1. 专辑名含 "soundtrack"（忽略大小写）→ 固定 `Soundtrack` 歌单
2. 否则 → 以 `%album artist%` 命名的歌单（`Various Artists` 归入固定合辑歌单）
3. 专辑艺人缺失 → 回退到 `%artist%`，再回退到 `Unknown Artist`

## 环境要求

- foobar2000 v2.0 或更高（Windows 10 64 位）

## 构建

需要 Visual Studio 2019+（C++17）与 foobar2000 SDK（已包含）。

```
cd floating_clouds_organizing_playlists
powershell -ExecutionPolicy Bypass -File build.ps1            # Release / x64 -> foo_playlist_organizer.dll
powershell -ExecutionPolicy Bypass -File build.ps1 -Package   # 同时打包 dist\foo_playlist_organizer.fb2k-component
```

## 许可证

详见 [LICENSE](../LICENSE)。
