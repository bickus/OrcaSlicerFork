# Upstream 2.3.2 / 2.4.0-beta Backlog

Comparison worktree: `/ai/orcaSlicerFork-v2.4.0-beta` at `fc9a8aa93f` (`v2.4.0-beta`).

Current fork branch at time of review: `claude/fix-print-time-estimation-veeqO`.

Context: this fork is tuned around a high-speed Voron/Klipper/PETG workflow, with custom seam/ooze behavior and active Klipper print-time-estimation work. The IDs below are stable handles for discussing/importing individual items later.

ID prefixes:

- `U*`: general upstream items worth knowing about.
- `Q*`: quality/speed recovery items, especially relevant because ultrahigh speed amplifies minor path/extrusion/cooling defects.
- `S*`: seam, travel, ooze, retraction, and path-ordering items.
- `K*`: Klipper, JD, acceleration, jerk, estimator, and calibration items.
- `P*`: preview, diagnostics, and visualization items.
- `L*`: lower-priority or probably-skip items.
- `UI*`: explicit UI preservation constraints / disliked upstream UI changes.

## Hard UI Constraints

### UI1: Keep the old main-view printer/nozzle/filament selector UI

- User preference: do **not** import the upstream compact/new selector UI around the main view's printer, nozzle, bed, and filament selector area.
- Primary files to protect: `src/slic3r/GUI/Plater.cpp`, `src/slic3r/GUI/Plater.hpp`, `src/slic3r/GUI/PresetComboBoxes.cpp`, `src/slic3r/GUI/PresetComboBoxes.hpp`, `src/slic3r/GUI/Widgets/ComboBox.*`, `src/slic3r/GUI/BitmapComboBox.*`.
- Main upstream redesign commits/features to avoid or isolate:
  - `73b93d2ed0` / `#11196`: compact printer selection UI part 1; large `Plater.cpp` sidebar rewrite, bed cover assets, printer sync icons.
  - `db64def4c2` / `#11401`: compact printer selection UI part 2; margins, focus fixes, bed-type preview logic, nozzle selector hiding for toolchangers.
  - `7fa6175b4f` / `#11676`: compact printer selection UI part 3; nozzle diameter precision and preset bundle compatibility.
  - `008a98ccda`: sidebar printer preset control alignment.
  - `b6184a540e`: sidebar width reduction.
  - `bbd25a733e`: always show print bed and nozzle diameter selectors.
  - `76a8cc6ba4`: dual-extruder nozzle setting on sidebar.
  - `d31c4427aa`: nozzle diameter selection in UI and preset bundle filtering machinery.
  - `9ee76e4775`: official filament color picker and sidebar swatch/display changes.
  - `10cfab0501` / `#12317` and `14c40d7120` / `#12660`: filament area height preference and layout calculation fixes.
  - `58a8722a69` / `#13959`: unsupported preset filtering in dropdowns; useful behavior but touches preset combo dropdown logic.
  - `e8af78d032` / `#11681`: custom filament preset grouping; useful behavior but touches dropdown grouping UX.
  - `1ec574bcbb` / `#14038`: filament color swatch update fix; likely only relevant if importing upstream swatch/color picker behavior.
- Related web/setup dialogs that are separate from the main view but also part of the selector UX shift:
  - `133dc1c8e5` / `#12248`: web-based printer selection dialog redesign.
  - `71eebc2332` / `#13579`: merged setup guide and standalone selection dialog codebase.
  - `b02a50add0` / `#12167`: filament selection dialog column-browser UI.
- Preservation rule: when importing slicer, preset, profile, nozzle, MMU, or filament features, port backend/config behavior without adopting the new main-view selector layout unless explicitly requested.
- Review trigger: any patch touching `Plater.cpp`, `PresetComboBoxes.*`, `WebGuideDialog.cpp`, `resources/web/guide/21/`, `resources/web/guide/22/`, `resources/web/guide/23/`, or `resources/web/guide/24/` must be checked against `UI1` before applying.
- Acceptable exceptions: small crash/freeze fixes in those files may be imported if they preserve the old visual structure and interactions.

## Top-Level Items

### U1: Security fix for 3MF path traversal

- Release: `v2.3.2`
- PR: `#12860`
- Area: 3MF import / project loading
- Summary: fixes a path traversal vulnerability where a crafted `.3mf` archive could write files outside the intended extraction path during import.
- Why it matters: this is independent of print behavior. If the fork opens shared/untrusted `.3mf` files, this should be treated as mandatory.
- Integration notes: likely a focused import-path fix; verify by inspecting the PR diff before broad cherry-picking unrelated 3MF changes.

### U2: Native Moonraker host type

- Release: `v2.4.0-beta`
- PR: `#13991`
- Area: device host integration
- Summary: adds direct Moonraker REST API host support with API-key auth and storage-root selection, avoiding OctoPrint-emulation assumptions.
- Why it matters: useful if Orca is used as the direct upload/control frontend for the Klipper Voron instead of offline slicing/manual upload.
- Integration notes: not core slicing. Import only if device integration matters; otherwise defer.

### U3: PETG max volumetric-speed profile adjustments

- Release: `v2.3.2-rc`
- PR: `#12329`
- Area: bundled profiles
- Summary: upstream reduced/adjusted PETG max volumetric speed values in bundled profiles for quality.
- Why it matters: mostly reference data for comparing against local measured high-flow limits. Your own measured PETG limits are far beyond generic profiles, so this is not a direct copy candidate.
- Integration notes: do not blindly import profile limits; use as sanity-check/reference material only.

## Quality / Speed Recovery

### Q1: Short-travel smoothing before external perimeters and overhang walls

- Releases: `v2.3.2-beta` cycle
- PRs: `#10722`, `#11975`
- Commits: `36fcf17358`, `a2cff48a46`
- Files: `src/libslic3r/GCode.cpp`
- Summary: changes travel/extrusion transition handling so short travel moves immediately before external perimeters, and later external overhang walls, do not create avoidable speed-transition artifacts at the visible wall start.
- High-speed angle: high acceleration makes tiny decel/accel artifacts and pressure-state discontinuities much more visible as zits, blobs, or vibration marks near seams.
- Why it is interesting: this is one of the closest upstream items to the custom `Optimize for Ooze` goal: improve the visible result without globally reducing print speed.
- Main risk: it touches the same conceptual area as custom seam/ooze travel behavior. Adoption should be validated with generated-G-code comparisons around seam starts and island transitions.
- Suggested test: small multi-island PETG model at normal high-speed settings; compare travel path length, external-wall start speed, retract/wipe events, and actual surface zits.

### Q2: Pressure equalizer endpoint and line-wide slowdown fixes

- Releases: `v2.3.2-beta` / `v2.3.2-rc2`
- PRs: `#11249`, `#12575`
- Commits: `8f5a5a9b16`, `d390524db0`
- Files: `src/libslic3r/GCode/PressureEqualizer.cpp`, `src/libslic3r/GCode/PressureEqualizer.hpp`
- Summary: fixes extrusion-rate smoothing so it does not slow an entire line when only line endpoints need smoothing, and prevents very short moves from being extended past their original endpoints.
- High-speed angle: short moves at high speed/high flow are where pressure compensation bugs turn into obvious endpoint blobs or unexpected slowdowns.
- Why it is interesting: likely quality-positive and speed-positive: less unnecessary slowdown, fewer endpoint artifacts.
- Main risk: depends on how pressure equalizer is enabled/configured in this fork and how it interacts with adaptive PA.
- Suggested test: seam-heavy model with many short external wall segments; inspect generated path endpoints and print for line-end blobs.

### Q3: Arachne max resolution/deviation controls

