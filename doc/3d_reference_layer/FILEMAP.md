# File map — what to create, what to edit, what to leave alone

Rule: **new 3D code lives in new files.** Existing files get **small, obvious switches** (`if (type == REF3D_XSHLEVEL)`), not refactors.

If a change needs more than ~20 lines in an old file, stop and put the logic in `ref3d*.cpp` instead.

---

## Files to CREATE (all new; cannot corrupt existing behavior if unused)

### Third-party (copy, do not rewrite)

| Path | Why |
|---|---|
| `thirdparty/tinygltf/tiny_gltf.h` | MIT glTF 2.0 loader |
| `thirdparty/tinygltf/json.hpp` | Required by tinygltf |
| `thirdparty/tinygltf/LICENSE` | Keep license with the code |

Optional: `stb_image.h` only if you cannot reuse Tahoma PNG loading for glTF extra images. Prefer **GLB** (embedded buffers) for the default model to avoid extra image code.

### Core library (toonzlib) — isolated module

| Path | Role |
|---|---|
| `toonz/sources/include/toonz/ref3dmodel.h` | CPU mesh, skeleton, inverse bind, rest pose. **No Qt UI.** |
| `toonz/sources/toonzlib/ref3dmodel.cpp` | `#define TINYGLTF_IMPLEMENTATION` **only here**. Load `.glb` / `.gltf` + `.bin` + relative images. |
| `toonz/sources/include/toonz/ref3dpose.h` | `std::vector<T3DPointD>` or quaternions per joint; `map<TFrameId, Ref3dPose>`; sidecar IO. |
| `toonz/sources/toonzlib/ref3dpose.cpp` | Read/write `*.t3dpose` (plain XML or JSON). Keep format versioned. |
| `toonz/sources/include/toonz/ref3ddrawer.h` | `draw(model, pose, TAffine, mode)` — immediate-mode or one VBO. |
| `toonz/sources/toonzlib/ref3ddrawer.cpp` | Flat shade / wire / outline. **No shaders required** for MVP (`glColor` + `GL_TRIANGLES`). |

### App UI (toonz)

| Path | Role |
|---|---|
| `toonz/sources/toonz/ref3dposepane.h` | Dockable pose panel (joint list + XYZ). |
| `toonz/sources/toonz/ref3dposepane.cpp` | Qt only. Writes into `Ref3dPoseStore` on the current simple level. |
| `toonz/sources/toonz/ref3dlevelpopup.h` | Optional: “New 3D Level” (name, path, default mannequin vs import). Can start as a branch of Level Create instead. |
| `toonz/sources/toonz/ref3dlevelpopup.cpp` | |

### Tool (tnztools)

| Path | Role |
|---|---|
| `toonz/sources/tnztools/ref3dposetool.cpp` | Click joint in viewer, drag to rotate (FK). Inactive on raster/vector. |
| `toonz/sources/include/tools/...` | Only if you add a small header; otherwise keep the tool in the `.cpp`. |

### Default asset

| Path | Role |
|---|---|
| `stuff/library/ref3d/default_mannequin.glb` | CC0 / original. Low poly (~2–8k tris). |
| `stuff/library/ref3d/LICENSE.txt` | Required. |

### Tests (later, not MVP)

`toonz/sources/toonzlib/tests/` if the project already has a pattern; otherwise a tiny CLI is enough. Do not block MVP on a test harness.

---

## Files to EDIT (surgical)

Touch **only** the listed functions. Do not “clean up” surrounding code.

### 1. Type enum — required

`toonz/sources/include/toonz/txshleveltypes.h`

Add **after** existing types (do not renumber):

```
REF3D_TYPE     = 0x80,
REF3D_XSHLEVEL = REF3D_TYPE | LEVELCOLUMN_XSHLEVEL,  // 0x82
```

`0x80` is unused today (`ZERARYFX_XSHLEVEL` is `1 << 7` which is also 128). **Collision check:** `ZERARYFX_XSHLEVEL = 1 << 7` **is 128 = 0x80**.

**Do not use 0x80.** Use the same style as mesh:

```
REF3D_XSHLEVEL = 8 << 7,   // 1024 — next free after PEGBAR_XSHLEVEL = 7 << 7
```

`LEVELCOLUMN_XSHLEVEL` is `0x2`. Mesh does **not** OR that bit (`MESH_XSHLEVEL = 5 << 7`), which is why it needs `TXshMeshColumn`.

For a **normal level column**, you **must** OR `LEVELCOLUMN_XSHLEVEL`:

```
REF3D_XSHLEVEL = (8 << 7) | LEVELCOLUMN_XSHLEVEL,  // 1026
```

Then `canSetCell` (`sl->getType() & LEVELCOLUMN_XSHLEVEL`) is true. **No new column class.**

### 2. File type — required for import

`toonz/sources/include/tfiletype.h`  
Optional: add `GLTF_IMAGE = 0x100` and `GLTF_LEVEL = GLTF_IMAGE | LEVEL`. **Safer MVP:** reuse a dedicated type so `ToonzScene::getLevelType` does not treat glb as raster.

