# Shield Look

Video Setup > SHIELD LOOK selects CLASSIC (the default) or ENHANCED.
`display.shield_look = "enhanced"`, or `HALO_SHIELD_LOOK=enhanced`, selects
CMT Snow Grove's body and first-person plasma parameters on stock Spartan
shields. This is a local rendering feature in `port/linux/game/shield_look.c`.
It does not use the mod loader or change simulation, shield strength, damage,
network messages, map tags, models or cache files. No network-version change
is required.

CLASSIC leaves the established drawing paths intact, including the separate
Arm Shields and Overshield Effect settings. ENHANCED supplies its own arm
profile regardless of Arm Shields and avoids applying that setting a second
time. Overshield Effect remains an independent, opt-in visual modifier; turn
it off when comparing against CMT's unmodified appearance.

## Reference and provenance

The reference is the user-supplied `cmt_Snow_Grove.map`, 131301829 bytes,
SHA-256 `d65da3089cfaf8243f4288bc079326c1ab89a46f8e2f2bed974eb548e5fd8fed`.
It is a stock CE-format cache (609), not an OpenSauce implementation.
The user authorized using its assets. No CMT art, geometry or executable code
is redistributed by this feature: the inspected shaders use the stock
`characters\elite\bitmaps\plasma shield noise` texture. Its entire six-level
32x32x32 mip chain (37449 texels) is identical to the Xbox texture after
addressing and A8Y8-to-ARGB conversion. The existing map therefore supplies
the exact noise data through its existing texture loader.

The multiplayer unit is tag 1880, its body modifier tag 1935; the globals'
first-person hands are model 2157 with plasma material 2163. Both shader
noise references point to bitmap 1126, external CE resource 863. The two
shader copies use the following inspected values (the regression test
compares their rendering fields byte-for-byte with the map):

| Parameter | Body | First-person hands |
| --- | --- | --- |
| Intensity source / exponent | B / 1 | B / 1 |
| Offset source / exponent | C / 2 | C / 2 |
| Offset amount | 0.035 | 0.009 |
| Perpendicular alpha / RGB | 1 / (0.909804, 0.772549, 0) | same |
| Parallel alpha / RGB | 1 / (0.937255, 0.694118, 0.0627451) | same |
| Color source | none | none |
| Primary period / direction | 2.1 / (2, 0, -0.5) | same |
| Primary scale | 18 | 45 |
| Secondary period / direction | 3.2 / (0, 2, 0.5) | same |
| Secondary scale | 14 | 62 |

Do not replace source C with the hit-intensity source B: CMT has two object
functions, and its unused C output is zero. The original shader's offset
parameters are deliberately retained. ENHANCED's arm values are separate
from Arm Shields' values, so the latter's displacement cap does not alter
the CMT profile. CMT supplies duplicate arm parts for the plasma layer: all
68-byte vertices in its four shell parts (660, 574, 222 and 39 vertices)
are byte-identical to their underlying arm parts. The feature uses the
engine's modifier pass on the current map's hands.
This preserves that map's geometry and animation. Matching shader data is
verified; matching rendered output on different geometry is not established
by that check alone.

## Map authority and isolation

Eligibility is deliberately conservative and independent of map names and
families. A biped qualifies only when its modifier's complete normalized
plasma definition matches the stock cyborg shield, its shield-driving
function and mappings are stock, and both noise references identify the same
verified stock bitmap. Both the body model and the globals' hands must have
only opaque model materials. Plasma shells, Chicago/other transparent
materials, alpha-blended model decals, missing/unknown data and extra object
functions preserve the authored look. A modified shader or noise texture
retaining the stock tag name is still rejected. This can conservatively
leave an otherwise stock-looking custom map unchanged.

FNV-1a fingerprints, after the documented normalization:

| Data | Fingerprint |
| --- | --- |
| Stock 0x14c plasma tag; type/pad and two relocated references zeroed | `762fcc1f` |
| Shield function: first 60 bytes plus four runtime reciprocals | `d130b3a8` |
| Xbox swizzled A8Y8 noise, 74898 bytes excluding padding | `f2f70788` |
| CE linear ARGB noise, 149796 bytes | `e7f72da4` |
| The same ARGB noise in MCC's hardware layout | `18ac1e94` |

The feature calls the common `cache_file_read` interface. Xbox stays on its
cache stream, CE on its resource-map reader, and MCC on its own prepared
bitmap storage. It does not call an MCC adapter for Xbox/CE, change a map
folder, or modify family detection. Bitmap reads and tag/block accesses are
bounded. A failed read or full 64-entry profile cache keeps the map's look.
The stock shader and function signatures were checked in Xbox
`beavercreek.map`, CE `deathisland.map` and version-13 MCC
`BetterWithFriends/b30.map`; all three match. This is a data inspection,
not a claim that live map-family transitions have been tested.

## Necessary shared hooks

The existing renderer has no registration point for a private modifier
profile. Three minimal `port:` additions supply the required integration:

- `render_objects.c`: substitute a private body shader at draw submission,
  after the existing modifier-type check. Animation values remain the unit's.
- `first_person_weapons.c`: supply the hands modifier between the weapon and
  hands draws; CLASSIC and ineligible maps follow the existing Arm Shields
  call. The weapon never receives the enhanced modifier.
- `cache_files.c`: clear eligibility and private shader/value copies before
  load and before unload. Failed loads and Xbox/CE/MCC transitions cannot
  reuse an earlier map's handles.

All eligibility, storage and parameter logic stays in the feature module.
Copies are never recycled within a map because deferred transparent draw
packets can retain their addresses. Arm animation values are separate for
each local player. Disabling takes effect on subsequent draw submissions;
the stable copies remain available for already queued packets until teardown.

## Verification

`python -m pytest -q tools/test_shield_look.py` compiles the actual feature
module as 32-bit C89 with fake engine services. It covers CLASSIC/invalid
settings, exact profile fields, authored shaders/textures/functions/body and
hands shells/decals, toggling, split screen, reloads, capacity and failed reads.
It also checks generated menus and placement of the draw/lifecycle hooks.
Set `SHIELD_LOOK_REFERENCE_MAP` and `SHIELD_LOOK_STOCK_MAP` to the local CMT
and stock Xbox caches to additionally check exact CMT parameters, all noise
texels, and the actual feature reader's Xbox/CE/MCC-layout fingerprints.
`SHIELD_LOOK_CC` optionally selects a 32-bit compiler wrapper.

Implementation validation on Windows: these 20 tests passed, along with the
21 existing Arm Shields tests, including their negative controls. The feature
module and three modified engine files compiled against the real project
headers using Zig's 32-bit compiler and a temporary MinGW inline compatibility
shim. This is not a standard MSVC build. Broader checks reported 46 passes,
135 skips (primarily the missing standard clang/cache-report tool) and five
web-graph failures caused by Windows backslashes versus the tests' POSIX path
expectations. No build-generator code was changed.

Full platform builds were attempted: Windows stopped at missing `llvm-rc`;
Android and web targets were unavailable because no NDK or Emscripten was
installed. A separate settings-table compile attempt with MinGW stopped on
its intrinsic declarations conflicting with the project's MSVC ABI headers.
No modified game executable, visual comparison, live map-family
transition or two-instance networking test was possible in this environment.
Those remain required before treating the feature as runtime-validated or
claiming pixel-identical output. No gameplay/network structures were changed.