- Release: `v2.3.2-beta`
- PR: `#11925`
- Commit: `5358191499`
- Files: `src/libslic3r/Arachne/WallToolPaths.cpp`, `src/libslic3r/Arachne/WallToolPaths.hpp`, `src/libslic3r/Arachne/utils/ExtrusionLine.cpp`, `src/libslic3r/PrintConfig.*`, `src/slic3r/GUI/Tab.cpp`
- Summary: exposes Arachne wall maximum resolution/deviation settings and tightens internal simplification thresholds, allowing more detail to be preserved in variable-width wall paths.
- High-speed angle: fast printing already degrades detail through vibration, pressure lag, and flow limits. Preserving more accurate wall geometry can recover detail, but may create more tiny segments.
- Why it is interesting: gives a knob to trade G-code density and motion complexity against geometric fidelity.
- Main risk: too many tiny segments can hurt Klipper lookahead, increase file size, and create motion that cannot physically be followed at 70k accel/high flow.
- Suggested test: compare path segment length histograms and print time estimates for curved/high-detail models before printing.

### Q4: Arachne duplicate thin-wall extrusion fix

- Release: `v2.4.0-beta`
- PR: `#14031`
- Commit: `1bbf8b64f4`
- Files: `src/libslic3r/Arachne/BeadingStrategy/WideningBeadingStrategy.cpp`, `tests/libslic3r/test_arachne_walls.cpp`
- Summary: fixes a bead-count mismatch where one part of Arachne decided a thin wall needed two beads while another generated one bead, producing duplicate/coincident extrusion loops.
- High-speed angle: duplicate extrusion on thin walls becomes a severe bulge when combined with high flow and aggressive motion.
- Why it is interesting: focused correctness fix with a test; likely high value if Arachne is used.
- Main risk: small but central Arachne behavior change; validate thin-wall fixtures.
- Suggested test: thin-wall calibration with outer/inner wall width differences and low `min_bead_width` values.

### Q5: Gap/wall/sparse-infill boundary fixes

- Releases: `v2.3.2` and `v2.4.0-beta` cycle
- PRs: `#12770`, `#12057`, `#12107`, `#12516`, `#11765`
- Commits: `09d5c92f6a`, `2e61d9eab2`, `c302355467`, `0ef8a79cd2`, `2877c6032d`
- Files: `src/libslic3r/Fill/Fill.cpp`, `src/libslic3r/Fill/FillRectilinear.cpp`, `src/libslic3r/Fill/FillBase.cpp`, `src/libslic3r/PerimeterGenerator.cpp`
- Summary: closes gaps between walls and sparse infill, fixes gap-fill width bounds, isolates narrow internal-solid regions, fixes missing infill after contraction splits a region, and improves multiline infill connection to perimeter walls.
- High-speed angle: weak wall/infill contact and tiny missing regions become more obvious when extrusion is near the edge of reliable flow.
- Why it is interesting: quality and strength improvements without simply slowing down the whole print.
- Main risk: this is a cluster, not one patch. Some fixes are small; multiline-related fixes may depend on larger infill changes.
- Suggested test: high-speed functional part with thin ribs, sparse infill touching walls, and narrow internal solid areas; inspect preview and destructive strength if useful.

### Q6: Clipper2 multiline infill

- Release: `v2.3.2-beta`
- PR: `#11435`
- Commit: `506fde8f86`
- Files: `src/libslic3r/Clipper2Utils.cpp`, `src/libslic3r/Clipper2Utils.hpp`, `src/libslic3r/Fill/FillBase.cpp`, `src/libslic3r/Fill/FillRectilinear.cpp`, `src/libslic3r/Fill/FillAdaptive.cpp`, `src/libslic3r/Fill/FillConcentric.cpp`, `src/libslic3r/Fill/FillPlanePath.cpp`
- Summary: moves multiline infill offsetting to Clipper2, uses round joins, supports more fill patterns, raises multiline count limits, and fixes closed-loop gaps.
- High-speed angle: smoother offsets and round joins may reduce abrupt path changes; better multiline infill can improve strength without extra wall count.
- Why it is interesting: pathbuilding-level improvement, not just a setting.
- Main risk: broad infill geometry change; may drag Clipper2 utility changes and alter many patterns.
- Suggested test: compare path continuity, travel count, and abrupt direction changes on multiline infill at high speeds.

### Q7: Per-role flow ratio tuning

- Release: `v2.3.2-beta`
- PR: `#10641`
- Commit: `e6a4ddf32d`
- Files: `src/libslic3r/GCode.cpp`, `src/libslic3r/PrintConfig.*`, `src/slic3r/GUI/Tab.cpp`
- Summary: adds independent flow multipliers for first layer, outer walls, inner walls, overhangs, sparse infill, solid infill, gap fill, supports, and support interfaces.
- High-speed angle: lets high-flow prints keep aggressive volumetric throughput while reducing flow only where quality fails first, such as outer walls, overhangs, gap fill, or top surfaces.
- Why it is interesting: probably one of the most practical quality-recovery knobs for “as fast as possible, still high quality.”
- Main risk: config/UI migration and interaction with existing flow ratio, filament flow, adaptive PA, and calibration workflows.
- UI1 note: this touches `Tab.cpp`/settings UI, not the main-view selector. Do not combine with sidebar selector changes.
- Suggested test: print the same part with global flow unchanged but small reductions for external wall / overhang / gap fill roles.

### Q8: Initial-layer travel acceleration and jerk controls

- Release: `v2.3.2-beta` / later follow-up
- PRs: `#11674`, `#13320`
- Commit: `00704c368f`
- Files: `src/libslic3r/GCode.cpp`, `src/libslic3r/PrintConfig.*`, `src/slic3r/GUI/Tab.cpp`, `src/slic3r/GUI/ConfigManipulation.cpp`
- Summary: separates initial-layer travel acceleration/jerk from normal travel acceleration/jerk.
- High-speed angle: keeps most of the print aggressive while avoiding first-layer drag, adhesion disturbance, or cold-plate scuffing from high-energy travel moves.
- Why it is interesting: targeted slowdown only where high speed is most risky.
- Main risk: Klipper flavor maps jerk to SCV semantics; verify generated `SET_VELOCITY_LIMIT` behavior.
- UI1 note: settings visibility changes are fine; avoid unrelated `Plater.cpp` sidebar changes from nearby upstream commits.
- Suggested test: first-layer travel across long diagonals, small islands, and thin first-layer features.

### Q9: Role-based fan and cooling preservation

- Releases: `v2.3.2` / `v2.4.0-beta` cycle
- PRs: `#13390`, `#13715`, `#12595`
- Commits: `0ee62a9662`, `3370e224b2`, `29addf5ca1`
- Files: `src/libslic3r/GCode.cpp`, `src/libslic3r/GCode/CoolingBuffer.cpp`, `src/libslic3r/GCode/FanMover.cpp`, `src/libslic3r/GCodeWriter.*`, `src/libslic3r/PrintConfig.*`
- Summary: preserves role-based fan speeds across layer changes, adds per-printer non-zero fan PWM floor, fixes fan speed reset after ironing.
- High-speed angle: at high PETG speed, cooling inconsistencies become surface-quality inconsistencies. Fan state bugs can look like random slicer or material artifacts.
- Why it is interesting: improves repeatability without necessarily reducing speed.
- Main risk: cooling behavior is profile/material-sensitive; avoid importing defaults blindly.
- Suggested test: PETG part with bridges, overhangs, external walls, and ironing/top surfaces while logging fan commands.

### Q10: Wall direction and inverse hole direction

