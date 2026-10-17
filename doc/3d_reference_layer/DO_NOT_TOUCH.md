# Do not touch (corruption risk)

Short list. Details in `FILEMAP.md`.

## Never reuse

- `MESH_XSHLEVEL` / `TXshMeshColumn` / Plastic Tool — 2D mesh deform, not glTF
- SceneViewer **3D camera mode** (`m_theta3D`, `m_phi3D`, `m_draw3DMode`) — 2D papers in perspective
- `Skeleton` / `IKSkeleton` / `T_Skeleton` — pegbar IK
- `TMeshImage` / `.mesh` I/O

## Never rewrite

- `TXshSimpleLevel` (class is `final`; do not un-final it)
- Brush, fill, vector, plastic tools (only a **disable** check in `tool.cpp`)
- `SceneViewer::paintGL` / `drawCameraStand` / `m_draw3DMode`
- `IconGenerator` raster/vector renderers (only a `case REF3D` that returns a static pixmap)
- `scenefx.cpp`, `movierenderer.cpp`, `tcomposer` (preview-eye already excludes guides)
- `TXshSimpleLevel::saveData` / `loadData` **tag list** — unknown tags **throw** and old Tahoma cannot open the scene
- Fx DAG / `TLevelColumnFx` behavior for all columns

## Never do in MVP

- New `TXshColumn` subclass
- Fake `TImage` per pose
- Assimp
- PBR / animation clip playback / IK
- Rasterizing 3D into a PNG every frame
- Changing how raster/vector levels load or save
