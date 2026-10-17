# Sync: xsheet, keyframes, save — how to inject 3D without fighting Tahoma

This is the “how it actually runs” map. Use it so the 3D pose layer **follows the same clock** as drawings, stays **cheap on weak PCs**, and does not break Save Scene.

---

## There are two timelines. Use both, do not mix joints into the wrong one.

```
Xsheet ROW  (scene time) ──────────────────────────────────────────►
     │
     │  A) CEL / EXPOSURE          B) STAGE TRANSFORM
     │     TXshCell                TStageObject (column pegbar)
     │     (level, TFrameId)       TDoubleParam: X Y Z Angle Scale…
     │     hold = same fid         interpolates BETWEEN rows
     │     copied like a drawing   diamond keys in xsheet
     │
     ▼
  Viewer looks up cell at current row
  → StageBuilder::addCell()
  → player.m_fid + pegbar->getPlacement(row)
```

| What the artist wants | Which timeline | Extra 3D code? |
|---|---|---|
| Pose of the mannequin (arm up, then arm down) | **A — cells / TFrameId** | Yes: pose map on the level |
| Move / rotate / scale the whole model in the scene | **B — column keys** | **No** — already works if 3D is a normal `TXshLevelColumn` |
| Camera | Camera `TStageObject` | **No** |
| In-betweens of joints (smooth FK) | Later optional | **Do not** put 50 joints on `TDoubleParam` |

**Lightweight rule:** joints are **cels**, not Function Editor curves.

- Pose stored once per `TFrameId` (like drawing 1, 2, 3).
- Xsheet **exposure** holds that pose on many rows (same as holding a drawing).
- Changing the current **row** already changes `player.m_fid` in `stage.cpp`. Drawer looks up `pose[fid]`. **Zero extra clock.**
- Interpolating every joint every frame on a cheap laptop is the slow path. Skip it for MVP.

Do **not** add joint channels to `TStageObject::Channel` / `T_ChannelCount`. That would:

- Rewrite `tstageobject.cpp` keyframe cache
- Flood the Function Editor
- Call `TDoubleParam::getValue(frame)` for every joint on every viewer redraw

---

## How the viewer stays in sync today (hook here)

On each redraw, `StageBuilder::addCell()` (`stage.cpp` ~330):

1. `TXshCell cell = xsh->getCell(row, col);`
2. `TXshSimpleLevel *sl = cell.m_level->getSimpleLevel();`
3. `pegbar->getPlacement(row)` → column affine (already keyframed)
4. Builds `Stage::Player` with `m_sl`, `m_fid`, `m_placement`

`OpenGlPainter::onImage` then draws `player.image()`.

**3D inject (4–8 lines):**

```
if (player.m_sl && player.m_sl->getType() == REF3D_XSHLEVEL) {
  Ref3dPose pose = Ref3dPoseStore::get(player.m_sl, player.m_fid); // rest if missing
  Ref3dDrawer::draw(player.m_sl, pose, player.m_placement * player.m_dpiAff);
  // pop matrix, return
}
```

Playback, onion-skin row list, sub-xsheet, and camera already feed this path. Do not add a second timer or a Qt animation.

**Cache:** if `(sl, fid, pose-revision)` unchanged since last draw, skip CPU skin. Scrubbing a held cell is then just one GL draw of a cached mesh — cheap enough for weak laptops.

---

## How artists “key” a pose (cel style, not stage keys)

Same gestures they already use for drawings:

| Artist action | Existing code | 3D meaning |
|---|---|---|
| New drawing in level strip | `setFrame(fid, image)` | `setPose(fid, currentPose)` + empty `setFrame(fid, TImageP())` so the fid **exists** in `m_frames` |
| Expose on xsheet | `TXshLevelColumn::setCell(row, cell)` | Same cell `(level, fid)` — pose follows for free |
| Hold / extend | copy same fid down the column | Same pose, no extra memory |
| Duplicate drawing | copy frame in strip | copy pose map entry |
| Delete drawing | `eraseFrame` | erase pose for that fid |

