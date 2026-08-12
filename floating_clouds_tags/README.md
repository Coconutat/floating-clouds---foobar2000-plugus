# floating_clouds_tags - Apple Music Tags

**Language**: **ENGLISH** | [简体中文](README_CN.md)

A foobar2000 component that fetches the official Apple Music album tags for the **storefront region you pick** and writes them to the selected tracks.

> Key idea: the **region is the metadata language**. `cn` → 简体中文, `hk`/`tw` → 繁體中文, `jp` → 日本語, `us` → English. No translation engine — you get Apple's own per-storefront metadata.

## Features

- **Paste an Apple Music link (or album ID)** — the clipboard is read automatically when the dialog opens; the album ID and a default region are extracted from the URL
- **Region = metadata language** — switch `CN / HK / TW / JP / US / GB / KR` to write tags in that storefront's language; the dropdown is never overridden by the pasted URL once you've touched it
- **Field-level control** — pick which tags to write (title, album, artist, album artist, genre, release date, track #, disc #, composer, copyright, total tracks, total discs, explicit)
- **Fill-empty by default, overwrite on demand** — safe by default; a global "overwrite existing" toggle forces replacement
- **Emergency "force write in selection order"** — for broken track numbers: writes the Nth selected track with the Nth Apple track's tags, top to bottom, ignoring track-number matching (implies overwrite)
- **CN → HK fallback** — if an album doesn't exist on the CN storefront, automatically fetches the HK edition and converts it **character-by-character** to Simplified Chinese (clearly flagged in the UI; not official CN localization)
- **Generic "Convert to Simplified Chinese"** — one click converts any Traditional-source fetched tags (HK/TW etc.) to Simplified; the preview hints "Traditional Chinese detected" when applicable
- **Local tag conversion** — a standalone "Convert Selected Tags to Simplified Chinese…" command converts your tracks' existing Traditional local tags to standard Simplified, with no Apple fetch involved (confirms first; reports "converted N / skipped M")
- **Safe writes** — duration/audio properties are never touched; unmatched tracks are skipped and reported ("updated N / skipped M")
- **EN / 中文 UI** + dark mode

## Quick Start

1. Build or download `foo_floating_clouds_tags.fb2k-component` and install it (`File > Preferences > Components > Install...`, or copy the DLL into your `components` folder)
2. Restart foobar2000
3. Copy an Apple Music album link, e.g. `https://music.apple.com/cn/album/艳阳天/156116977`
4. Select the album's tracks in a playlist, right-click → **Update Tags from Apple Music…**
5. In the dialog: Fetch → confirm the parsed album → choose fields/overwrite → Apply

## Dialog

| Control | Meaning |
| --- | --- |
| Album URL or ID | link or bare numeric album ID (clipboard prefilled) |
| Region | storefront = metadata language (defaults to the URL region until you change it) |
| Fetch | background lookup via the iTunes Lookup API (cancelable) |
| Fields to update | per-field checkboxes (all checked except Explicit) |
| Overwrite existing tags | off = fill empty only; on = force replace |
| Force write in selection order | emergency: positional matching, implies overwrite |
| Convert to Simplified Chinese | generic toggle: char-by-char 繁→简 of any Traditional fetched tags; preview hints when detected |

## Traditional→Simplified (independent) & CN → HK fallback

- **T2S conversion is an independent capability** (Windows NLS char-by-char mapping, not official localization): the dialog's "Convert to Simplified Chinese" toggle converts any **Traditional-source** fetched tags (HK/TW etc.); the preview hints "Traditional Chinese detected" when applicable.
- **Local tag conversion**: the standalone "Convert Selected Tags to Simplified Chinese…" menu command converts your tracks' existing Traditional local tags to standard Simplified (all text fields; numeric/empty/already-Simplified skipped; confirms first; reports "converted N / skipped M").
- **CN → HK fallback** (data-source behavior): when region is **CN** and the album is not available there, the component **automatically retries the HK storefront** and applies the T2S conversion. The real source storefront (hk) and whether T2S was applied are tracked separately.
- This is **not official CN localization** — a popup and the preview explicitly say the tags were converted, because some titles or names may differ from mainland conventions.
- Picking any non-CN region (HK/US/JP…) never triggers the CN fallback.

## Proxy

The component uses the foobar2000 SDK `http_client` (implemented by the foobar2000 core), which **inherits foobar2000's global proxy settings** (`File > Preferences > Advanced`). There is no per-plugin proxy field.

> ⚠ The proxy inheritance is not yet verified on a live machine — if it does not behave, a per-plugin proxy option will be considered.

## Requirements

- foobar2000 v2.0 or later (Windows 10 64-bit)
- Internet access to `itunes.apple.com` (globally reachable; `country=jp` etc. works from mainland China for most regions)

## Build

Requires Visual Studio 2019+ with C++17 support and the foobar2000 SDK.

The foobar2000 SDK is **not bundled** with this repository. Download the matching SDK version (2025-03-07) from <https://www.foobar2000.org/SDK> and extract it into an `SDK/` folder at the repository root — the project files reference `..\SDK\...` paths. Use it under its own license (`SDK/sdk-license.txt`).

```
cd floating_clouds_tags
powershell -ExecutionPolicy Bypass -File build.ps1            # Release / x64 -> foo_floating_clouds_tags.dll
powershell -ExecutionPolicy Bypass -File build.ps1 -Package   # also package dist\foo_floating_clouds_tags.fb2k-component
```

## License

See [LICENSE](../LICENSE) for details.
