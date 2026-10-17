# Implementation phases and PR checklist

Work in this order. Each phase should leave the app **fully usable** without the next phase.

## Phase 0 — read only (done)

- This `doc/3d_reference_layer/` folder.

## Phase 1 — Load a glTF with a skeleton (no posing UI)

**Create** tinygltf + `ref3dmodel.*`  
**Edit** `txshleveltypes.h`, `tfiletype.h`, `tiio.cpp` (declare only), `toonzscene.cpp` (`getLevelType` / `loadLevel`), `txshsimplelevel.cpp` (extension → type, `load()` early-out), CMake include path.

**Done when:**

- File → Load Level → `character.glb` creates a level of type `REF3D_XSHLEVEL` in the cast.
- Dropping it on the xsheet uses a **normal** `TXshLevelColumn`.
- Preview eye **off**, camera-stand **on**.
- Old `.tnz` files open unchanged.
- Raster / vector new-level still identical.

**Not in this PR:** drawing, posing, menus beyond load.

## Phase 2 — Viewer display (flat / wire)

**Create** `ref3ddrawer.*`  
**Edit** `stagevisitor.cpp` `OpenGlPainter::onImage` only (plus RasterPainter skip).

Modes: flat shaded, wireframe. Outline can be `glPolygonMode` + bias, not a post process.

**Done when:** mannequin appears in the viewer, follows column placement / camera like a drawing, **does not** appear in Preview / Render.

## Phase 3 — FK pose panel + per-frame save

**Create** `ref3dpose.*`, pose pane, `ref3dposetool.cpp`  
**Edit** tool command ids, CMake, mainwindow (register pane).

Pose stored in sidecar `*.t3dpose`. Changing xsheet frame loads that frame’s pose.

**Done when:** rotate a joint, change frame, come back, pose is still there; save scene, reopen, pose remains; old Tahoma2D still opens the `.tnz` (poses ignored).

## Phase 4 — Productization (still small)

- `MI_NewRef3dLevel` + Level Create combo
- Preference toggle
- CC0 default mannequin
- File dialog filters
- Xsheet tint/icon
- Copy extra files on `.gltf` import

## Phase 5 — later (do not start now)

- Camera/light controls **for the model only** (not SceneViewer 3D mode)
- Pose presets / copy-paste
- Onion skin (ghost previous pose with alpha — still skip RasterPainter)
- IK, hand helpers
- 3D → line art

---

## Compatibility tests (every PR)

1. Open a scene **without** 3D levels (any existing production file).
2. Draw on raster + vector, undo, save, reload.
3. Render a frame (preview and output). Must match pre-change.
4. After Phase 1+: new scene with a 3D column, render — 3D **absent** from output.
5. After Phase 3: round-trip poses.

---

## If something looks like it needs a rewrite

Stop. Put the code in `ref3d*.cpp`. The original files (`stage.cpp`, `txshsimplelevel.cpp`) already have `MESH_XSHLEVEL` cases — copy that pattern, nothing more.
