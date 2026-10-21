# Shield Look

Video Setup > FOV and Viewmodels > SHIELD LOOK selects CLASSIC (the default)
or ENHANCED. The former ENHANCED 2 profile is now the sole ENHANCED look.
`display.shield_look = "enhanced"`, or `HALO_SHIELD_LOOK=enhanced`, selects
CMT Snow Grove's plasma pattern and first-person parameters with the approved
small outward body flare, overriding map-authored shield modifiers. This is a local rendering feature in `port/linux/game/shield_look.c`.
It does not use the mod loader or change simulation, shield strength, damage,
network messages, map tags, models or cache files. No network-version change
is required.

CLASSIC leaves the established drawing paths intact. Arm Shields is
now authoritative for the added first-person pass in every SHIELD LOOK
mode: OFF adds no arm shield; ON supplies the stock displacement and its
distance/FOV-based cap. ENHANCED restyles that pass with the CMT pattern and
colors while preserving its animation values and overshield handling.
CLASSIC preserves authored map shields. ENHANCED replaces authored shield
modifiers and omits their duplicate plasma model shells. Overshield Effect remains an
independent, opt-in visual modifier.

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
functions, and its unused C output is zero. ENHANCED uses the approved body
displacement below; first-person displacement follows the stock modifier
and Arm Shields cap. ENHANCED retains Arm Shields' animation-values pointer,
so its displacement cap and overshield handling apply to the enhanced
profile. CMT supplies duplicate arm parts for the plasma layer: all
68-byte vertices in its four shell parts (660, 574, 222 and 39 vertices)
are byte-identical to their underlying arm parts. The feature uses the
engine's modifier pass on the current map's hands.
This preserves that map's geometry and animation. Controlled render
comparisons on identical geometry are recorded below; the feature does not
replace another map's models to make their silhouettes match CMT's.

## Approved ENHANCED profile

ENHANCED now uses the approved former ENHANCED 2 appearance exactly. The
body displacement is outgoing hit intensity B, exponent 0.5 and amount
0.0125 world units (about 3.8 cm at full intensity). The CMT pattern, colors, animation and brightness remain unchanged.
The first-person offset now follows Arm Shields, as described below.
The former skin-tight body option and separate SHIELD LOOK submenu are gone.

Existing `"enhanced2"` config/environment values normalize to `"enhanced"`
at load, so playtest selections retain their appearance and show ENHANCED in
the menu. Both spellings also select the same stable shader in the feature.
There is only one enhanced body copy. CLASSIC still preserves stock behavior.
No shared-engine hook, map-family adapter, geometry mutation or simulation
change is needed; the existing renderer displaces vertices along normals.

## Map authority and isolation

CLASSIC returns the map's original shaders and submits its authored model
materials unchanged. ENHANCED (including migrated `enhanced2`) takes precedence
over authored shield modifier parameters and plasma shell materials on Xbox,
CE and MCC. Shader/function fingerprints and unrelated transparent model
materials no longer veto the selected appearance. Hit timing and the map's
intensity channel remain unchanged; appearance is applied at draw time.

The selected profile keeps the approved body parameters and the established
Arm Shields stand-off (0.03, exponent 2), with its existing distance/FOV cap.
Arm Shields OFF also suppresses authored plasma hand shells under ENHANCED;
CLASSIC keeps the original map behavior. Visors, generator lights, decals and
other non-plasma materials are retained, since transparency alone does not
identify a shield. No map tags or geometry are rewritten.

The exact stock/CMT noise is found among the loaded map's bitmap tags using
bounded reads and full-mip fingerprints: Xbox A8Y8 `f2f70788`, CE linear ARGB
`e7f72da4`, MCC packed ARGB `18ac1e94`. This prevents custom modifier noise
from changing the selected pattern. Each family retains its existing resource
loader; no foreign map or mod is loaded. Missing/malformed plasma data, no
matching noise resource, or exhausted profile capacity retains the original
shader safely. Other valid modifier types receive the same private plasma profile; the
animated overshield resolves its intensity channel from that selected profile.

Resource discovery and private shader copies reset before map load/unload.
CLASSIC bypasses discovery and overrides; selecting ENHANCED again reuses
stable per-map copies. Original map resources remain owned by their loaders.

## Necessary shared hooks

The existing renderer has no registration point for a private modifier
profile. Minimal `port:` additions supply the required integration:

- `render_objects.c`: substitute a private body shader at draw submission,
  after the existing modifier-type check. Animation values remain the unit's.
- `first_person_weapons.c`: supply the hands modifier between the weapon and
  hands draws; CLASSIC and ineligible maps follow the existing Arm Shields
  call. The weapon never receives the enhanced modifier.
- `models.c`: omit authored plasma shell parts belonging to a selected body
  or hands profile before their normal transparent submission. A modifier-only
  replacement cannot suppress these separately submitted model materials.
  CLASSIC and unrelated models/materials retain their existing paths.
- `cache_files.c`: clear eligibility and private shader/value copies before
  load and before unload. Failed loads and Xbox/CE/MCC transitions cannot
  reuse an earlier map's handles.

All eligibility, storage and parameter logic stays in the feature module.
Copies are never recycled within a map because deferred transparent draw
packets can retain their addresses. Arm animation values are separate for
each local player. Disabling takes effect on subsequent draw submissions;
the stable copies remain available for already queued packets until teardown.

## Verification history

The earlier authority-refusal results below describe the previous policy.
The current precedence policy supersedes those eligibility expectations.

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

## Arm Shields authority and offset

The approved third-person profile is unchanged. ENHANCED's first-person
shader now retains the validated stock modifier's offset source B, amount
0.03 and exponent 2 instead of CMT's unused C displacement. Its pattern,
colors, intensity and noise animation still use the CMT arm profile.

