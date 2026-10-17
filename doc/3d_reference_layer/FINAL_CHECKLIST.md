# Final checklist — files we actually touch

This is the **only** list to implement against. Everything else: **do not open**.

Legend:

- **NEW** = create file (safe)
- **EDIT** = few lines in an existing file (`if (REF3D)` / add combo item / CMake line)
- **MUST** = feature does not work / can crash without it
- **GUI** = menus, dialogs, toolbar, panes
- **INJECT** = level type + load/save + xsheet
- **3D** = model / pose / draw
- skip = not MVP (later)

---

## A. NEW files — all 3D + GUI we own

| File | Role | MUST |
|---|---|---|
| `thirdparty/tinygltf/tiny_gltf.h` | Loader | MUST |
| `thirdparty/tinygltf/json.hpp` | tinygltf dep | MUST |
| `thirdparty/tinygltf/LICENSE` | License | MUST |
| `toonz/sources/include/toonz/ref3dmodel.h` | CPU mesh + skeleton | MUST |
| `toonz/sources/toonzlib/ref3dmodel.cpp` | Load glb/gltf (`TINYGLTF_IMPLEMENTATION` **only here**) | MUST |
| `toonz/sources/include/toonz/ref3dpose.h` | Pose per `TFrameId` + sidecar | MUST (phase 3) |
| `toonz/sources/toonzlib/ref3dpose.cpp` | `*.t3dpose` IO | MUST (phase 3) |
| `toonz/sources/include/toonz/ref3ddrawer.h` | Draw API | MUST (phase 2) |
| `toonz/sources/toonzlib/ref3ddrawer.cpp` | Flat / wire OpenGL | MUST (phase 2) |
| `toonz/sources/toonz/ref3dposepane.h` | Pose panel widget | MUST (phase 3 GUI) |
| `toonz/sources/toonz/ref3dposepane.cpp` | Joint list + XYZ + `TPanelFactory` + Windows menu | MUST (phase 3 GUI) |
| `toonz/sources/tnztools/ref3dposetool.cpp` | FK click-drag in viewer | MUST (phase 3 GUI) |
| `stuff/library/ref3d/default_mannequin.glb` | CC0 low-poly default | GUI later |
| `stuff/library/ref3d/LICENSE.txt` | Asset license | GUI later |

**Do not create:** new column class, new `TImage`, `ref3dlevelpopup` (use existing New Level dialog).

---

## B. EDIT — level injection (type + load + save)

| File | What to change | Lines (approx) | MUST |
|---|---|---|---|
| `toonz/sources/include/toonz/txshleveltypes.h` | `REF3D_XSHLEVEL = (8 << 7) \| LEVELCOLUMN_XSHLEVEL` | 2 | MUST |
| `toonz/sources/include/tfiletype.h` | `GLTF_IMAGE` / `GLTF_LEVEL` enum | 3 | MUST |
| `toonz/sources/image/tiio.cpp` | `TFileType::declare("glb")` and `"gltf"` **no** `TLevelReader` | 2 | MUST |
| `toonz/sources/toonzlib/toonzscene.cpp` | `getLevelType`, `getFolderName`, `getDefaultLevelPath`, `createNewLevel` skip raster init, `loadLevel` 3D branch | ~30 | MUST |
| `toonz/sources/toonzlib/txshsimplelevel.cpp` | ext→type in `loadData`; **early-out `load()`/`save()`** before `TLevelReaderP`; `createEmptyFrame`; `getBBox`; every `MESH_XSHLEVEL` fall-through (~5 spots) | ~40 | MUST |
| `toonz/sources/toonz/iocommand.cpp` | After `exposeLevel`, if 3D: `column->setPreviewVisible(false)` | ~5 | MUST |
| `toonz/sources/toonzlib/CMakeLists.txt` | Add `ref3d*.cpp/h` + tinygltf include | ~8 | MUST |

**Not edited for injection:** `txshcolumn.cpp`, `txsheet.cpp`, `txshlevelcolumn.cpp`, `scene.tnz` tag format, `scenefx.cpp`. Cells + columns already accept `LEVELCOLUMN_XSHLEVEL`.

---

## C. EDIT — draw (viewer, not SceneViewer)

| File | What to change | MUST |
|---|---|---|
| `toonz/sources/toonzlib/stagevisitor.cpp` | `OpenGlPainter::onImage`: if REF3D → `Ref3dDrawer::draw`, skip `player.image()`. `RasterPainter::onImage`: if REF3D **return** | MUST |

**Do not edit** `sceneviewer.cpp`, `stage.cpp` (Player already built for simple levels).

---

## D. EDIT — GUI