If you do **not** add a new `TFileType`, `getInfo("a.glb")` is `UNKNOW_FILE` and `loadLevel` returns 0. So you **must** either:

- `TFileType::declare("glb", ...)` and `"gltf"`, **or**
- Special-case extensions in `ToonzScene::getLevelType`.

Preferred: declare in `toonz/sources/image/tiio.cpp` next to `"mesh"`:

```
TFileType::declare("glb", TFileType::GLTF_LEVEL);
TFileType::declare("gltf", TFileType::GLTF_LEVEL);
```

Do **not** register `TLevelReader` for glb. glTF is not a `TImage` sequence. Load via `Ref3dModel`, not `TLevelReaderP`.

`toonz/sources/common/tsystem/tfiletype.cpp` — only if `declare()` is not enough (it should be).

### 3. Scene load / new level — required

`toonz/sources/toonzlib/toonzscene.cpp`

| Function | Change |
|---|---|
| `getLevelType()` | `glb`/`gltf` → `REF3D_XSHLEVEL` |
| `getFolderName()` | `REF3D_XSHLEVEL` → `TProject::Extras` (same as overlay) |
| `getDefaultLevelPath()` | `.glb` |
| `createNewLevel()` | After creating `TXshSimpleLevel`, **skip** `initializePalette` / raster dpi if type is 3D (or no-op those for this type). Copy default mannequin path. |
| `loadLevel()` | After `setType`, call `Ref3dModel::load(path)` instead of `xl->load()` image frames. Still insert into `levelSet`. |
| `getImportedLevelPath()` | Let extras folder handle it (already default). Copy **sidecar** `.bin` + textures when importing `.gltf`. |

Keep `createNewLevel` raster/vector branches **byte-identical**.

### 4. Simple level persist / type from extension — required

`toonz/sources/toonzlib/txshsimplelevel.cpp`

| Function | Change |
|---|---|
| `loadData()` | In the extension switch (~1081): `glb`/`gltf` → `REF3D_XSHLEVEL`. **Do not add a new XML tag.** |
| `createEmptyFrame()` | `case REF3D_XSHLEVEL: return TImageP();` — never allocate a raster. |
| `getBBox()` | Return a unit/model AABB from `Ref3dModel`, not `getFrame()`. |
| `load()` / `save()` | **Critical early-out** before `TLevelReaderP(path)` (~1239). glb is not an image; that constructor will throw or mis-read. Load model + `*.t3dpose`; `setFrame(fid, TImageP())` so fids exist. `save()` writes sidecar only, never re-encodes glb. |
| Other `switch (getType())` | Add `case REF3D_XSHLEVEL:` next to mesh where it would otherwise fall into **raster** (that would try to open glb as png). |

Search this file for `MESH_XSHLEVEL` and add a parallel case each time. That is the full list of dangerous fall-throughs.

### 5. Stage visit — required for display

`toonz/sources/toonzlib/stage.cpp`

`addCell()` already stores a Player for any `TXshSimpleLevel`. **Probably no change** if 3D is a simple level.

Confirm `player.image()` is not required for OpenGL draw (you will use `player.m_sl` + `player.m_fid`).

When **creating** the column from UI, set preview off. That is not in `stage.cpp`.

### 6. Viewer paint — required

`toonz/sources/toonzlib/stagevisitor.cpp`

| Function | Change |
|---|---|
| `OpenGlPainter::onImage` | After plastic check: if `player.m_sl && player.m_sl->getType() == REF3D_XSHLEVEL` → `Ref3dDrawer::draw` → `goto` pop matrix (do not call `player.image()`). |
| `RasterPainter::onImage` | If 3D type, **return immediately** (no icon rasterization of a skinned mesh). |

Do not add 3D to `onMeshImage` / plastic helpers.

### 7. Column preview default — required for “guide”

When the user **creates or imports** a 3D level into the xsheet:

- `column->setPreviewVisible(false);`
- `column->setCamstandVisible(true);`

Find the insert-column site used by `levelcreatepopup.cpp` / `levelcommand.cpp` (same place raster levels get a column). One `if (type == REF3D_XSHLEVEL)` there.

Do **not** change `TXshColumn::isPreviewVisible` globally.

### 8. UI — required for artists

| File | Change |
|---|---|
| `toonz/sources/toonz/levelcreatepopup.cpp` | Combo item `"3D Reference Level"` with `REF3D_XSHLEVEL`. Hide width/dpi. |
| `toonz/sources/toonz/menubarcommandids.h` | `#define MI_NewRef3dLevel "MI_NewRef3dLevel"` |
| `toonz/sources/toonz/mainwindow.cpp` | `createMenuLevelAction(...)` |
| `toonz/sources/toonz/menubar.cpp` | Add under Level menu |
| `toonz/sources/include/tools/toolcommandids.h` | `#define T_Ref3dPose "T_Ref3dPose"` |
| `toonz/sources/tnztools/CMakeLists.txt` | Add `ref3dposetool.cpp` |
| `toonz/sources/toonz/CMakeLists.txt` | Add pose pane sources + MOC header |
| `toonz/sources/toonzlib/CMakeLists.txt` | Add `ref3d*.cpp` + headers; `include_directories` for `thirdparty/tinygltf` |
| `toonz/sources/CMakeLists.txt` | Only if you add an option `WITH_REF3D` (recommended: `ON` by default once stable, or `ON` always since tinygltf is header-only) |

