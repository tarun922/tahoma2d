# 3D Pose Reference Layer — Analysis (do not start coding from this file alone)

This folder is **analysis only**. It exists so the 3D pose-reference feature can be added later in **small PRs** without rewriting Tahoma2D’s drawing pipeline.

**No production `.cpp` / `.h` was changed for this write-up.** Follow `FILEMAP.md` before touching code.

---

## What this feature is

A **reference-only 3D level**: the artist loads a glTF/GLB (plus sidecar files for `.gltf`), poses a skeleton in the viewer, and draws on raster/vector levels on top.

| Requirement | How Tahoma2D already supports it |
|---|---|
| Visible while working | Column **camera-stand** eye (`isCamstandVisible`) |
| Excluded from final render | Column **preview** eye (`isPreviewVisible`) — `StageBuilder` **already skips** preview-hidden columns when rendering |
| Pose per xsheet frame | `TXshCell` = `(level, TFrameId)` — store poses keyed by `TFrameId` |
| Old scenes still open | Infer new type from `.glb` / `.gltf` extension; **do not add unknown XML tags** inside `TXshSimpleLevel` (old builds **throw** on unexpected tags) |

This is **not** Clip Studio’s full 3D suite. MVP is: load model, show it cheaply, rotate joints (FK), remember poses.

---

## What already exists (do not confuse these)

| Name | What it actually is |
|---|---|
| **SceneViewer 3D mode** (`sceneviewer.cpp`, `m_theta3D` / `m_phi3D`, `pane_3d.svg`) | Camera/table **perspective for 2D papers**. Not a 3D model loader. **Do not hijack this.** |
| **`MESH_XSHLEVEL` + `TXshMeshColumn`** | 2D **plastic deformation mesh**. Reusing it would mix two features and break Plastic Tool. |
| **`Skeleton` / `T_Skeleton` / `IKSkeleton`** | 2D column/pegbar skeleton. Wrong data model for glTF joints. |
| **`TMeshImage` / `tiio_mesh`** | `.mesh` 2D images. Not glTF. |

Tahoma2D does **not** currently load glTF/GLB. There is **no** `tinygltf` / Assimp in `thirdparty/` yet.

---

## Recommended architecture (smallest, cheapest)

**New level type that still lives in a normal level column** — same pattern as raster/vector, **not** a new column class like mesh/sound.

```
REF3D_TYPE     = 0x80   (new bit on TXshLevelType)
REF3D_XSHLEVEL = REF3D_TYPE | LEVELCOLUMN_XSHLEVEL
```

Why this shape:

1. `TXshSimpleLevel` is `final` and owns image frames. **Do not subclass it.** Treat 3D as a **new `TXshLevelType` on `TXshSimpleLevel`**, like `MESH_XSHLEVEL`, but **without** a new column class.
2. `TXshLevelColumn::canSetCell()` already accepts anything with `LEVELCOLUMN_XSHLEVEL`. A new column type would force edits in `txshcolumn.cpp`, `txsheet.cpp`, Fx DAG, Plastic, alpha-lock, masks. **Avoid that in MVP.**
3. Drawing tools stay on raster/vector. The 3D column is a **guide**. Brush tools already gate on `TZP_XSHLEVEL` / `PLI_XSHLEVEL` / `OVL_XSHLEVEL`.

### Data layout

```
Level (TXshSimpleLevel, type REF3D_XSHLEVEL)
  path ........ decoded .glb or .gltf (plus .bin / textures next to .gltf)
  model cache . Ref3dModel (CPU mesh + inverse bind + rest pose) loaded once
  poses ....... map<TFrameId, Ref3dPose>  // FK joint rotations

Xsheet cell
  (level, frameId)  // same as every other level
  pose for that frame, or rest pose if missing

Column
  TXshLevelColumn
  On create: setPreviewVisible(false)   // not in render/preview
             setCamstandVisible(true)   // visible in viewer
```

Do **not** store a `TImage` per pose. That would go through `ImageManager`, icon cache, and writers, and would be slow and fragile.

### Persistence (backward compatible)

`TXshSimpleLevel::loadData()` **throws** `unexpected tag` on unknown XML. So:

| Data | Where to store | Old Tahoma2D |
|---|---|---|
| Model file | Level `path` (`.glb` / `.gltf`) | Infers `OVL_XSHLEVEL` from unknown ext; load as raster **fails safely** if wrapped in existing `try/catch` in `ToonzScene::loadLevel`. Scene XML still valid. |
| Poses | **Sidecar** `name.t3dpose` (or `name.glb.t3dpose`) next to the model, **not** a new tag in the `.tnz` level block | Ignored |
| Default preview-off | Column `status` word already persisted | Harmless |

New Tahoma2D: if extension is `glb`/`gltf` → `REF3D_XSHLEVEL`. Old scenes without those files are unchanged.