`m_frames` on `TXshSimpleLevel` is the set of **valid fids**. Even with no `TImage`, `setFrame(fid, TImageP())` is how scan levels already register frames (`txshsimplelevel.cpp` load path). Reuse that: 3D “drawings” are pose slots.

Xsheet **diamond keys** (`CellArea::drawKeyframe`) are **column transform** keys (`TStageObject::isKeyframe(row)`). They must keep meaning “move the whole layer”. Do not draw joint poses as those diamonds in MVP — artists would think the arm is on the pegbar.

If you want a visual for “this cell has a unique pose”, tint the **cell**, not the key diamond (`xshcellviewer.cpp` cell color `case`).

---

## Project save — two files, two jobs

`IoCmd::saveAll()` (`iocommand.cpp`):

```
saveScene()                  → ToonzScene::save()  writes  scene.tnz
SceneResources::save()       → each SceneLevel::save()  writes level files
```

### 1) `scene.tnz` (XML)

`ToonzScene::save()` writes only:

- `properties`
- `levelSet` → each `TXshSimpleLevel::saveData()` = **name + path + info attrs**
- `xsheet` → columns (cells = row, count, **level pointer**, fid, increment) + pegbars (transform keys) + fxnodes

Cells do **not** embed image pixels or poses. They only say “column 3, rows 10–16, level `mannequin`, fid `0003`”.

**3D:** keep `saveData()` looking like a normal simple level:

```
<level>
  mannequin
  <path>+extras/mannequin.glb</path>
  <info dpiType="image"/>
</level>
```

Old Tahoma: unknown extension → treated as overlay; `load()` fails inside try/catch in `loadResources`. Scene still opens.

**Do not** add a `<ref3dPoses>` tag. `loadData()` throws `unexpected tag`.

Pegbar keys in `xsheet/pegbars` already save column X/Y/Angle. Whole-model motion is free.

### 2) Level files on disk (`SceneLevel::save`)

`SceneResources` walks every `TXshSimpleLevel` and calls `m_sl->save(actualFp)` if dirty or path changed.

`TXshSimpleLevel::load()` / `save()` currently go through `TLevelReaderP` / `TLevelWriterP` (image IO). **glb is not an image.** If `load()` is not short-circuited, `TLevelReaderP(path)` on a `.glb` will throw or mis-detect.

**Required early-out in `TXshSimpleLevel::load()` and `save()`:**

```
if (getType() == REF3D_XSHLEVEL || ext is glb/gltf) {
  Ref3dModelCache::ensureLoaded(decodedPath);  // once
  Ref3dPoseStore::loadSidecar(decodedPath.withType("t3dpose"));
  // register fids from sidecar (or fid 1 if empty)
  return;
}
```

`save()` for 3D:

- **Do not rewrite the .glb** unless the user replaced the model.
- Write/update `mannequin.t3dpose` next to it (small JSON/XML: fid → joint rotations).
- Copy `.gltf` extras (`.bin`, textures) on Save As / collect, same as `TXshSimpleLevel::copyFiles`.

Hook `SceneLevel::save()` only if the generic `m_sl->save()` early-out is not enough (prefer early-out so `sceneresources.cpp` stays unchanged).

### Load order (already correct)

```
ToonzScene::load()
  loadNoResources()  → parse .tnz, build empty levels + xsheet cells
  loadResources()    → for each level: level->load()
```

Xsheet cells already point at levels **before** pixels/models exist. After `Ref3dModel` loads, the next viewer paint works. Same as raster.

`loadResources` wraps `level->load()` in try/catch — a bad glb must **not** abort the whole scene.

`makeSceneIcon()` calls `renderFrame` during save. Preview-eye-off already skips 3D. Do not skin a mesh just to make the 128px icon.

---

## Dirty flags

| Flag | Who sets it | 3D |
|---|---|---|
| Scene dirty | `TSceneHandle::setDirtyFlag` (xsheet edits, keys) | Exposing a pose cell already dirties the scene |
| Level dirty | `TXshSimpleLevel::setDirtyFlag` / properties | Set when a **pose** changes so Save All writes `*.t3dpose` |
| Palette dirty | ignore | 3D has no palette |

