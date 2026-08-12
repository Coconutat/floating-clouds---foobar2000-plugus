# floating_clouds_tags — Apple Music 标签更新

**Language**: [ENGLISH](README.md) | **简体中文**

一个 foobar2000 组件：按你选择的**地区**（= 元数据语言）获取 Apple Music 专辑的官方标签，并写入选中的曲目。

> 核心思路：**地区即元数据语言**。`cn` → 简体中文、`hk`/`tw` → 繁體中文、`jp` → 日本語、`us` → English。不做翻译——直接取 Apple 各 storefront 的官方元数据。

## 功能

- **粘贴 Apple Music 链接（或专辑 ID）**：打开对话框自动读取剪贴板，从 URL 提取专辑 ID 与默认地区
- **地区 = 元数据语言**：切换 `CN / HK / TW / JP / US / GB / KR` 即可写入该地区语言的标签；你手动改过下拉后，粘贴链接里的地区不再覆盖你的选择
- **逐字段控制**：勾选要写的标签（标题、专辑、艺人、专辑艺人、流派、发行日期、曲目号、碟号、explicit）
- **默认补空、可选覆写**：默认安全（只补空字段）；「覆写已有」开关可强制替换
- **紧急「强制按选择顺序写入」**：轨道号错乱时，忽略匹配，第 N 首选中 ↔ Apple 第 N 首，从上到下写入（隐含覆写）
- **CN → HK 自动回退**：CN 无此碟时自动改取港版，并**逐字**转简（界面明确标注，非官方简中）
- **手动「转为简体中文」**：对任意已取标签（如港版繁体）按需转简
- **安全写入**：绝不碰时长等音频属性；匹配不上的曲目跳过并汇总（「更新 N / 跳过 M」）
- **界面 EN / 中文** + 深色模式

## 快速开始

1. 构建或下载 `foo_floating_clouds_tags.fb2k-component` 并安装（`文件 > 偏好设置 > 组件 > 安装…`，或把 DLL 拷进 `components` 目录）
2. 重启 foobar2000
3. 复制一张 Apple Music 专辑链接，如 `https://music.apple.com/cn/album/艳阳天/156116977`
4. 在歌单里选中该专辑的曲目，右键 → **从 Apple Music 更新标签…**
5. 对话框内：获取 → 核对解析到的专辑 → 选择字段/覆写 → 应用

## 对话框

| 控件 | 说明 |
| --- | --- |
| 专辑 URL 或 ID | 链接或纯数字专辑 ID（剪贴板自动预填） |
| 地区 | storefront = 元数据语言（在你手动改动前，默认跟随 URL 地区） |
| 获取 | 后台调 iTunes Lookup API（可取消） |
| 要更新的字段 | 逐字段勾选（默认全选，explicit 除外） |
| 覆写已有标签 | 关=只补空；开=强制替换 |
| 强制按选择顺序写入 | 紧急：按选中顺序匹配，隐含覆写 |
| 转为简体中文 | 对已取标签逐字繁→简 |

## CN → HK 回退与逐字转换

- 地区为 **CN** 且该专辑在 CN 不存在时，组件**自动改用 HK storefront**，并把每个文本字段**逐字**从繁体转成简体中文
- 这是**非官方简体本地化**——弹窗与预览都会明确提示"港版繁体逐字转简"，因为个别标题/译名可能与大陆习惯不同
- 明确选择 HK/US/JP 等非 CN 地区时不会触发回退

## 代理

组件使用 foobar2000 SDK 的 `http_client`（由 foobar2000 核心实现），**继承 foobar2000 的全局代理设置**（`文件 > 偏好设置 > 高级`）。插件内不提供单独代理项。

> ⚠ 代理继承尚未在实机验证；若行为不符，再考虑增加插件内代理选项。

## 环境要求

- foobar2000 v2.0 或更高（Windows 10 64 位）
- 可访问 `itunes.apple.com`（全局可访问；`country=jp` 等参数从中国大陆通常也能取到对应地区数据）

## 构建

需要 Visual Studio 2019+（C++17）与 foobar2000 SDK（已包含）。

```
cd floating_clouds_tags
powershell -ExecutionPolicy Bypass -File build.ps1            # Release / x64 -> foo_floating_clouds_tags.dll
powershell -ExecutionPolicy Bypass -File build.ps1 -Package   # 同时打包 dist\foo_floating_clouds_tags.fb2k-component
```

## 许可证

详见 [LICENSE](../LICENSE)。