### Rendering hook (viewer only)

Pipeline today:

```
StageBuilder::addFrame   (stage.cpp)
  → skips column if !isPreviewVisible() when rendering
  → addCell only builds a Player if TXshSimpleLevel (or sub-xsheet / mask)
OpenGlPainter::onImage   (stagevisitor.cpp)   // viewer
RasterPainter::onImage   (stagevisitor.cpp)   // icons / some raster paths
MovieRenderer / SceneFx  (fx graph)           // final render
```

MVP draw path:

- In `OpenGlPainter::onImage`, if `player.m_sl->getType() == REF3D_XSHLEVEL`, call `Ref3dDrawer::draw(...)` and **return**.
- In `RasterPainter::onImage`, **skip** 3D (or draw a 1-pixel dummy) so **icons / scene icons / render** do not skin meshes.
- Do **not** add a `TFx`. Preview-eye-off already keeps it out of `scenefx` / `movierenderer`.

### Posing

New tool `T_Ref3dPose` (like `T_Skeleton`, but only active when current level is `REF3D_XSHLEVEL`). Simple pose panel: joint list + Euler XYZ sliders. FK only.

---

## Library: tinygltf, not Assimp

| | tinygltf | Assimp |
|---|---|---|
| License | MIT (json.hpp MIT / Apache) | BSD-3, **much larger** |
| Size | Header-only | Heavy scene importer, many formats |
| CPU | Load once; we own a tiny GPU cache | Extra copies, more formats we do not need |

Vendor **only**:

- `thirdparty/tinygltf/tiny_gltf.h`
- `thirdparty/tinygltf/json.hpp` (the copy tinygltf ships)
- keep `TINYGLTF_IMPLEMENTATION` in **one** `.cpp` (`ref3dmodel.cpp`)

Do **not** enable tinygltf’s STB image if Tahoma already has PNG loaders you can reuse for `.gltf` extras; if you use bundled STB, isolate it with the macros tinygltf documents so it does not clash with other STB copies.

Default mannequin: **CC0 or self-made** `.glb` under `stuff/library/ref3d/`. Do not ship Mixamo / CSP / commercial figures.

---

## Low processing power (non-negotiable for MVP)

See `PERFORMANCE.md`. Short version:

- Load + GPU upload **once** per model, not per frame.
- Flat shade or silhouette; **no PBR, shadows, IBL, morphs, animation clips**.
- CPU skin **only when pose changes**; cache joint matrices.
- Cap vertices (e.g. skip / warn above ~50k tris).
- Never run in `tcomposer` / preview / movie render.
- One OpenGL draw per visible 3D column per viewer frame.

---

## Optional toggle

Add a preference (`enableRef3dLevel`, default **on** or **off** — off is safer for first merge) in `preferencesitemids.h` / `preferences.cpp`. Hide New/Load 3D Level when off. Existing workflows unchanged.

---

## PR split (what the original proposal asked for)

1. **Vendor tinygltf + load + `REF3D_XSHLEVEL` type + file association.** No viewer draw yet.
2. **Viewer draw** (flat / wire) in `OpenGlPainter` only. Preview eye off.
3. **FK pose panel + tool.** Sidecar pose save/load.
4. **UI**: New 3D Level, import filter, xsheet icon color, default mannequin.

Do not combine with IK, onion skin, or line-art conversion.

**Save + xsheet + keys:** see `SYNC_SAVE_KEYFRAME.md`. Joints are cels (`TFrameId`), not Function Editor channels. Column move/rotate already keyframes itself.

**App wiring (tools, viewer, browser, icons):** see `CODEBASE_HOOKS.md`. Must disable brushes on 3D levels and add an `IconGenerator` case or debug builds assert.

**Implement from this list only:** [`FINAL_CHECKLIST.md`](FINAL_CHECKLIST.md) — every NEW vs EDIT file for GUI, 3D, and level injection.

---

## Preferred project answer (for maintainers)

| Question | Suggested answer from this codebase |
|---|---|
| Accept this feature? | Fits the xsheet/camera workflow; keep it **guide-only**. |
| New level type vs overlay on existing viewer? | **New level type** on existing **level column**. Not a SceneViewer overlay (that would ignore xsheet/camera). Not MESH. |
| Read first | `txshleveltypes.h`, `txshsimplelevel.cpp` load/save, `stage.cpp` `addCell`/`addFrame`, `stagevisitor.cpp` `OpenGlPainter::onImage`, `toonzscene.cpp` `getLevelType`/`loadLevel`, `txshcolumn.cpp` preview flag. |
| Constraints | Do not add XML tags `TXshSimpleLevel` cannot skip. Do not put 3D into the Fx DAG. Do not reuse MESH. Do not touch brush tools. |