- Release: `v2.4.0-beta` cycle
- PRs: `#6193`, `#12669`
- Commits: `e1d6cb1764`, `4d1efad5e9`
- Files: `src/libslic3r/PerimeterGenerator.cpp`, `src/libslic3r/GCode.cpp`, `src/libslic3r/PrintConfig.*`, `src/slic3r/GUI/ConfigManipulation.cpp`
- Summary: removes unreliable automatic wall direction and adds inverse print direction for hole perimeters.
- High-speed angle: deterministic wall direction can improve repeatability; hole direction affects dimensional accuracy and seam/pressure behavior around holes.
- Why it is interesting: useful for engineering parts with holes where dimensional consistency matters at speed.
- Main risk: changes path direction and seam/pressure behavior; may affect established profile expectations.
- Suggested test: hole-dimension test plate and fast perimeter quality around circles/counterbores.

### Q11: Path conflict checker overlap detection

- Release: `v2.4.0-beta` cycle
- Commit: `6923230495`
- Files: `src/libslic3r/Line.cpp`, `src/libslic3r/Line.hpp`, `src/libslic3r/GCode/ConflictChecker.cpp`
- Summary: treats coincident/overlapping line segments as path conflicts, not only true line intersections.
- High-speed angle: undetected coincident paths can cause double extrusion, collisions, or scratches that are more violent at high travel speed.
- Why it is interesting: small geometry correctness improvement with potentially large artifact impact in edge cases.
- Main risk: conflict checker behavior changes may alter path decisions in subtle ways.
- Suggested test: models with thin adjacent features and overlapping/near-overlapping paths.

### Q12: Geometry numeric robustness fixes

- Release: `v2.3.2` / `v2.4.0-beta` cycle
- PR/commits: `#10906`, `52c2a85d28`, `83e9f17aa8`, `90cf80a1f8`
- Files: `src/libslic3r/Polygon.cpp`, `src/libslic3r/Geometry.cpp`, `src/libslic3r/Polyline.cpp`
- Summary: fixes convex/concave point angle threshold precision, fixes `deg2rad(int)` precision loss, and fixes `Polyline3::split_at` point insertion order for ZAA paths.
- High-speed angle: numeric edge cases can become bad path choices, wrong support/seam decisions, or sudden extrusion jumps.
- Why it is interesting: these are foundation-level correctness fixes.
- Main risk: some are tied to ZAA/Point3 architecture and may not backport cleanly without that feature.
- Suggested test: inspect whether each fix is standalone before attempting import.

### Q13: Avoid-crossing-perimeters ignores tiny holes

- Release: `v2.3.2-beta`
- PR: `#10942`
- Commit: `ac75c69ddf`
- Files: `src/libslic3r/GCode/AvoidCrossingPerimeters.cpp`
- Summary: ignores holes with diameter <=2mm when planning avoid-crossing-perimeters detours.
- High-speed angle: prevents tiny holes from causing long detours that waste time and increase ooze/stringing exposure.
- Why it is interesting: only relevant if avoid-crossing is enabled selectively despite being usually banned in this workflow.
- Main risk: avoid-crossing itself can scratch or slow prints in this setup; this is not useful if the feature remains disabled.

### Q14: Avoid extra Z travel moves

- Release: `v2.4.0-beta` cycle
- Commit: `160173b9ca`
- Files: `src/libslic3r/GCode.cpp`
- Summary: removes redundant Z travel/lift/lower moves where they are not needed.
- High-speed angle: fewer Z moves reduce time, stringing windows, and motion discontinuities.
- Why it is interesting: aligns with the local “no unnecessary Z-hop” preference.
- Main risk: make sure it does not alter intentional timelapse/lift-Z safety behavior.

### Q15: Slowdown-for-curled-perimeters default off

- Release: `v2.4.0-beta`
- PR: `#13870`
- Commit: `f593d97f31`
- Files: `src/libslic3r/PrintConfig.cpp`
- Summary: disables `slowdown_for_curled_perimeters` by default and improves tooltip guidance.
- High-speed angle: avoids hidden speed penalties from a quality feature that may not match this printer's tuned cooling/flow behavior.
- Why it is interesting: more about avoiding accidental slowdowns than adding quality.
- Main risk: if curling is the actual limiter, this could reduce quality; treat as a profile/default decision.

### Q16: Z Anti-Aliasing / Z contouring

- Release: `v2.4.0-beta` cycle, originally alpha work
- PR/commits: `#12736`, `0f366ddff1`, `963f8d86b7`, fixes `1e4a5589b5`, `a5e12529f8`, `447d4452e6`, `a1be11126d`, `90cf80a1f8`
- Files: `src/libslic3r/ContourZ.cpp`, `src/libslic3r/Point.hpp`, `src/libslic3r/Line.hpp`, `src/libslic3r/Polyline.cpp`, `src/libslic3r/ExtrusionEntity.*`, `src/libslic3r/GCode.cpp`, `src/libslic3r/PerimeterGenerator.cpp`, `src/libslic3r/PrintObject.cpp`, `src/libslic3r/PrintConfig.*`
- Summary: introduces 3D extrusion geometry and raycasts extrusion points against the source mesh to adjust Z, reducing stair-stepping on curved/sloped surfaces.
- High-speed angle: can recover visible quality on curved surfaces without lowering XY speed, but creates more complex geometry and G-code.
- Why it is interesting: potentially major surface-quality feature for non-benchy real parts with curves and slopes.
- Main risk: huge geometry architecture change. It changes `ExtrusionPath::polyline` to `Polyline3` and propagates through arc fitting, seam/scarf, G-code, supports, and tests.
- Suggested test: treat as a dedicated project, not a cherry-pick. Compare curved surface quality and inspect for small-segment overload.

### Q17: Ironing quality improvements

- Releases: `v2.3.2-beta` / `v2.3.2-rc2`
- PRs: `#11195`, `#10841`, `#11194`, `#12595`
- Files: `src/libslic3r/Fill/Fill.cpp`, `src/libslic3r/Fill/FillBase.*`, `src/libslic3r/PrintConfig.*`, `src/libslic3r/GCode/CoolingBuffer.cpp`, `src/slic3r/GUI/Tab.cpp`
- Summary: adds fixed ironing angle for uniform finish, ironing angle as relative offset from top-surface infill direction, per-filament ironing overrides, and fan reset after ironing.
- High-speed angle: top surfaces often suffer when the rest of the print is optimized for speed; ironing can recover top quality as a targeted post-pass.
- Why it is interesting: quality recovery localized to top surfaces, not global slowdown.
- Main risk: ironing is inherently time-costly; should be used selectively.

### Q18: Elephant-foot compensation for early solid layers

- Releases: `v2.3.2-beta` / later fixes
- PRs: `#11526`, `#13523`, `#11760`
- Files: `src/libslic3r/Fill/Fill.cpp`, `src/libslic3r/PrintObject.cpp`, `src/libslic3r/PrintConfig.*`, brim code
- Summary: applies elephant-foot compensation behavior to early solid layers and optionally generates brim from the compensated outline.
- High-speed angle: fast first layers and cold plates can exaggerate bottom-edge dimensional issues; this targets dimensional quality without slowing the whole part.
- Why it is interesting: engineering-fit improvement.
- Main risk: can affect first-layer strength/adhesion and bottom solid density; test with PETG on CryoGrip.

### Q19: Bridge geometry overhaul

- Releases: `v2.3.2-beta` / `v2.4.0-beta`
- PRs: `#11255`, `#12055`, `#12568`, `#13860`, `#13956`, `#13971`, `#13411`, `#12062`, `#13052`
- Commits: `ae16c76dd2`, `065540e48f`, `6f011c9f6a`, `957d3017b4`, `ac92125012`, `752551292b`, `e54e7a61c0`, `f27605eac1`, `6f1b1b8767`
- Files: `src/libslic3r/Flow.cpp`, `src/libslic3r/LayerRegion.cpp`, `src/libslic3r/PerimeterGenerator.cpp`, `src/libslic3r/Fill/Fill.cpp`, `src/libslic3r/Fill/Fill3DHoneycomb.cpp`, `src/libslic3r/PrintObject.cpp`, `src/libslic3r/Algorithm/RegionExpansion.cpp`, `src/libslic3r/PrintConfig.*`
- Summary: adds bridge line width/density controls, relative bridge direction, model-aligned bridge/ironing angles, external bridge expansion fixes, counterbore bridge fixes, modifier bridge speed handling, top-surface false bridge fixes, outer wall bridge fixes, and sacrificial bridge anchor fixes.
- High-speed angle: bridging is one of the first things to fail when pushing speed/flow. These target geometry, density, direction, and speed control rather than simply lowering everything.
- Why it is interesting: high value for functional parts with counterbores, holes, overhangs, and unsupported spans.
- Main risk: broad cluster; split into small bridge fixes first, then settings/features.

