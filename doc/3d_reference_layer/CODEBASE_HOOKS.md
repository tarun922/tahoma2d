# Extra codebase map — inject 3D without fighting the app

This is the live wiring: **handles, viewer, tools, browser, icons, panes**. Read with `SYNC_SAVE_KEYFRAME.md`.

---

## App clock: `TApp` (singleton)

`toonz/sources/toonz/tapp.h`

Everything current is here. 3D code should **read** these, not invent a second current-frame:

| Handle | Meaning for 3D |
|---|---|
| `getCurrentFrame()` | Xsheet row **or** level-strip fid. `isEditingScene()` vs `isEditingLevel()` |
| `getCurrentColumn()` | Which column is selected |
| `getCurrentLevel()` | Cast / strip level (`TXshSimpleLevel*`) |
| `getCurrentXsheet()` | Cells + pegbars |
| `getCurrentObject()` | Column `TStageObject` (whole-model keys) |
| `getCurrentTool()` | Active tool |
| `getCurrentScene()` | Dirty flag, `ToonzScene*` |

When the user scrubs the xsheet, `TFrameHandle` changes → SceneViewer repaints → `Stage::visit` with that **row**. Pose lookup is `cell.m_frameId`. No extra signal.

**Level Strip mode** (`isEditingLevel()`): viewer draws **one level**, not the whole xsheet (`sceneviewer.cpp` ~2331 `Stage::visit(painter, currentLevel, ...)`). 3D must still draw from `player.m_sl` + `player.m_fid` so both modes work with the same drawer.

---

## Viewer paint path (do not rewrite SceneViewer)

```
SceneViewer::paintGL()
  drawCameraStand()          // working view (not full preview)
    OpenGlPainter painter
    Stage::visit(painter, args)   // xsheet compositing
      StageBuilder::addFrame / addCell
      OpenGlPainter::onImage(player)
  drawPreview()              // skip: preview eye off
```

`m_draw3DMode` here is **table/camera 3D**, not models. Leave it.

**Inject only in `OpenGlPainter::onImage`.** SceneViewer stays untouched for MVP.

Depth: camera-stand 2D view uses `glDisable(GL_DEPTH_TEST)`. A 3D mannequin still looks OK as a **flat projection** (orthographic, like CSP’s default). Enabling depth **only inside** `Ref3dDrawer::draw` (push attrib / enable / draw / pop) is enough. Do not turn on depth for the whole viewer.

Picking (click joint): `sceneviewer.cpp` already has a pick path with `Stage::visit(picker, ...)`. MVP pose tool can ray-pick in the tool’s `leftButtonDown` using the same view matrix as the viewer (`TTool::m_viewer`). Do not add a new pick visitor in Phase 1–2.

---

## Tools — why brushes must stay off, pose tool must opt in

`TTool::updateEnabled()` (`tnztools/tool.cpp` ~946) gates tools by **level type**:

```
PLI  → needs VectorImage
TZP  → needs ToonzImage
OVL  → needs RasterImage
MESH → needs MeshImage
```

If the level is **`REF3D_XSHLEVEL`**, none of those `if`s fire. Execution **falls through**. Then `LevelWriteTool` (brush, fill, eraser) may enable and call `getImage()` on an empty 3D “frame” → crash or corrupt.

**Required small edit in `tool.cpp`:**

```
if (levelType == REF3D_XSHLEVEL && !(targetType & Ref3dImage))
  return enable(false), tr("The current tool cannot be used on a 3D Reference Level.");
```

Add `Ref3dImage = 0x100` to `TTool::ToolTargetType` in `tool.h` (next free after `EmptyTarget = 0x80`). **Do not** add it to `CommonImages` / `AllImages` / `CommonLevels`. Brushes stay off automatically.

`TTool::bind()` (`tool.cpp` ~198) only registers Toonz/Vector/Raster/Mesh in `toolTable`. Pose tool:

```
bind(TTool::Ref3dImage | TTool::LevelColumns);
```

and extend `bind()` with the same dummy-slot pattern used for `MeshImage` (4–6 lines). Copy mesh, do not invent a new registry.

**Do not reuse `T_Skeleton`.** It builds a 2D pegbar `Skeleton` (`skeletontool.cpp` `buildSkeleton`). Wrong data.

**Animate tool** (`edittool.cpp` `bind(AllTargets)`) already moves the **column**. Keep it: artists key the whole mannequin with Animate, joints with Pose.

Pose tool type: `LevelReadTool` (does not write pixels). Playback already disables all tools (`isPlaying()`).

---

## File browser / Load Level — `TFileType` is the gate