File browser / load dialog: wherever image extensions are listed (search `png` / `pli` in load-level file dialog), add `*.glb *.gltf`. Typical files: `filebrowserpopup` / `iocommand.cpp` — **search at implementation time**, add extension string only.

### 9. Preferences toggle — recommended

| File | Change |
|---|---|
| `toonz/sources/include/toonz/preferencesitemids.h` | `enableRef3dLevel` |
| `toonz/sources/toonzlib/preferences.cpp` | Default bool |
| Preferences UI `.cpp` in `toonz/` | One checkbox |

### 10. Tools — required (otherwise brushes may enable on 3D)

`toonz/sources/include/tools/tool.h` — `Ref3dImage = 0x100` on `ToolTargetType`. Do **not** add to `CommonImages`.

`toonz/sources/tnztools/tool.cpp`

- `bind()`: dummy + register slot for `Ref3dImage` (copy the `MeshImage` lines).
- `updateEnabled()`: next to the `MESH_XSHLEVEL` checks (~1020 and ~1139): if type is 3D and tool lacks `Ref3dImage`, disable.

### 11. Icons — required (debug assert)

`toonz/sources/toonzqt/icongenerator.cpp` — every `switch (type)` that has `MESH_XSHLEVEL`. `case REF3D_XSHLEVEL:` return a **static** pixmap. Do not skin.

### 12. Xsheet cosmetics — optional MVP

`xshcellviewer.cpp` / `xshcolumnviewer.cpp` — cell color / icon for the new type so it is not mistaken for a broken raster. Low risk if you only add a `case`.

---

## Files you must NOT edit (for this feature)

Editing these is how the drawing app gets corrupted.

| Area | Paths | Why |
|---|---|---|
| Plastic / 2D mesh | `txshmeshcolumn.*`, `plasticdeformerfx.cpp`, `tnzext/mesh*`, `plastictool*.cpp`, `tiio_mesh.*` | Different feature |
| SceneViewer 3D camera | `sceneviewer.cpp` `m_theta3D` / `m_phi3D` / `m_draw3DMode` | Table perspective, not models |
| 2D skeleton / IK | `skeleton.cpp`, `skeletontool.cpp`, `ikengine.cpp`, `ikskeleton.*` | Pegbar IK, 2D |
| Brush / fill / vector tools | `*brushtool.cpp`, `filltool.cpp`, `vector*.cpp` | Must keep working on TZP/PLI/OVL only |
| Final render graph | `scenefx.cpp`, `movierenderer.cpp`, `multimediarenderer.cpp`, `tcomposer`, `stdfx/*` | Preview-eye already excludes the column |
| Core image types | `trasterimage`, `ttoonzimage`, `tvectorimage` | Do not invent a fake TImage for glTF |
| Persist of simple level tags | Do not add tags inside `TXshSimpleLevel::saveData` | Old Tahoma **throws** `unexpected tag` |
| `TXshSimpleLevel` becoming non-`final` | — | Huge blast radius |
| New `TXshColumn` subclass | `txshcolumn.cpp` `createEmpty` / `toColumnType` | Avoid for MVP |
| Fx DAG | `tcolumnfx.cpp`, `fxdag.cpp` | 3D must not become a render fx |
| Third-party besides tinygltf | zlib, libpng, glew, … | Unrelated |

---

## CMake notes

`toonz/sources/toonzlib/CMakeLists.txt`:

- Append `ref3dmodel.cpp`, `ref3dpose.cpp`, `ref3ddrawer.cpp` to `SOURCES`
- Append matching headers to `HEADERS`
- `include_directories(${CMAKE_SOURCE_DIR}/../thirdparty/tinygltf)` (adjust to actual root; this repo’s thirdparty is `tahoma2d/thirdparty`)

tinygltf is header-only: **no extra .lib**.

---

## Import of “gltf + the other files”

`.gltf` is JSON + external:

- `*.bin` buffers
- texture images (`png`/`jpg`)
- sometimes more `.gltf` buffers

On import (`getImportedLevelPath` / copy into `+extras`):

1. Copy the `.gltf`
2. Copy every URI listed in `buffers` and `images` (tinygltf `LoadASCIIFromFile` needs them beside the file)
3. Copy `*.t3dpose` if present

`.glb` is one file — prefer it for the default mannequin.

Do **not** try to pack user `.gltf` into a new `.glb` in MVP (extra code, easy to corrupt user data). Copy the folder as-is.

---

## How to keep diffs reviewable

1. First PR: thirdparty + `REF3D_XSHLEVEL` + load + empty column. Viewer still blank.
2. Second PR: `Ref3dDrawer` + 10-line hook in `OpenGlPainter`.
3. Third PR: pose sidecar + panel + tool.
4. Fourth PR: menus, preference, default model, xsheet color.

Never mix a refactor of `TXshSimpleLevel` into these PRs.
