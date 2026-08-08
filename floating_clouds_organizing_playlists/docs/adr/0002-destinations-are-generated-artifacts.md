# 0002 — Destination playlists are generated artifacts (rebuilt on every run)

Destination playlists (artist-named, plus `Soundtrack`, `Various Artists`,
`Unknown Artist`) are treated as generated artifacts: if a destination already exists,
the organizer clears it and refills it from the current source run. We rejected
merge-with-dedup and skip-if-exists.

Why: destinations are the *output* of the organizer, not user-owned data. Rebuilding
keeps runs idempotent — tracks removed from the source don't linger, and a second run
never accumulates duplicates. The cost is that a manually-created playlist whose name
happens to equal an album artist is overwritten; we accepted this because such names
almost always come from previous organizer runs.