### Q20: Support and support-interface quality overhaul

- Releases: `v2.3.2-rc` / `v2.4.0-beta`
- PRs: `#11812`, `#12141`, `#14040`, `#14095`, `#13454`, `#11761`, `#13310`, `#13524`, `#13936`, `#14069`, `#14074`, `#13460`, `#13327`
- Files: `src/libslic3r/Support/SupportMaterial.*`, `src/libslic3r/Support/TreeSupport.cpp`, `src/libslic3r/Support/TreeSupport3D.cpp`, `src/libslic3r/Support/SupportCommon.cpp`, `src/libslic3r/GCode.cpp`
- Summary: fixes support-interface gap semantics, bottom interfaces, organic support base infill patterns, tree support interfaces/edge filling, organic tree smoothing drift, missing support layers in preview, wrong-Z support extrusion, hybrid tree first-layer behavior, and crashes.
- High-speed angle: supported surfaces are quality-critical; unsupported or drifting supports fail harder at high speeds.
- Why it is interesting: significant quality improvement for complex functional parts.
- Main risk: large support subsystem changes. Use support-heavy regression models.

### Q21: Optimized gyroid infill

- Release: `v2.4.0-beta`
- PR: `#13379`
- Commit: `956fcea7e2`
- Files: `src/libslic3r/Fill/FillGyroid.cpp`, `src/libslic3r/Fill/Fill.cpp`, `src/libslic3r/PrintConfig.*`
- Summary: adds gyroid optimization using auto-tuned wavelength/amplitude and Z-buckling bias, with claimed Z-compression strength gains at low densities.
- High-speed angle: may improve strength/weight without simply increasing infill percentage or walls.
- Why it is interesting: functional quality rather than surface quality.
- Main risk: experimental; verify strength and print-time impact locally.

### Q22: Fuzzy skin fixes and ripple mode

- Releases: `v2.3.2-rc2` / `v2.4.0-beta`
- PRs: `#13471`, `#12479`, `#12920`, `#12632`, `#11923`, `#12611`, `#10312`
- Files: `src/libslic3r/Feature/FuzzySkin/FuzzySkin.cpp`, `src/libslic3r/PrintConfig.*`, `src/slic3r/GUI/Tab.cpp`
- Summary: adds ripple fuzzy mode and fixes painted fuzzy holes/artifacts, classic wall generator regressions, dual seams, crash cases, and option visibility.
- High-speed angle: fuzzy skin is often used to hide artifacts; ripple mode may provide a controlled texture rather than random noise.
- Why it is interesting: only if textured finish is desirable.
- Main risk: less relevant for precision engineering parts unless texture is intentional.

### Q23: Spiral vase cleanup

- Releases: `v2.3.2` / `v2.4.0-beta` cycle
- PRs: `#13517`, `#12564`
- Commits: `16fc3c1b14`, `1388dc5da8`
- Files: `src/libslic3r/GCode/SpiralVase.cpp`, `src/libslic3r/GCodeWriter.cpp`
- Summary: filters zero-flow spiral-vase moves and reduces spiral-Z segment density.
- High-speed angle: avoids useless moves and cleaner G-code in vase mode.
- Why it is interesting: only relevant if vase mode is used.

### Q24: Adaptive infill disconnecting from walls at low density

- Release: `v2.3.2-beta` cycle
- PR: `#11728`
- Commit: `2a0cfdb356`
- Files: `src/libslic3r/Fill/FillAdaptive.cpp`
- Summary: fixes adaptive cubic infill disconnecting from perimeter walls at low densities and disables unsupported rotation for that infill type.
- High-speed angle: low-density infill is common when chasing speed; if it disconnects from walls, top surfaces and part strength suffer disproportionately.
- Why it is interesting: small structural-quality fix for fast, sparse prints.
- Main risk: specific to adaptive infill use.

### Q25: Raft and first-layer speed handling fixes

- Release: `v2.4.0-beta` cycle
- PRs: `#13224`, `#13415`
- Commits: `5b05b6104a`, `699a11f75e`
- Files: `src/libslic3r/GCode.cpp`
- Summary: fixes first-layer speed, slow-down-layer interpolation, and overhang speed handling when rafts are present; follow-up fixes raft-vs-object layer detection.
- High-speed angle: first model layers over rafts can silently run with the wrong speed schedule, hurting adhesion and surface quality.
- Why it is interesting: only relevant if rafts are used, but important when they are.

### Q26: Resonance avoidance speed calculation adjustment

- Release: `v2.3.2-beta` cycle
- PR: `#11462`
- Commit: `f08cae6c5a`
- Files: `src/libslic3r/GCode.cpp`
- Summary: changes resonance-avoidance speed clamping so speeds in the upper half of the avoidance window are pushed to the max instead of unnecessarily slowed.
- High-speed angle: avoids hidden slowdowns when resonance avoidance is enabled, but may print louder/faster in resonance-adjacent ranges.
- Why it is interesting: speed-preservation fix if resonance avoidance is part of local profiles.
- Main risk: profile-specific; verify against measured resonance bands.

### Q27: Small-area flow compensator improvements

- Release: `v2.3.2-beta` cycle
- PR: `#11716`
- Commit: `81dd153798`
- Files: `src/libslic3r/GCode/SmallAreaInfillFlowCompensator.cpp`, `src/libslic3r/GCode/SmallAreaInfillFlowCompensator.hpp`
- Summary: replaces spline dependency with a PCHIP interpolator and enforces better flow-compensation factor behavior for small areas.
- High-speed angle: small isolated features are where volumetric flow, pressure, and cooling are least stable; smoother compensation can reduce spikes/artifacts.
- Why it is interesting: focused quality improvement for small details.
- Main risk: depends on whether small-area compensation is enabled in local profiles.

### Q28: Brim flow ratio

- Release: `v2.3.2-beta` cycle
- PR: `#11876`
- Commit: `c30276ef24`
- Files: `src/libslic3r/GCode.cpp`, `src/libslic3r/PrintObject.cpp`, `src/libslic3r/PrintConfig.*`, `src/slic3r/GUI/Tab.cpp`
- Summary: adds a dedicated `brim_flow_ratio` setting instead of forcing brims to share general first-layer/model flow behavior.
- High-speed angle: PETG brims often need different squish/flow from the model body; this helps first-layer reliability without ruining model dimensions.
- Why it is interesting: targeted first-layer adhesion knob.
- Main risk: config/UI addition; should be validated on CryoGrip/cold-plate workflow.
- UI1 note: settings UI only; keep old main selector unchanged.

### Q29: Combine brims and brim/EFC outline behavior

- Release: `v2.3.2` / `v2.4.0-beta` cycle
- PRs: `#12343`, `#11760`
- Commits: `208ebfc703`, `3dc80593bd`
- Files: `src/libslic3r/Brim.cpp`, `src/libslic3r/GCode.cpp`, `src/libslic3r/PrintConfig.*`
- Summary: can union adjacent object brims into combined brims, and optionally generate brims from the elephant-foot-compensated outline.
- High-speed angle: combined brims reduce travel/stringing around multiple brim loops; EFC-outline brims keep brim behavior consistent with bottom-dimension compensation.
- Why it is interesting: first-layer reliability and reduced brim travel overhead.
- Main risk: `Brim.cpp` refactor is large; import only if brim workflow matters.