| File | What to change | MUST |
|---|---|---|
| `toonz/sources/toonz/levelcreatepopup.cpp` | Combo `"3D Reference Level"`; hide dpi/size; on apply skip raster palette; `setPreviewVisible(false)` on new column | MUST |
| `toonz/sources/toonz/menubarcommandids.h` | `#define MI_NewRef3dLevel` and `MI_OpenRef3dPose` | MUST |
| `toonz/sources/toonz/mainwindow.cpp` | `createMenuLevelAction(MI_NewRef3dLevel, ...)` next to New Raster | MUST |
| `toonz/sources/toonz/menubar.cpp` | Level menu item next to `MI_NewRasterLevel`; Windows menu for pose pane | MUST |
| `toonz/sources/include/tools/toolcommandids.h` | `#define T_Ref3dPose "T_Ref3dPose"` | MUST |
| `toonz/sources/toonz/toolbar.h` | One `{T_Ref3dPose, ...}` next to `T_Skeleton` | MUST |
| `toonz/sources/include/tools/tool.h` | `Ref3dImage = 0x100` — **not** in `CommonImages` | MUST |
| `toonz/sources/tnztools/tool.cpp` | `bind()` Mesh-style slot; `updateEnabled()` disable brushes on REF3D | MUST |
| `toonz/sources/toonzqt/icongenerator.cpp` | 4× `case REF3D_XSHLEVEL:` static pixmap (next to MESH) | MUST |
| `toonz/sources/toonz/CMakeLists.txt` | `ref3dposepane.cpp/h` in SOURCES + MOC | MUST |
| `toonz/sources/tnztools/CMakeLists.txt` | `ref3dposetool.cpp` | MUST |
| `toonz/sources/toonzqt/CMakeLists.txt` | **none** unless icon pixmap lives there; `icongenerator.cpp` is already in the lib | — |
| `toonz/sources/include/toonz/preferencesitemids.h` | `enableRef3dLevel` | recommended |
| `toonz/sources/toonzlib/preferences.cpp` | default bool | recommended |
| `toonz/sources/toonz/preferencespopup.cpp` | one checkbox | recommended |
| `toonz/sources/toonz/xshcellviewer.cpp` | cell tint `case REF3D` | skip MVP |
| `toonz/sources/toonz/xshcolumnviewer.cpp` | column icon color | skip MVP |

Pose pane factory + `OpenFloatingPanel` live **inside** new `ref3dposepane.cpp` (same pattern as `tpanels.cpp`). **Do not** add a factory into the middle of `tpanels.cpp` unless CMake linking requires it — prefer self-registering static in the new file.

---

## E. Count (MVP must)

**NEW:** 12 files (tinygltf 3 + ref3d 6 + pose pane 2 + pose tool 1). Default glb later.

**EDIT:** 16 files if you include prefs + CMake; **13** without prefs.

| Bucket | EDIT files |
|---|---|
| Inject | `txshleveltypes.h`, `tfiletype.h`, `tiio.cpp`, `toonzscene.cpp`, `txshsimplelevel.cpp`, `iocommand.cpp`, `toonzlib/CMakeLists.txt` |
| Draw | `stagevisitor.cpp` |
| GUI | `levelcreatepopup.cpp`, `menubarcommandids.h`, `mainwindow.cpp`, `menubar.cpp`, `toolcommandids.h`, `toolbar.h`, `tool.h`, `tool.cpp`, `icongenerator.cpp`, `toonz/CMakeLists.txt`, `tnztools/CMakeLists.txt` |

Stay under “don’t rewrite the app”: each EDIT is a switch or a list entry, not a refactor.

---

## F. Order of work (same as PHASES)

1. tinygltf + type + `tiio` declare + `toonzscene` + `txshsimplelevel` load early-out + CMake → **Load Level shows glb**
2. `ref3ddrawer` + `stagevisitor` + `iocommand` preview-off + `icongenerator` + `tool.cpp` disable → **visible guide, brushes safe, no crash**
3. `ref3dpose` + pane + tool + toolbar + menus + levelcreate combo → **pose + GUI**
4. default glb + prefs checkbox + xsheet tint → polish

---

## G. Do not touch (final)

- `sceneviewer.cpp` (3D camera mode)
- `txshcolumn.cpp` / new column type
- `txshsimplelevel` XML tags (`saveData` new children)
- `scenefx.cpp` `movierenderer.cpp` `tcomposer`
- `*brushtool.cpp` `filltool.cpp` `plastictool*` `skeletontool.cpp`
- `tiio_mesh.*` `txshmeshcolumn.*`
- `tstageobject.cpp` (no joint channels)

---

## H. Done when

- New Level → 3D Reference Level → column in xsheet, preview eye **off**
- Load Level / browser shows `.glb` `.gltf`
- Mannequin in viewer, **not** in Render
- Brush disabled on that column; Animate still moves the column
- Pose panel + tool rotate a joint; change frame; pose holds
- Save All writes `.tnz` + `*.t3dpose`, does not rewrite `.glb`
- Old scenes open; raster/vector unchanged
