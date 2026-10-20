# Shield Look

Video Setup > FOV and Viewmodels > SHIELD LOOK selects CLASSIC (the default)
or ENHANCED. The former ENHANCED 2 profile is now the sole ENHANCED look.
`display.shield_look = "enhanced"`, or `HALO_SHIELD_LOOK=enhanced`, selects
CMT Snow Grove's plasma pattern and first-person parameters with the approved
small outward body flare on stock Spartan shields. This is a local rendering feature in `port/linux/game/shield_look.c`.
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
noise references point to bitmap 1126, external CE resource 863. The original
CMT shaders use the following inspected values. The regression test compares
all rendering fields, allowing only the approved body displacement below:

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

The original CMT profile uses source C rather than B: CMT has two object
functions, and its unused C output is zero. ENHANCED deliberately replaces only the body's offset parameters as described
below; the first-person parameters are retained. ENHANCED's arm values are separate
from Arm Shields' values, so the latter's displacement cap does not alter
the CMT profile. CMT supplies duplicate arm parts for the plasma layer: all
68-byte vertices in its four shell parts (660, 574, 222 and 39 vertices)
are byte-identical to their underlying arm parts. The feature uses the
engine's modifier pass on the current map's hands.
This preserves that map's geometry and animation. Controlled render
comparisons on identical geometry are recorded below; the feature does not
replace another map's models to make their silhouettes match CMT's.

## Approved ENHANCED profile

ENHANCED now uses the approved former ENHANCED 2 appearance exactly. The
body displacement is outgoing hit intensity B, exponent 0.5 and amount
0.0125 world units (about 3.8 cm at full intensity). The CMT pattern, colors,
animation, brightness and complete first-person shader remain unchanged.
The former skin-tight body option and separate SHIELD LOOK submenu are gone.

Existing `"enhanced2"` config/environment values normalize to `"enhanced"`
at load, so playtest selections retain their appearance and show ENHANCED in
the menu. Both spellings also select the same stable shader in the feature.
There is only one enhanced body copy. CLASSIC still preserves stock behavior.
No shared-engine hook, map-family adapter, geometry mutation or simulation
change is needed; the existing renderer displaces vertices along normals.

## Map authority and isolation

Eligibility is deliberately conservative and independent of map names and
families. A biped qualifies only when its modifier's complete normalized
plasma definition matches the stock cyborg shield, its shield-driving
function and mappings are stock, and both noise references identify the same
verified stock bitmap. Both the body model and the globals' hands must have
only opaque model materials or the two exactly identified stock Chicago
generator-light materials. Their complete normalized definitions, single
texture stages and full DXT1 texture mip chains are checked; tag names are
not trusted. Other plasma shells or transparent materials, alpha-blended
model decals, missing/unknown data and extra object
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
| Stock generator-light definitions; relocated references/blocks and type zeroed | `66e15fe3`, `015c769c` |
| Stock generator-light stage; relocated bitmap reference zeroed | `7fda0175` |
| Generator-light DXT1 texture, 174776 bytes, Xbox / CE / inspected MCC layout | `71b35cd8`, `42b1a420`, `e5e7e894` |

The feature calls the common `cache_file_read` interface. Xbox stays on its
cache stream, CE on its resource-map reader, and MCC on its own prepared
bitmap storage. It does not call an MCC adapter for Xbox/CE, change a map
folder, or modify family detection. Bitmap reads and tag/block accesses are
bounded. A failed read or full 64-entry profile cache keeps the map's look.
The stock shader and function signatures were checked in Xbox
`beavercreek.map`, CE `deathisland.map` and version-13 MCC
`BetterWithFriends/b30.map`; all three match. These chains were subsequently
exercised through their own loaders in the Windows gameplay runs below.

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

## Verification history

`python -m pytest -q tools/test_shield_look.py` compiles the actual feature
module as 32-bit C89 with fake engine services. It covers CLASSIC/invalid
settings, exact profile fields, authored shaders/textures/functions/body and
hands shells/decals, toggling, split screen, reloads, capacity and failed reads.
It also checks generated menus and placement of the draw/lifecycle hooks.
Set `SHIELD_LOOK_REFERENCE_MAP` and `SHIELD_LOOK_STOCK_MAP` to the local CMT
and stock Xbox caches to additionally check exact CMT parameters, all noise
texels, and the actual feature reader's Xbox/CE/MCC-layout fingerprints.
`SHIELD_LOOK_CC` optionally selects a 32-bit compiler wrapper.

Validation on Windows (20 October 2026): 25 Shield Look tests and all 21
existing Arm Shields tests passed, including negative controls. The added
stock-light tests exercise the real generator-light definitions and pixels,
and reject altered materials, stages and textures. The harness used LLVM's
32-bit MSVC target; its existing Unix `-lm` argument was omitted by a local
Windows wrapper because the Microsoft runtime already supplies those calls.

