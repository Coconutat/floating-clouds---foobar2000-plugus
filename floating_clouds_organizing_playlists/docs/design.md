# Playlist Organizer — Design

Independent foobar2000 component (`floating_clouds_organizing_playlists`) that organizes
one source playlist by album artist into generated destination playlists.

Grilled 2026-08-08 (grill-with-docs / domain-modeling). Glossary: `CONTEXT.md`; decisions:
`docs/adr/0001`, `docs/adr/0002`.

## Decisions

| # | Question | Decision |
| --- | --- | --- |
| 1 | Delivery | Independent foobar2000 component (own DLL, uses SDK `playlist_manager`) |
| 2 | Grouping key | `%album artist%` (album artist), not `%artist%` |
| 3 | Soundtrack match | `%album%` contains "soundtrack" (case-insensitive, per track) |
| 4 | Precedence | Soundtrack-first; each track → exactly one destination |
| 5 | Source | The active playlist only; never modified |
| 6 | Destination collision | Rebuild (clear + refill); also applies to `Soundtrack` |
| 7 | Source handling | Left intact; `Soundtrack` playlist fixed name |
| 8 | Compilations / missing tags | `Various Artists` → fixed playlist; missing album artist → `%artist%` → `Unknown Artist` |
| 9 | Entry points | Main-menu item + Playlist context-menu item, shared core; preferences with EN/CN switch |
| 10 | Feedback | Localized summary popup (EN/CN) |
| 11 | Empty groups | Not created |
| 12 | Ordering | Destination keeps source order |
| 13 | Sorting | Destinations created in A-Z order (case-insensitive) |
| 14 | Lock | Generated destinations are locked (foobar2000 playlist lock); unlocked before rebuild, relocked after |

## Routing algorithm (per track, in source order)

1. album name contains "soundtrack" → `Soundtrack`
2. else `%album artist%` == "Various Artists" → `Various Artists`
3. else `%album artist%` empty → use `%artist%`; if that's also empty → `Unknown Artist`
4. else → playlist named after the album artist

Each track lands in exactly one destination (ADR 0001).

## Rebuild semantics

Destinations are cleared and refilled from the current source each run (ADR 0002);
empty groups are skipped. Source order is preserved within each destination. The
source playlist is never modified.

Destinations are created in **A-Z order** (case-insensitive by name) and then
**locked** via foobar2000's `playlist_lock` (blocking add/remove/reorder/rename/
replace/remove-playlist; double-click playback still works), so the hundreds of
per-artist playlists stay alphabetically ordered and untouched between runs. The
organizer unlocks each destination before rebuilding it and relocks it after.

## Entry points / UI

- Main menu item and Playlist context-menu item both invoke the same core
  "organize active/right-clicked playlist" operation.
- Preferences page: EN/CN language switch (pattern: `tr()` + `cfg_guids::language`,
  as in `foo_floating_clouds/localization.h`). Menu labels and the summary popup are
  localized; destination playlist names stay fixed English constants.