### Q30: Support layers missing or emitted at wrong Z

- Release: `v2.4.0-beta` cycle
- PRs: `#13310`, `#13327`, `#13460`
- Commits: `0c7e16aa36`, `95ce474c8d`, `a3f229f406`
- Files: `src/libslic3r/Support/SupportMaterial.cpp`, `src/libslic3r/Support/TreeSupport.cpp`, `src/libslic3r/Support/TreeSupportCommon.hpp`, `src/libslic3r/GCode.cpp`
- Summary: fixes floating-point gap subdivision causing missing support layers, and fixes support extrusion being placed at the wrong Z height in back-to-back support-layer edge cases.
- High-speed angle: support Z mistakes cause collisions or unsupported spans; high-speed PETG has less tolerance for either.
- Why it is interesting: print-failure prevention for support-heavy parts.
- Main risk: support subsystem/G-code interaction; validate support-heavy models.

### Q31: Invalid fan speed and stuck fan-state fixes

- Release: `v2.4.0-beta` cycle
- PR/commits: `#13336`, `7de3031121`, `6feb99bd2d`
- Files: `src/libslic3r/GCode/FanMover.cpp`, `src/libslic3r/GCode/CoolingBuffer.cpp`
- Summary: fixes fan state getting stuck at the wrong speed after role-driven fan changes and fixes generation of invalid negative feed/speed values such as `G1 F-2147483648` from the cooling buffer.
- High-speed angle: PETG fan mistakes look like random surface/adhesion failures; invalid G-code can hard-fail Klipper.
- Why it is interesting: correctness and safety for cooling state machine.
- Main risk: should be reviewed with `Q9` because these touch the same fan/cooling area.

## Seam / Travel / Ooze

### S1: Scarf seam fixes

- Release: `v2.4.0-beta` cycle
- Commits: `9fae402d62`, `2957508b4a`
- Files: `src/libslic3r/GCode/SeamPlacer.cpp`, `src/libslic3r/GCode.cpp`, `src/libslic3r/PerimeterGenerator.cpp`, `src/libslic3r/ExtrusionEntity.cpp`, `src/libslic3r/Polyline.cpp`, geometry utilities
- Summary: fixes scarf seam generation, start/end overlap and positioning, arc fitting data through split/merge/reverse, and a bug where scarf seam depended incorrectly on ZAA state.
- Why it matters: scarf seam is one of the few ways to hide seam artifacts without just slowing external walls, but it can also interact badly with high-speed ooze/pressure behavior.
- Main risk: broad geometry/seam touch area and likely conflict with custom seam behavior.
- Suggested test: cylinders, sharp-corner boxes, and multi-island parts with scarf seam on/off and `Optimize for Ooze` on/off.

### S2: Aligned/back seam positioning for mirrored objects

- Release: `v2.3.2-beta`
- PR: `#12028`
- Commit: `ecc2c91e32`
- Files: `src/libslic3r/GCode/SeamPlacer.cpp`
- Summary: keeps normals outward during mirrored-object seam visibility transforms so aligned/back seams stay on the actual back side.
- Why it matters: focused seam correctness fix for mirrored models.
- Main risk: low; likely standalone.

### S3: Seam overhang distance setting

- Release: `v2.4.0-beta` cycle
- Commit: `8bed074a15`
- Files: `src/libslic3r/GCode/SeamPlacer.cpp`, `src/libslic3r/PrintConfig.*`, `src/slic3r/GUI/Tab.cpp`
- Summary: adds a configurable distance to keep seams away from overhang regions.
- Why it matters: overhang seams are visually and mechanically fragile at speed.
- Main risk: config/UI addition; may interact with custom seam priorities.

### S4: Wipe inside before external perimeter

- Release: `v2.3.2` cycle
- PR: `#3287`
- Commit: `6060777dd0`
- Files: `src/libslic3r/GCode.cpp`, `src/libslic3r/PrintConfig.*`, `src/libslic3r/Preset.cpp`, `src/slic3r/GUI/Tab.cpp`
- Summary: ports SuperSlicer-style wiping inside the model before starting an external perimeter to reduce seam blobs.
- Why it matters: directly targets visible external-perimeter start artifacts.
- Main risk: may add path length/time and can conflict with a travel-minimizing ooze strategy.

### S5: Wipe plus firmware retraction fix

- Release: `v2.4.0-beta`
- PR: `#13812`
- Area: retraction/wipe behavior
- Summary: forces retract-before-wipe to 100% when firmware retraction is enabled so wipe behavior remains valid.
- Why it matters: relevant only if firmware retraction is enabled.
- Main risk: low relevance if firmware retraction is not part of the profile.

## Klipper / Motion / Calibration

### K1: Klipper/JD time estimation and estimator branch overlap

- Releases: `v2.3.2-rc`, `v2.3.2-rc2`, later `v2.4.0-beta` commits
- PRs: `#12417`, `#12440`
- Files: `src/libslic3r/GCode/GCodeProcessor.cpp`, `src/libslic3r/GCode/GCodeProcessor.hpp`
- Summary: upstream added JD-aware estimation and follow-up JD processor fixes. The current fork branch has much more extensive Klipper two-pass estimator work in the same files.
- Why it matters: accurate estimates are important for tuning and validating high-speed prints, but this is a conflict hotspot.
- Main risk: very high merge risk; compare algorithms before importing.
- Suggested approach: diff current branch against upstream `v2.4.0-beta` `GCodeProcessor.*` and decide whether to port individual bug fixes or keep the fork's estimator as the authoritative version.

### K2: JD/jerk settings behavior fixes

- Release: `v2.4.0-beta`
- PRs: `#13575`, `#14004`
- Commits: `ff68a2859d`, `bd41eebba9`
- Files: `src/libslic3r/Print.cpp`, `src/slic3r/GUI/Tab.cpp`, `src/slic3r/GUI/ConfigManipulation.cpp`
- Summary: hides jerk settings when JD is active, shows JD settings when relevant, suppresses invalid jerk warnings under JD, and fixes mismatch warnings.
- Why it matters: avoids profile/UI confusion that could lead to wrong motion settings or unnecessary tuning changes.
- Main risk: must preserve Klipper SCV/JD semantics expected by local profiles.

### K3: Input shaping / cornering / PA calibration improvements

- Release: `v2.3.2-beta` cycle
- PRs: `#10913`, `#10962`, `#11178`, `#11835`, `#11389`, `#11988`, `#11493`
- Area: calibration generators and UI
- Summary: firmware-aware input shaping calibration, cornering/jerk calibration for Klipper SCV / Marlin JD / classic jerk / RRF, PA preserved during IS/cornering tests, configured layer height used, calibration order improved, reverse-on-even disabled for consistency, PA values validated.
- Why it matters: fits the measurement-driven Voron workflow.
- Main risk: calibration generators are less risky than core slicing but still need generated-G-code review.

### K4: VFA / flow calibration fixes

- Release: `v2.3.2` / `v2.4.0-beta`
- PRs: `#12290`, `#14079`, `#11956`
- Summary: repairs VFA tower on Linux, fixes VFA interaction with input shaping, improves flow-rate pattern calibration menu.
- Why it matters: high-speed tuning depends on reliable calibration artifacts.
- Main risk: mostly low; verify generated calibration G-code.

### K5: Adaptive bed mesh / exclude-object support