The existing `viewmodel_shield_apply` runs first and remains responsible for
activation, animation values, arm distance and the renderer's FOV-dependent
cap. `shield_look_arms` only replaces its shader on eligible stock maps,
leaving colors and the values pointer intact. OFF leaves no pass to replace.
The minimal `port:` hook reorder in `first_person_weapon_draw` is necessary
because Arm Shields owns the geometry-distance information and pass setup;
no new renderer hook or duplicate displacement calculation was introduced.
CLASSIC and authored-map fallback retain the original implementation.

Validation: 65 focused tests passed, including real Arm Shields renderer-cap
and negative-control tests, enabled/disabled enhanced arms, exact profile
fields and preserved values/color pointers. Windows, Android guest/host and
web release builds passed. Runtime checks and evidence are recorded below.

Hidden Windows OpenGL runs on the same bounded Beaver Creek fixture captured
the old skin-tight ENHANCED arms, the new ENHANCED arms with Arm Shields ON,
ENHANCED with Arm Shields OFF, and CLASSIC with Arm Shields ON at 1280x960.
OFF showed no added first-person shield during scripted hits. The shader
pattern remains CMT's, so apparent brightness differs from CLASSIC even
though the offset source, amount, exponent and cap are the same. Original
maps and saves were untouched. The first-person hook is shared by Xbox, CE
and MCC; the unchanged conservative eligibility cases pass in the harness.
Earlier family-specific live authority audits were not repeated for this
pass-order change. Android/browser gameplay and live split-screen were not
exercised; split-screen values and FOV-limit behavior have harness coverage.

The final production Windows executable passed both host roles in the
Beaver Creek two-player comparison with CLASSIC versus ENHANCED, Arm Shields
ON for both, scripted damage/deaths and real rendering (24 seconds compared
per run, unchanged judge/tolerances). No network protocol or version changed.

## Animated overshield layer colours

Visible Overshields remains independently optional. It now follows the
unit's current shield vitality: above 2.0 it is green (78, 253, 116), and
through 2.0 it is reddish-pink (253, 78, 116). The existing charging rise
remains active while charging; once charging is over, the persistent effect
ends at normal shield strength (1.0). Body and arm draws use the same
per-unit layer snapshot. First-person animation additionally requires Arm
Shields. Original shader brightness, animation speed and geometry are retained.

The layer selection and snapshot live entirely in `overshield_effect.c`.
No source hook, map-family adapter, network message or gameplay value changed.
66 focused tests passed, including actual effect-state refresh, exact 2.0
boundary, charge/drain direction, body/arm colour agreement, brightness,
expired/dead units and both setting guards. Windows, Android guest/host and
web release builds passed. Hidden Windows OpenGL runs used a real overshield
pickup in a bounded Beaver Creek fixture: logged shields charged through
1.97 to 2.97, then scripted damage reduced 2.91 to 1.29. Captures confirmed
green then reddish-pink on body and arms, and the effect ended when shields
were exhausted. Arm Shields OFF kept the arms clear; Visible Overshields
OFF also removed the persistent first-person effect at full charge. Original maps/saves
were untouched. Android/browser gameplay, CE/MCC live maps, live split-screen
and network sessions were not repeated for this visual-only colour change.

## Selected-look precedence (21 October 2026)

The current policy replaces the earlier authored-shield veto. CLASSIC is
still the default and returns the original shader. ENHANCED and its legacy
alias now replace valid authored modifiers; the original approved profile
parameters are unchanged. The material hook also omits plasma shells and
transparent parts using the authored modifier on registered biped/hands
models. Opaque surfaces and unrelated transparent materials stay intact.
The existing Arm Shields and animated Overshield controls retain ownership.

69 focused tests pass, including an exact byte comparison of the approved
rendering fields against the CMT reference, authored shader/noise/function
cases, non-plasma modifiers, CLASSIC restoration, shell suppression,
Arm Shields OFF, the actual overshield intensity-channel resolver, missing
resources, cache reset and capacity. Windows, Android guest/host and web
release builds pass. Only the FOV and Viewmodels menu help XML changed when
regenerated from the settings generator.

Windows OpenGL comparisons use the previous commit `657806db` and the new
build at 1280x960. Stock Beaver Creek CLASSIC/body, ENHANCED/body and
ENHANCED/arms retain their appearance. Disposable cyan-shield variants of
Xbox Beaver Creek and CE Death Island show the authored cyan look with
CLASSIC and the approved gold look with ENHANCED. CMT Snow Grove retains
its authored arm layer with CLASSIC; ENHANCED replaces it, and Arm Shields
OFF suppresses it. Frames come from separate runs, so animation phase is
not pixel-identical; profile equality is checked by the regression test.

The MCC BetterWithFriends/b30 fixture disables its introductory scripts and
uses a steady outgoing B function for the material comparison, because the
scripted damage cue did not reduce shields. This checks material precedence,
not MCC damage behavior. The previous ENHANCED path and the new CLASSIC
control retain cyan; the new ENHANCED path draws the approved gold profile.
Test-only script references must be aligned to the
map's virtual tag addresses, not file offsets. Rejected initial fixtures and
local-network launch failures are retained in diagnostic logs and are not
counted as successful comparisons. Both builds emitted MCC resource/audio
diagnostics; this is not a full campaign compatibility claim.

No original map or save was edited. Temporary runs are removed after capture,
and test saves are archived with verified hashes. Android/browser gameplay,
live split-screen, and two-machine networking were not repeated. Production
changes only affect local rendering and menu help, with no protocol or
simulation changes.
