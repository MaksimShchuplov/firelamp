# Roadmap — deferred work and closed questions

Nothing here is scheduled. Each item records enough context to pick it up
later, or to avoid re-proposing something that was already considered and
turned down. Status is one of **paused** (wanted, not now), **deferred**
(waiting on a trigger), **open question** (needs information), or
**rejected** (decided against — read before proposing it again).

---

## Paused

### Measurement-driven tuning instead of tuning by eye

**Status: paused.** Wanted in principle, not obviously worth the effort yet.

Today the five fire parameters are tuned by adjusting one slider, waiting,
and looking at the lamp. The feedback loop is tens of seconds, two settings
cannot be compared side by side, and the parameters interact. Good results
are therefore accidental — which is how the palette seams (see *Rejected*)
came to matter without anyone deciding they should.

Three stages, each useful alone:

1. **Simulator.** A host-side script running the shipped model — cooling →
   convection → sparks → temporal blend → palette — rendering to video. Any
   parameter set becomes visible in seconds with no hardware and no OTA, and
   a contact sheet (e.g. 5×5 over sparking × blend) makes comparison direct.
   Take the palette from `fire_math.h` so colours match the lamp exactly; the
   three physics steps would have to be mirrored, so pair it with a
   `test_consistency.py` check against `fire.cpp` the way the UI mirrors are
   already guarded. No firmware change, no risk to the tuned look.

2. **A numeric target.** Measure a real fire on video and compute the same
   statistics already measured for the lamp, so "looks right" becomes a
   distance:

   | Statistic | Lamp (measured) | Real fire |
   |---|---|---|
   | Autocorrelation of luminance falls to 0.5 | 540 ms | ~240 ms (f ≈ 1.5/√D → 4.2 Hz at ⌀125 mm) |
   | Correlation across 4 LED rows | 0.73 | lower — tongues move independently |
   | Variance by height: tip / core | 20% / 2.5% | same shape — already matches |
   | Rise velocity of structures | not measured | 0.5–1 m/s (buoyancy) |

3. **Search.** Sweep the parameter grid in the simulator, rank by distance to
   the target, present the best handful for the eye to choose from.

Likely outcome: sparking and blend close most of the gap with no firmware
change at all (the lamp currently runs about half the flicker rate of a real
flame of its diameter). If something cannot be reached by parameters, the
search says *what* is unreachable, which turns a guess into a specification.

---

## Deferred — waiting on a trigger

### Decouple the dark filaments from the contrast slider
**Trigger: moving the contrast slider visibly changes the texture, not just
the colour — most likely near contrast 90–100, where the effect vanishes.**

The palette seams render as dark filaments (see *Rejected → normalising the
palette seams*). Their palette index is fixed, but the *heat value* it
corresponds to follows the contrast gamma curve:

| Contrast | 20 | 35 | 50 | 65 | 80 | 100 |
|---|---|---|---|---|---|---|
| Dark seam at heat | 17 | 54 | 86 | 111 | 130 | — none — |
| Cells per frame | 2.4 | 2.7 | 2.5 | 2.2 | 2.0 | 0 |

Density is stable across most of the range; the *location* is not, and at
contrast 100 the curve is steep enough to skip the boundary entirely, so the
filaments disappear.

Fix would be a dark band defined as an explicit fraction of the heat range,
with its own parameter, added on top of the existing seams — default 0 keeps
the palette bit-identical. Cost is the full new-parameter checklist in
`CODING.md`: eight firmware files plus six UI files, an NVS key, and a preset
field. Not worth it until the coupling actually bites.

### Per-cell cooling ceiling
**Trigger: the horizontal banding becomes the thing that bothers you.**

`coolMax[y]` is one value per row, refreshed every 5 s. The cooling *amount*
is already per cell (`random8(cmax)` inside the x loop), but that is white
noise and the temporal blend erases it; the per-row ceiling is slow and
constant along a row, so it is the structure that survives to the eye — hence
horizontal rings. Widening `coolMax` to `[ROWS][COLUMNS]` would break them
up. Small code change, large and irreversible visual change.

### OTA from the immutable per-build release
**Trigger: an OTA fails its MD5 check in the field.**