- Release: `v2.3.2` / `v2.4.0-beta` cycle
- PRs: `#11491`, `#13290`, `#12869`
- Commits: `b30d7fb0fa`, `d37444b34a`, `fa744e39b9`
- Area: Klipper start G-code metadata / printer defaults
- Summary: fixes adaptive bed mesh probe count, adds print-area bounding-box comments (`MINX`, `MINY`, `MAXX`, `MAXY`) used by Klipper-style adaptive mesh flows, and enables exclude objects by default for dynamic mesh workflows.
- Why it matters: on a large Voron bed, wrong or missing adaptive mesh metadata can silently degrade first-layer quality. Exclude objects also matters for long multi-object prints.
- Main risk: verify exact comment/macro convention used by local Klipper configs before importing.

### K6: Extrusion-role change G-code

- Release: `v2.4.0-beta` cycle
- PR: `#11784`
- Commit: `dee9c0d72c`
- Files: `src/libslic3r/GCode.cpp`, `src/libslic3r/PrintConfig.*`, `src/libslic3r/Preset.cpp`, `src/slic3r/GUI/Tab.cpp`
- Summary: adds process/filament G-code hooks emitted when extrusion role changes, such as wall to infill or support to model.
- Why it matters: enables per-role Klipper tuning such as `SET_PRESSURE_ADVANCE`, velocity limits, cooling commands, or logging without hand-editing layer-change G-code.
- Main risk: can bloat G-code and cause firmware command overhead if used too frequently; should be opt-in and carefully templated.
- UI1 note: touches `Tab.cpp`/preset config; do not import selector/sidebar rewrites bundled nearby.

## Preview / Diagnostics

### P1: G-code preview overhaul / libvgcode

- Release: `v2.3.2-beta`
- PR: `#10735`
- Files: `src/libvgcode/`, `src/slic3r/GUI/GCodeViewer.cpp`, `src/slic3r/GUI/GCodeViewer.hpp`, `src/slic3r/GUI/GLCanvas3D.*`
- Summary: ports PrusaSlicer 2.8.0 `libvgcode`, improving preview performance, arc rendering, variable-flow rendering, and actual speed / actual volumetric-flow visualization.
- Why it matters: essential diagnostics for high-flow/high-accel generated G-code.
- Main risk: very large UI/viewer stack change.

### P2: Pressure Advance visualization

- Release: `v2.3.2-rc`
- PR: `#11673`
- Files: `src/libslic3r/GCode/GCodeProcessor.*`, `src/libvgcode/*`, `src/slic3r/GUI/GCodeViewer.cpp`, `src/slic3r/GUI/LibVGCode/LibVGCodeWrapper.cpp`
- Summary: parses and visualizes PA values from Klipper `SET_PRESSURE_ADVANCE`, Marlin `M900`, and RRF `M572`.
- Why it matters: directly supports adaptive PA tuning and defect correlation.
- Main risk: depends on viewer stack; may be hard to import without P1.

### P3: Acceleration / jerk visualization

- Release: `v2.4.0-beta` cycle
- PR: `#13169`
- Commit: `f8b0bcc725`
- Files: `src/libslic3r/GCode/GCodeProcessor.*`, `src/libvgcode/*`, `src/slic3r/GUI/GCodeViewer.cpp`
- Summary: adds kinematics preview modes for acceleration and jerk/effective jerk.
- Why it matters: lets you see where slicer-generated `SET_VELOCITY_LIMIT` and feature settings affect motion.
- Main risk: likely depends on P1/libvgcode.

### P4: Line type preview distances/amounts and hidden marker fixes

- Releases: `v2.3.2` / `v2.4.0-beta`
- PRs: `#13681`, `#12364`
- Summary: displays distance/amount values in line-type preview and fixes z-fighting that hid wipe/seam/retract markers.
- Why it matters: useful for verifying seam, wipe, retract, and move quantities.
- Main risk: preview-only.

### P5: Speed/flow preview correctness and vertex diagnostics

- Release: `v2.3.2` / `v2.4.0-beta` cycle
- PR/commits: `#11848`, `#12840`, `4af168bac7`, `e0cdae0cb4`, `bec883e667`
- Files: `src/libslic3r/GCode/GCodeProcessor.cpp`, `src/slic3r/GUI/GCodeViewer.cpp`
- Summary: fixes misleading interpolation in speed/flow preview, adds per-vertex speed and volumetric-flow diagnostics, and fixes actual-speed visualization discontinuities.
- Why it matters: high-speed tuning depends on trusting preview diagnostics; incorrect gradients or per-segment data can make you chase nonexistent slicer problems.
- Main risk: mostly viewer/processor-side; may depend on `P1` viewer overhaul.

### P6: Time-estimation precision and block ordering fixes

- Release: `v2.4.0-beta` cycle
- Commits: `acd3c94955`, `bdbeb34158`
- Files: `src/libslic3r/GCode/GCodeProcessor.cpp`
- Summary: fixes time-machine block traversal order to better match firmware behavior and accumulates total estimated print time in `double` instead of `float`.
- Why it matters: long high-speed Voron prints need accurate estimates to compare tuning changes and diagnose slowdowns.
- Main risk: overlaps with `K1` current custom Klipper estimator work.

## Custom G-code / Placeholders / Macros

### H1: File header G-code and print-time/filament placeholders

- Release: `v2.3.2` cycle
- PR: `#12186`
- Commit: `2f2f0df6af`
- Files: `src/libslic3r/GCode.cpp`, `src/libslic3r/GCode/GCodeProcessor.*`, `src/libslic3r/PrintConfig.*`, `src/slic3r/GUI/Tab.cpp`
- Summary: adds machine file-header G-code before the header block and new placeholders such as `{print_time_sec}` and `{filament_length_m}`.
- Why it matters: useful for Klipper screen/macros and metadata-aware start scripts.
- Main risk: affects header ordering and placeholder processing; inspect generated G-code headers.

### H2: Built-in placeholder fixes

- Release: `v2.4.0-beta`
- PRs: `#13892`, `#13895`, `#11485`
- Commits: `67b9f07655`, `4000445345`, `0aac0478e7`
- Files: `src/libslic3r/GCode.cpp`, placeholder parser/test files
- Summary: fixes missing `{version}`, `{timestamp}`, `{user}`, `{initial_extruder}`, `{extruded_weight_total}`, `{extruded_volume_total}` behavior, fixes `{total_toolchanges}` returning 0 without a wipe tower, and fixes placeholder parser bugs.
- Why it matters: Klipper `START_PRINT`/filename/macros can silently get empty or wrong values without these fixes.
- Main risk: generated custom G-code may change; validate local macros.

### H3: More Klipper-useful placeholders

- Release: `v2.4.0-beta` cycle
- Commits: `99cb888421`, `b636bbaf2f`, `3f00fd2993`
- Summary: adds placeholders such as `hold_chamber_temp_for_flat_print`, `{max_print_z}`, and `{min_vitrification_temp}`.
- Why it matters: useful for Voron/Klipper chamber behavior, max-Z-aware macros, and material-specific chamber/exhaust logic.
- Main risk: optional; only useful if local macros consume them.

### H4: Power-loss recovery G-code control

- Release: `v2.3.2` / `v2.4.0-beta` cycle
- PRs: `#11582`, `#11616`
- Commits: `5c547ea4a1`, `7c91459c37`
- Files: `src/libslic3r/GCode.cpp`, `src/libslic3r/GCodeWriter.cpp`, `src/libslic3r/PrintConfig.cpp`
- Summary: changes power-loss recovery G-code emission from implicit/buggy behavior to explicit config/enum behavior.
- Why it matters: less relevant to Klipper if handled in firmware, but important to avoid incorrect assumptions about emitted recovery commands.
- Main risk: default behavior changes for some printer families.

## Lower Priority / Probably Skip For Now

### L1: Most new vendor printer/profile bundles

- Summary: massive profile churn, including Creality K-series, Bambu, Elegoo, Qidi, Snapmaker, SeeMeCNC, and many filament bundles.
- Why lower priority: local printer/profile tuning is highly custom and generic profiles are unlikely to be imported directly.
- Keep as reference: profiles may still be useful for comparing material assumptions or machine-limit conventions.

