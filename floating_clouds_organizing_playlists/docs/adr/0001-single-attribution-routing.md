# 0001 — Single-attribution routing (Soundtrack-first)

Every track in the source playlist is routed to exactly **one** destination playlist.
Routing priority: (1) album name contains "soundtrack" (case-insensitive) → the fixed
`Soundtrack` playlist; (2) otherwise group by album artist (`Various Artists` is its own
bucket, missing album artist falls back to track artist, then `Unknown Artist`).

We rejected multi-attribution (adding a track to both its artist playlist and
`Soundtrack`) and artist-first ordering.

Why: a single, deterministic destination keeps the output predictable and free of
duplicate entries, and matches the user's intent that soundtrack albums are pulled out
into their own list. Routing is hard to reverse later — changing it means regenerating
every existing destination playlist under new rules.