`concurrency: publish-firmware` plus the forced version refresh before
download closed the practical race. A single publish is still non-atomic —
`firmware.bin` and `version.json` are replaced in separate requests — leaving
a seconds-wide window. The complete fix is to download from the immutable
`build-<stamp>-<sha>` release rather than the rolling `latest`, which means
emitting the tag in `version.json` and building the firmware URL from it — a
change to the OTA URL contract.

### `/surprise` blocks Core 1 for up to 25 s
**Trigger: something other than MQTT keepalive is actually harmed by it.**

`setKeepAlive(60)` removed the broker disconnect, which was the only
demonstrated consequence. Making the handler asynchronous (202 + result
cached for the next poll) is a real change to the endpoint contract and the
UI, for a problem that currently manifests as nothing.

### Extend `fire_math.h` to convection and cooling
**Trigger: someone is editing those formulas anyway.**

The palette and brightness math moved into `fire_math.h`, so the tests now
exercise shipped code. Convection and cooling are still mirrored in
`test_main.cpp`, and `154`/`102` remain inline numbers — a `CODING.md`
violation. Left alone deliberately: the formulas are correct as shipped, the
`y == 1` edge clamp carries a do-not-touch note, and bundling this into a
commit that touches fire behaviour would muddy a bisect.

### Keyboard access to preset long-press actions
Save-to-filled-slot and delete are pointer-only. A keyboard route needs a new
visible affordance, which is a UI design decision, not a surgical fix.

---

## Open questions

### HARDWARE.md geometry does not add up
A row of 20 LEDs at 144 LED/m spans 139 mm, but a ⌀125 mm cylinder has a
393 mm circumference — a row cannot wrap it. Likewise 40 rows at that pitch
give 278 mm, against the documented "≥ 45 cm tall" frame. The lamp works, so
the real build differs from the document. Needs the actual diameter, strip
density, and whether the strip runs in rows or columns; the file cannot be
corrected by calculation alone.

### PSU headroom
The OTA progress bar used to hold all 800 LEDs near white-hot across the
reboot, which brownout-looped the lamp once in the field; that peak is gone.
If `GET /info` ever reports `"reset":"brownout"` during normal operation, the
current margin is genuinely thin and the wiring — conductor size, injection
points — is worth re-measuring. `PSU_MAX_MA 20000` is empirically tuned and
must not be lowered to the PSU datasheet without measuring on hardware.

---

## Rejected — do not re-propose without new information

### Normalising the palette seams
`heatRamp()` selects branches with `>` while `ramp` wraps to 0 at multiples
of 0x40, so the boundary values collapse to zero intensity — palette indices
86 and 171–172 at contrast 50. This does not match FastLED's `HeatColor` and
reads as a bug. It has been "fixed" twice and reverted twice.

It is what the lamp's look was tuned against, and the mechanism is sound: the
darkened entries are selected by heat *value*, and the heat field is smooth,
so they land along an iso-thermal contour. Measured on the model at defaults
over 1000 frames: ~2.3 cells/frame hit index 86, present in 81% of frames,
and 16% have an adjacent hit against ~2% expected from a uniform scatter —
8× more clustered than noise. That clustering is why it survives the temporal
blend and the convection blend, which both erase uncorrelated per-cell noise.
On the lamp it reads as thin dark filaments drifting with the flame.

Both seams are pinned by tests in `test_main.cpp`.

### Replacing the seams with per-cell "soot" in the simulation
Proposed as the physically-motivated version of the above, then withdrawn.
Uniformly scattered random cells are exactly the white noise the temporal and
vertical blends erase — the same reason the existing per-cell random cooling
contributes no visible texture. Matching the current effect would require
spatially-correlated drifting noise, which is more machinery and a coarser,
slower look: patches instead of filaments. Strictly worse for more work.

### Lowering `PSU_MAX_MA` to the PSU rating
20 A sits above the 12 A supply on purpose. Voltage drop along the 5.5 m
strip keeps measured current within what the supply delivers, while a
datasheet-correct cap visibly clips maximum brightness. Empirical, tuned on
the real build.

### Normalising the `y == 1` convection edge clamp
Row 1 is a full-intensity copy of row 0 by design. The default cooling and
sparking values were dialled in against it. Changed once, reverted in
`db4fe05`.