Release, portable Windows, Android guest/host, and web targets built with
LLVM 23.1.4, Visual Studio Build Tools 2022 and Windows SDK, Android NDK r30,
and Emscripten 6.0.10. Windows used the normal project build. The Android and
web builds used local equivalents for generated Unix shell bootstrap/copy
commands and normalized response-file paths; no build-generator changes are
part of this feature. Cross-platform compilation is not device/browser
runtime validation.

Hidden Windows OpenGL runs used isolated data/save folders, at most two
instances, real scripted damage, and bounded copies of the supplied maps.
Original maps were never edited. Script-reference and spawn controls in the
copies made the hits and cameras reproducible; MCC fixtures preserved script
indices while making startup scripts dormant. A temporary diagnostic build
controlled randomness and recorded render-path selection. Its instrumentation
and random-seed change were removed before the final production builds.

The live tests found that the original eligibility filter rejected stock
Spartan generator lights. The exact material/stage/texture checks above fix
that without admitting arbitrary Chicago shield shells.

Visual equivalence was checked separately from eligibility:

- CMT's arm shell has identical vertices **and triangle strips** to the
  underlying hands. On controlled copies with identical geometry, lighting,
  hit timing and interpolation disabled, the original authored first-person
  shell and ENHANCED's modifier pass produced pixel-identical captures.
  Both comparison copies used the same opaque body-underlay control to
  exclude unrelated CMT weapon/lamp materials from eligibility.
- For the body, a temporary diagnostic alternated the original CMT shader
  and the feature's stock-based profile on the same frozen in-game scene.
  Twenty consecutive frame comparisons had zero differing pixels. This
  isolates the shader from different random idle animations or lighting
  phases between processes. The shield was visibly active in the captures.
- Normal runtime captures showed ENHANCED on stock Xbox shields. Render-path
  audits also confirmed both body and hands overrides on stock CE
  `deathisland` and MCC `BetterWithFriends/b30` shield chains.
- Authored magenta/scale variants of those Xbox, CE and MCC maps retained
  their original body shader pointers and received no enhanced arm pass.
  Unaltered CMT body/arm materials also received neither override. Their
  authored appearance remained visible in gameplay.

The visual claim concerns the shield shader, not replacing another map's
armor, hands, animations, lighting or camera with CMT's. Those remain the
map's own. The automated harness covers toggling, split-screen state,
failed reads, reloads and capacity. Live runs cover Xbox UI-to-CE/MCC loads
and shutdown; a full gameplay Xbox-to-CE-to-MCC transition chain, live
split-screen, Android hardware and browser gameplay were not exercised.
No gameplay/network structures were changed.

The final production Windows executable also passed the repository's
two-player network comparison on Beaver Creek, with real rendering,
CLASSIC versus ENHANCED, scripted damage/deaths and swapped host roles.
Both runs compared 22 seconds and passed the existing judge/tolerances
(562 and 660 prediction-drift samples). A local adapter supplied Windows
process handling and isolated file paths; the comparison logic was unchanged.
This does not cover browser transport or internet matchmaking.

ENHANCED 2 follow-up validation: 63 focused tests passed. Both enhanced
settings exercise authored-shield refusal, malformed data, reloads, toggles
and split-screen state. An additional real-module test compares the complete
first-person shader byte-for-byte, checks that only the three body displacement
fields differ, and verifies stable copies across all three settings. Release
Windows, Android guest/host and web builds passed. Hidden Windows OpenGL
runs on a bounded Beaver Creek fixture with scripted damage confirmed the
small body separation at 1280x960; ENHANCED was also captured for comparison.
The initial small offset was retained without increasing it. Existing Xbox,
CE and MCC eligibility checks are unchanged; the previous family-specific
live authority audits were not repeated for this displacement-only extension.
Android/browser runtime and live split-screen remain untested.

The ENHANCED 2 production build passed both host roles in the two-player
Beaver Creek network comparison against CLASSIC, with damage/death events
and real rendering (24 seconds compared per run, existing judge/tolerances).
No networking implementation or version changed.

Profile consolidation: 63 focused tests passed, including exact CMT rendering
fields with only the approved body displacement substituted, both saved
spellings selecting the same shader, unchanged arms, and removal of the
submenu from the generated screens and asset manifest. Earlier ENHANCED
references in this validation history describe the original CMT comparison;
the approved ENHANCED 2 appearance is now the current ENHANCED profile.

The consolidated profile built successfully for Windows, Android guest/host
and web. A hidden Windows OpenGL menu run at 1280x960 confirmed the relocated
row fits and an `enhanced2` environment value displays as ENHANCED. The shader
parameters are identical to the approved profile tested above; gameplay and
network runs were not repeated for this naming/menu consolidation.