### L2: Orca Cloud / profile-sync related work

- Summary: cloud sync and account/profile infrastructure from the 2.4 cycle.
- Why lower priority: not aligned with the local high-speed slicing/quality focus.

### L3: MMU / wipe tower feature clusters

- Summary: wipe tower interface stability, Happy Hare sync, MMU detection, type-2 tower settings, toolchanger travel-to-tower behavior.
- Why lower priority: high value only if multi-material, PETG/PLA support interfaces, or wipe towers become part of the workflow.
- Exception: individual wipe/retraction fixes may matter if firmware retraction or tool changes are used.

### L4: Cosmetic UI changes

- Summary: accordion tabs, dynamic titlebar, realistic view, dialog layout changes.
- Why lower priority: useful but not central to print speed/quality.

### L4a: Compact/new printer-nozzle-filament selector UI

- See: `UI1`
- Summary: upstream compact selector/sidebar redesign, filament picker/dialog redesigns, nozzle selector/sidebar changes, swatch/color picker additions, preset dropdown grouping/filtering UI.
- Why lower priority / avoid: user explicitly dislikes this main-view selector UI. Do not import it as part of other features.
- If backend behavior is needed: isolate backend changes from `Preset.cpp`, `PresetBundle.cpp`, `PrintConfig.*`, or validation code and keep the old `Plater.cpp`/`PresetComboBoxes.*` presentation.

### L5: Large dependency upgrades

- PR: `#11311`
- Commit: `dc5897d7b5`
- Summary: updates Eigen and libigl substantially.
- Why lower priority: potentially broad numerical and geometry benefits, but too invasive to treat as a normal backport.
- Keep as reference: if a future rebase imports upstream wholesale, this may improve geometry robustness indirectly.

## Dependency-Aware Suggested Order

This order is based on likely dependency chains, commit sequencing, and conflict risk. The goal is to import small correctness fixes first, then knobs/settings, then large architecture changes only when needed.

### Phase 0: Baseline And Safety

1. `U1`: 3MF security fix
   - Reason: independent security fix; should not wait for quality work.
   - Dependency risk: low.

2. Establish local comparison fixtures before quality imports
   - Suggested fixtures: seam/ooze multi-island part, thin-wall Arachne part, holes/counterbores part, bridge/overhang part, top-surface/ironing part, support-heavy part.
   - Why first: many items below change generated paths subtly; without fixtures it will be hard to tell if a backport helps or harms.

### Phase 1: Small Standalone Correctness Fixes

These are the best first candidates because they are small, focused, and mostly do not require large feature infrastructure.

1. `S2`: aligned/back seam positioning for mirrored objects
   - Commit: `ecc2c91e32`
   - Reason: focused seam correctness in `SeamPlacer.cpp`.

2. `Q4`: Arachne duplicate thin-wall extrusion fix
   - Commit: `1bbf8b64f4`
   - Reason: focused bugfix with upstream test; prevents duplicate extrusion.

3. `Q2`: pressure equalizer endpoint fix first, then line-wide slowdown fix
   - Suggested order: `d390524db0` (`#12575`) before/with `8f5a5a9b16` (`#11249`) depending on local file state.
   - Reason: both touch `PressureEqualizer.*`; import as a mini-cluster to avoid reconciling the same code twice.

4. `Q12`: standalone numeric robustness fixes where not tied to ZAA
   - Suggested order: inspect/import `83e9f17aa8` (`deg2rad(int)` precision) and `52c2a85d28` polygon threshold fix if they apply cleanly.
   - Defer `90cf80a1f8` unless `Q16` ZAA is imported, because it fixes `Polyline3` behavior.

5. `Q14`: avoid extra Z travel moves
   - Commit: `160173b9ca`
   - Reason: likely small and aligned with local no-unnecessary-Z-hop preference.
   - Caveat: verify it does not affect intentional lift-Z/timelapse behavior.

### Phase 2: Motion/Cooling State Fixes Before New Motion Knobs

These preserve expected behavior and should come before adding more settings that depend on that behavior.

1. `Q9`: role-based fan-state fixes
   - Suggested order: `0ee62a9662` role-based fan speeds lost on layer change, `7de3031121` stuck fan speed, `6feb99bd2d` invalid cooling-buffer speed, then `29addf5ca1` fan reset after ironing, then `3370e224b2` fan PWM floor if the new setting is desired.
   - Reason: state-preservation fixes are lower risk than adding a new fan-floor feature.

2. `K2`: JD/jerk settings behavior fixes
   - Suggested order: `ff68a2859d` warning suppression, then `bd41eebba9` settings visibility/behavior.
   - Reason: avoid UI/profile confusion before importing/adjusting estimator or calibration work.

3. `K1`: compare Klipper estimator implementations before importing any upstream estimator commits
   - Reason: current branch has custom two-pass Klipper estimator work; blindly importing upstream `GCodeProcessor.*` changes is high risk.
   - Output of this step should decide whether `K1` is skipped, partially ported, or used as reference only.

### Phase 3: High-Speed Quality Knobs With Limited Geometry Risk

These add tunable recovery mechanisms and should be easier to reason about than architecture-scale geometry changes.

1. `Q7`: per-role flow ratio tuning
   - Commit: `e6a4ddf32d`
   - Reason: high practical value for high-speed quality recovery.
   - Dependency risk: config/UI/G-code plumbing; no major geometry architecture dependency.

2. `Q8`: initial-layer travel acceleration and jerk
   - Suggested order: `00704c368f` then follow-up `#13320` conditional UI behavior.
   - Reason: feature plus UI dependency; import together if possible.

3. `Q28`: brim flow ratio
   - Reason: targeted first-layer adhesion knob with limited geometry risk.

4. `Q15`: slowdown-for-curled-perimeters default off
   - Commit: `f593d97f31`
   - Reason: profile/default decision; can be handled independently.

5. `Q3`: Arachne max resolution/deviation controls
   - Commit: `5358191499`
   - Reason: useful knob, but changes Arachne simplification behavior; should follow `Q4` so thin-wall duplicate correctness is already fixed.

6. `K6`: extrusion-role change G-code
   - Reason: powerful for per-role Klipper tuning, but should come after placeholder/custom G-code sanity checks if macros will consume slicer variables.

### Phase 4: Gap / Infill / Wall Contact Mini-Clusters

These are pathbuilding fixes where dependencies can vary by local file state. Prefer importing in small clusters by file/feature.

1. `Q5a`: small gap/wall fixes
   - Suggested commits: `2e61d9eab2` gap-fill bounds, `09d5c92f6a` gaps between walls and sparse infill, `e5ca01ba9e` only-one-wall override if applicable, `2a0cfdb356` adaptive infill low-density wall connection if adaptive infill is used.
   - Reason: smaller and likely less dependent than multiline infill.

2. `Q5b`: missing/narrow infill fixes
   - Suggested commits: `c302355467` narrow internal solid regions, `0ef8a79cd2` missing infill layers.
   - Reason: both touch fill behavior and should be validated together with top/internal solid surfaces.

3. `Q27`: small-area flow compensator improvements
   - Reason: focused small-feature flow quality; likely separate from Fill path geometry.

4. `Q6`: Clipper2 multiline infill
   - Commit: `506fde8f86`
   - Reason: larger dependency boundary. Import after the smaller Fill fixes, or treat as a replacement cluster if it supersedes them.
   - Caveat: because it adds/uses `Clipper2Utils`, this may be more work than the smaller Q5 fixes.

5. `Q29`: combine brims / EFC outline brims
   - Reason: larger `Brim.cpp` behavior/refactor; import after `Q28` and `Q18` decisions.

