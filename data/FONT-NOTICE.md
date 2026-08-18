<!-- carts/jetchess/data/FONT-NOTICE.md -->
# Typefaces

JetChess draws its text from two typefaces, through the two font paths the
console offers. Neither of the fonts themselves is redistributed: what ships is
rendered glyph bitmaps. They are named here because attribution is owed whether
or not a licence compels it.

| where | face | by | licence |
|---|---|---|---|
| `data/gfx/0.gfx`, four cuts | Fontin (Regular, Bold) | Jos Buivenga (exljbris) | free for personal and commercial use; see `assets/fonts/fontin/ReadMe.txt` |
| `data/font.rom`, bank 0 | Press Start 2P | CodeMan38 | SIL Open Font Licence 1.1 |
| `data/font.rom`, bank 3 | monogram | Vinícius Menézio (datagoblin) | CC0 1.0 — public domain |

**Fontin** is the interface's voice: four sizes of one humanist face, drawn for
small text, baked into sprite cells by the Sprite Font Lab from the recipe in
`data/gfx/fonts.ron`. It is a proportional face with anti-aliased edges, which
is the whole reason the panel reads the way it does.

**Press Start 2P** sets the logotype, and only the logotype. It is the console's
own voice, and a wordmark is allowed one of its own.

**monogram** is not the cart's choice: Font ROM bank 3 is reserved by the
platform for the workstation's terminal console, and a project that leaves it
empty has it filled back in (`src/core/loader.rs`). JetChess draws nothing with
it. Banks 1 and 2 are empty, which is the whole of what changed here: this cart
used to bake four faces into the ROM and now bakes the one it sets a word in.

## What ships, and what does not

Neither `.ttf` is part of a published cart. `data/gfx/0.gfx` holds rasterised,
palette-quantised glyph *pictures* — the same kind of artwork the pieces are —
and `data/font.rom` is a regenerable 1-bit cache that the export tool rebuilds
from the source `.ttf` and prunes from distribution. Both are made from the
fonts in the Jet64 workspace under `assets/fonts/`, which is where the licences
live and where they stay.

Press Start 2P's OFL explicitly permits embedding, modification and
redistribution. Fontin's licence is the narrower of the two: it forbids
redistributing *the font*, and is silent on raster art set in it — which is the
reading this cart takes, the same one `carts/jetquest` takes. If that reading is
ever revisited, swapping the face is four `source_path` lines in
`data/gfx/fonts.ron` and a re-bake; nothing in the cart's C names a typeface.