On pose edit: `sl->setDirtyFlag(true)` + `TUndo` that swaps old/new `Ref3dPose` for that fid. Follow `tnztools` undo pattern (`TUndoManager::manager()->add`). Do not invent a second undo stack.

---

## Import / Save As / collect

`SceneResources` + `ResourceImporter` copy simple-level files. For `.gltf` (not glb):

When copying `foo.gltf`, also copy URIs from tinygltf (`buffers`, `images`) and `foo.t3dpose`.

Put this in `Ref3dModel::copySidecarFiles(src, dst)` and call it from `TXshSimpleLevel::copyFiles` **only** if type is 3D (one `if`). Do not teach `copyFiles` about every image format.

Prefer telling users to use **.glb** (one file). Weak-PC friendly, fewer missing-texture bugs.

---

## Weak PC / laptop budget (sync + save)

| Moment | Cost we allow |
|---|---|
| Open scene | Parse .tnz (existing). Load each glb **once**. No GPU until first viewer paint. |
| First paint of a 3D column | Upload VBO once. CPU skin rest pose once. |
| Change xsheet row, **same fid** (hold) | **0** skin. Draw cached. |
| Change fid (new pose) | Skin once, cache. |
| Playback | Same as scrub. No animation clip sampler. |
| Column transform keys | Existing `getPlacement(row)` — already cheap. |
| Save scene | Write tiny `*.t3dpose` + existing .tnz. Never re-export glb. |
| Render / preview | 0 (preview eye off). |

Caps:

- Default mannequin: a few thousand tris.
- User model > ~50k tris: warn, draw wire AABB, do not skin.
- Max one 3D column “hot” (current) at full shade; others can stay last-cached mesh even if slightly stale during playback — optional later, not MVP.

No PBR, no shadows, no per-frame `glGet`, no rasterizing 3D to PNG.

---

## Files involved in save/key/sync (edit vs not)

| File | Role | 3D action |
|---|---|---|
| `toonzscene.cpp` `save` / `loadTnzFile` / `loadResources` | Scene XML + resource loop | Type from ext; **no new tnz tags** |
| `levelset.cpp` | Serializes levels | none |
| `txshsimplelevel.cpp` `saveData`/`loadData`/`load`/`save`/`copyFiles` | Level meta + files | Ext → type; **early-out load/save**; copy sidecar |
| `txshlevelcolumn.cpp` `saveData` | Cells | none (fid already saved) |
| `txsheet.cpp` `saveData` | columns + pegbars + fx | none |
| `tstageobject.cpp` | Transform keys | **do not add joints** |
| `sceneresources.cpp` `SceneLevel::save` | Disk save of levels | prefer no edit if `sl->save` handles 3D |
| `iocommand.cpp` `saveAll` | Orchestrates | none |
| `stage.cpp` `addCell` | Row → player | none if simple level |
| `stagevisitor.cpp` | Draw | OpenGL hook only |
| `xshcellviewer.cpp` `drawKeyframe` | Diamond keys | **do not overload** |

---

## MVP pose file (sidecar)

`mannequin.t3dpose` example (keep it dumb):

```
{
  "v": 1,
  "joints": ["Hips", "Spine", "LeftArm", ...],
  "frames": {
    "1": [0,0,0,  0,12,0,  ...],
    "2": [0,0,0,  0,40,0,  ...]
  }
}
```

- Euler degrees or packed quaternions — pick one and stick.
- Missing fid → rest pose.
- File is a few KB. Fine for cheap disks and git.

Register fids `1`, `2`, … in `m_frames` so the level strip and xsheet expose UI work without images.

---

## Checklist: “is it in sync?”

1. Change current frame in the xsheet → mannequin pose matches that cell’s fid.
2. Hold a cell 6 frames → pose does not re-skin, model does not drift.
3. Key the **column** X position → whole model moves; joints unchanged.
4. Save, reopen → same cells, same poses, same column keys.
5. Render movie → no 3D in output.
6. Open an old scene with no glb → identical.
