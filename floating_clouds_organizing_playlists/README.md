# floating_clouds_organizing_playlists - Playlist Organizer

**Language**: **ENGLISH** | [简体中文](README_CN.md)

A foobar2000 component that organizes an active playlist by **album artist** into generated, A-Z-sorted, **locked** per-artist playlists — with dedicated buckets for soundtracks, various-artist albums, and unknown artists.

## Features

- **Organize by album artist** — every track is routed to exactly one destination playlist named after its `%album artist%`
- **Smart buckets** — albums whose name contains "soundtrack" → a fixed `Soundtrack` list; `Various Artists` albums → a fixed `Various Artists` list; missing artist → `Unknown Artist`
- **A-Z sorted** — destination playlists are created in alphabetical order; an organize run also re-sorts the whole playlist list
- **Locked destinations** — generated playlists are locked (padlock): they can't be accidentally edited, reordered, renamed or deleted, but double-click still plays
- **Idempotent rebuild** — re-running updates the destinations in place (clear + refill): no leftovers, no duplicates
- **Deterministic routing** — single attribution: one track always goes to exactly one playlist
- **Sort all playlists A-Z** — a standalone command that also reorders your pre-existing playlists
- **EN / 中文 UI** + dark mode

## Quick Start

1. Install `foo_playlist_organizer.fb2k-component` (`File > Preferences > Components > Install...`, or copy the DLL into your `components` folder)
2. Restart foobar2000
3. Right-click a playlist tab (or inside a playlist) → **Organize this playlist by album artist**, or use `File > Playlist > Playlist Organizer` → **Organize Active Playlist by Album Artist**
4. Destination playlists are created (or rebuilt), A-Z sorted, and locked

> To reorder the whole playlist list (including your own playlists): `File > Playlist > Playlist Organizer` → **Sort all playlists A-Z**. This only sorts names — it never creates or alters playlists.

## How routing works

Priority per track:

1. Album name contains "soundtrack" (case-insensitive) → fixed `Soundtrack` playlist
2. Otherwise → a playlist named after `%album artist%` (`Various Artists` gets its own fixed bucket)
3. Missing album artist → falls back to `%artist%`, then `Unknown Artist`

## Requirements

- foobar2000 v2.0 or later (Windows 10 64-bit)

## Build

Requires Visual Studio 2019+ with C++17 support and the foobar2000 SDK (included).

```
cd floating_clouds_organizing_playlists
powershell -ExecutionPolicy Bypass -File build.ps1            # Release / x64 -> foo_playlist_organizer.dll
powershell -ExecutionPolicy Bypass -File build.ps1 -Package   # also package dist\foo_playlist_organizer.fb2k-component
```

## License

See [LICENSE](../LICENSE) for details.