`filebrowser.cpp` ~584: if the popup has **no filter list**, files with `TFileType::getInfo == UNKNOW_FILE` are **hidden**.

Load Level popup (`LoadLevelPopup`) does **not** list `png`/`pli` itself; it relies on that. So:

**`TFileType::declare("glb" / "gltf", ...)` is what makes files appear.** No need to edit every popup.

`IoCmd::loadLevel` (`iocommand.cpp` ~932):

- `scene->loadLevel(actualPath, ...)`
- if `!xl` → `"this filetype is not supported"`
- then exposes cells on the xsheet

If `ToonzScene::getLevelType` returns `REF3D_XSHLEVEL` and `loadLevel` creates a simple level, **Load Level already works**. After load, set preview eye off (one `if` where the column is first exposed — same function after `expose`).

Drag-drop onto the xsheet uses the same `loadLevel` / `TFileType` path.

---

## Icons — `assert(false)` if we forget this

`toonzqt/icongenerator.cpp` ~1434:

```
switch (sl->getType()) {
  OVL / TZI → RasterImageIconRenderer
  PLI       → VectorImageIconRenderer
  TZP       → ToonzImageIconRenderer
  MESH      → MeshImageIconRenderer
  default:  assert(false);
}
```

Unknown type **asserts in debug** and can lock the icon thread. Filmstrip, xsheet cell thumbs, cast all call this.

**Lightweight:** `case REF3D_XSHLEVEL: return a static QPixmap (mannequin silhouette);` **Do not** offscreen-render the skinned mesh. Weak PC + `TOfflineGL` = extra context, extra RAM, easy crashes.

Same switch appears more than once in that file (~1451, 1539, 1581, 1619). Grep `MESH_XSHLEVEL` and add the 3D case beside each.

Filmstrip (`filmstrip.cpp`) lists `sl->getFids()`. If we `setFrame(fid, TImageP())` when loading poses, the strip shows pose slots. Icon = static pixmap. Good enough for MVP.

---

## Panes / rooms

Dock widgets: `TPanelFactory("Name")` in `tpanels.cpp` + `OpenFloatingPanel(MI_..., "Name", title)`.

Pose panel: **new factory** in a **new file** (`ref3dposepane.cpp`) or a short block at the **end** of `tpanels.cpp`. Prefer new file + one line in `toonz/CMakeLists.txt` to keep `tpanels.cpp` diffs tiny.

Do **not** add the pane to default rooms in `mainwindow.cpp` in MVP. Users open it from Windows menu. Existing layouts stay valid (`TPanelFactory::createPanel` unknown name already has a fallback path).

---

## `TApp` / undo / dirty (pose edits)

Pattern used everywhere:

```
sl->setDirtyFlag(true);
TApp::instance()->getCurrentScene()->setDirtyFlag(true);
TUndoManager::manager()->add(new Ref3dPoseUndo(...));
TApp::instance()->getCurrentXsheet()->notifyXsheetChanged(); // if cells changed
```

Pose undo = swap `Ref3dPose` for one `TFrameId`. Do not snapshot the mesh.

Notify `IconGenerator::invalidate(sl, fid)` only if you show pose-specific thumbs (MVP: static icon → skip).

---

## What falls out for free

| Feature | Why it already works |
|---|---|
| Column move/rotate/scale keys | `TStageObject` on `TXshLevelColumn` |
| Hold / expose / copy cells | `TXshCell (level, fid)` |
| Sub-xsheet | `StageBuilder` recurses |
| Camera | existing visit |
| Render exclusion | preview eye |
| Save scene XML | level path + cells |
| Animate tool | `AllTargets` / column tool |

## What will break if we skip the hook

| Skip | Symptom |
|---|---|
| `TFileType::declare` | glb invisible in browser |
| `getLevelType` / `load()` early-out | “filetype not supported” or `TLevelReader` throw |
| `tool.cpp` REF3D disable | Brush enabled on 3D → crash |
| `icongenerator` case | assert / hung thumbs |
| `OpenGlPainter` hook | Level exists, viewer blank |
| Preview eye off | 3D in final render (slow + wrong) |

---

## Weak-PC notes from this pass

- IconGenerator uses a **background GL context**. Never queue mesh skinning there.
- Filmstrip `loadAllIconsAndPutInCache` would try to cache every “frame”. For 3D, `load()` must not register hundreds of empty fids; only fids that exist in the sidecar (often < 50).
- `TTool::updateEnabled` runs on every frame/column change — keep the 3D check a single int compare.
- SceneViewer already redraws the whole stage; 3D cost is **one extra `onImage`**, not a second widget.