### Phase 5: Bridge Work In Dependency Order

Bridge changes touch geometry, flow, config, and path planning. Split them instead of importing `Q19` as one block.

1. `Q19a`: bridge correctness fixes without new user settings
   - Suggested commits: `6f011c9f6a` external bridge fix, `e54e7a61c0` outer wall for bridges, `6f1b1b8767` sacrificial bridge anchor fix, `f27605eac1` 3D honeycomb bridge direction fix.
   - Reason: correctness before new controls.

1.5. `Q30`: support Z-height / missing-layer fixes if supports are being touched for bridge-heavy parts
   - Suggested commits: `0c7e16aa36`, `95ce474c8d`, `a3f229f406`.
   - Reason: support Z correctness can be as important as bridge correctness for high-speed overhangs.

2. `Q19b`: counterbore bridge fix
   - Commit: `ac92125012`
   - Reason: large rewrite in `PerimeterGenerator.cpp`; should be isolated and tested on counterbore/hole fixtures.

3. `Q19c`: bridge modifier/top-surface fixes
   - Suggested commits: `752551292b` bridge modifier speed, `957d3017b4` false bridges on top surfaces / extra bridge layers.
   - Reason: these depend on how current modifier and extra-bridge-layer code looks.

4. `Q19d`: bridge line width/density and relative/model-aligned bridge direction
   - Suggested order: `ae16c76dd2` bridge line width/density, then `065540e48f` relative bridge direction / align bridge and ironing angles.
   - Reason: settings/features after correctness fixes; second commit spans bridge and ironing orientation behavior.

### Phase 6: Seam / Travel / Ooze Behavior Changes

These are valuable but most likely to conflict with the custom `Optimize for Ooze` work. Handle only after baseline fixtures exist.

1. `Q1`: short-travel smoothing
   - Suggested order: `36fcf17358` first, then `a2cff48a46` overhang-wall extension.
   - Reason: second extends the first; import in upstream order.

2. `S3`: seam overhang distance setting
   - Reason: new seam placement knob; evaluate after seeing `Q1` effects.

3. `S4`: wipe inside before external perimeter
   - Reason: directly targets seam blobs but may add travel/path time; evaluate against local ooze strategy.

4. `S1`: scarf seam fixes
   - Suggested order: only after deciding whether to import `Q16` ZAA. If not importing ZAA, inspect `2957508b4a` carefully because it specifically decouples scarf seam from ZAA state.
   - Reason: `9fae402d62` touches geometry/Polyline3/scarf internals and may depend on ZAA-era geometry changes.

### Phase 7: Calibration And Diagnostics

These are useful for tuning but generally do not need to block slicing bugfixes.

1. `K3`: input shaping / cornering / PA calibration improvements
   - Suggested order: firmware detection (`#10913`), cornering/jerk calibration (`#10962`), PA-preservation (`#11178`), layer-height/order/value validation fixes.
   - Reason: generated calibration G-code should be consistent before relying on results.

2. `K4`: VFA / flow calibration fixes
   - Reason: useful after `Q7` if per-role flow ratio tuning becomes part of workflow.

3. `K5`: adaptive bed mesh / exclude-object support
   - Reason: useful for large Voron first-layer consistency; verify local macro conventions before import.

4. `H1`/`H2`: custom G-code placeholder fixes
   - Reason: custom Klipper macros should receive correct values before adding more macro-driven features.

4.5. Apply `UI1` filter to any profile/preset/nozzle/filament work
   - Reason: many upstream selector-related commits mix backend compatibility with main-view UI redesign. Split them deliberately.

5. `H3`: additional Klipper-useful placeholders
   - Reason: optional, after core placeholder correctness.

6. `H4`: power-loss recovery control
   - Reason: mostly explicitness/safety; import only if relevant to local firmware workflow.

7. `P1`: libvgcode preview overhaul
   - Reason: likely prerequisite for `P2` and `P3`.

8. `P2`: PA visualization
   - Reason: likely depends on viewer stack from `P1`.

9. `P3`: acceleration / jerk visualization
   - Reason: likely depends on viewer stack from `P1`; useful after `K1`/`K2` decisions.

10. `P5`: speed/flow preview correctness and vertex diagnostics
   - Reason: diagnostics accuracy for high-speed tuning; may depend on `P1`.

11. `P6`: time-estimation precision and block-ordering fixes
   - Reason: overlaps with `K1`; decide after estimator comparison.

12. `P4`: line type distances/amounts and hidden marker fixes
   - Reason: preview-only; can be imported opportunistically if viewer stack is already being touched.

### Phase 8: Large Architecture Features

Only do these as dedicated projects. They have large dependency surfaces and can invalidate assumptions in smaller backports.

1. `Q16`: ZAA / Z contouring
   - Dependency order inside the cluster: base ZAA commit (`963f8d86b7` or updated `0f366ddff1`), then ZAA fixes (`95736445a4`, `1d4c7c56a2`, `447d4452e6`, `a1be11126d`, `1e4a5589b5`, `90cf80a1f8`, `4f162b9058`).
   - Reason: introduces `Point3`/`Polyline3` through the extrusion pipeline. Do not mix with small seam/scarf imports unless intentionally adopting the architecture.

2. `Q20`: support and support-interface overhaul
   - Suggested order: support interface semantics (`#11812`), organic support infill patterns (`#12141`), then 2.4 fixes (`#13310`, `#13454`, `#14040`, `#14069`, `#14095`, `#14074`).
   - Reason: later support fixes likely assume earlier support semantics.

3. `Q21`: optimized gyroid
   - Reason: self-contained but experimental; better after core infill stability work.

4. `Q22`: fuzzy skin fixes/ripple mode
   - Suggested order: bug fixes first (`#11923`, `#12632`, `#12920`, `#12479`), then ripple mode (`#13471`).
   - Reason: do not import new texture mode before the existing path/artifact fixes.

5. `Q17`: ironing feature cluster
   - Suggested order: angle semantics (`#10841`), fixed angle (`#11195`), per-filament overrides (`#11194`), UI hiding fixes (`#11451`, `#11286`), fan reset (`#12595`) if not already imported via `Q9`.
   - Reason: settings build on each other; fan reset is a correctness fix.

6. `Q18`: elephant-foot compensation cluster
   - Suggested order: `#11526`, then `#13523`, then optional brim-from-EFC outline `#11760`.
   - Reason: import feature before its calculation fix; brim behavior is optional.

7. `Q23`: spiral vase cleanup
   - Suggested order: `#13517` zero-flow fix before `#12564` segment-density reduction.
   - Reason: correctness before cleanup.

### Practical First Batch Recommendation

If starting implementation soon, a pragmatic first batch would be:

1. `U1`
2. `S2`
3. `Q4`
4. `Q2`
5. `Q9`/`Q31` fan-state fixes that do not add new settings
6. `Q5a` small gap/wall fixes
7. `Q28` brim flow ratio if first-layer/brim tuning matters
8. `H2` placeholder fixes if local Klipper macros use slicer placeholders
9. `K2` warnings/settings only, after checking local Klipper config expectations

This batch avoids the biggest architecture changes while targeting artifacts that high speed is likely to amplify.

## Notes

- The upstream range from `v2.3.1` to `v2.4.0-beta` is very large: thousands of commits, heavy churn in `src/libslic3r/GCode.cpp`, `src/libslic3r/GCode/GCodeProcessor.cpp`, `src/libslic3r/PerimeterGenerator.cpp`, `src/libslic3r/PrintConfig.*`, geometry primitives, support generation, and the viewer stack.
- Avoid broad cherry-picks unless the dependency chain is understood.
- For seam/travel/pathbuilding changes, generated G-code comparison matters more than clean compile alone.
- For high-speed quality recovery, prefer changes that are local, measurable, and configurable before importing architecture-scale features.
