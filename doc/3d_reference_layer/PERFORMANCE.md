# Performance constraints (MVP)

The 3D layer is a **guide**. It must stay cheaper than drawing a raster onion skin.

## Budget (viewer, one 3D column)

| Step | Budget |
|---|---|
| Load model | Once, on level load / reload. Never per frame. |
| GPU upload | Once. VBO of rest-pose positions + indices + (optional) normals. |
| Pose change | CPU: joint matrices + skin **or** GPU: dual-quat / matrix palette. Cache until the pose or frame changes. |
| Idle playback / scrub with **same** pose | **Zero** CPU skin. Draw cached mesh. |
| Draw | 1 draw call, no textures in MVP, no lights (or 1 directional in view space). |
| Render / preview / tcomposer | **0**. Preview eye off. |

If a model is > **50k triangles**, refuse or show a warning and draw wire AABB only. Artists can retopo in another app.

## What not to implement in MVP (expensive)

- PBR, IBL, shadows, SSAO
- Morph targets
- Playing glTF animation clips every viewer frame (store **posed joints** instead)
- Per-pixel outline full-screen passes
- Assimp post-process (JoinVertices, triangulate everything at runtime every load is OK once; do not triangulate every frame)
- Qt3D / extra scene graph
- Rasterizing the 3D view into a `TRasterImage` every frame to composite as 2D (that doubles cost and hits ImageManager)

## Skinning

MVP FK:

```
jointWorld[i] = parentWorld * localTRS(restBind, poseRotation[i])
skinned = jointWorld[i] * inverseBind[i] * restPosition
```

Do this on CPU for mannequins (<10k verts). Simpler, no custom shaders, works with Tahoma’s old fixed-function OpenGL path (`tgl.h` / `OpenGlPainter`).

If CPU skin shows up in a profiler, move to a tiny GLSL 120 shader in Phase 2 — **do not start there**.

## Viewer integration cost

`SceneViewer` already redraws the whole stage. Extra cost is one more Player in `OpenGlPainter`. Keep `Ref3dDrawer::draw` free of:

- `glGet*` (sync)
- shader compile
- file IO
- `QImage` conversion
- allocating vectors each call (reuse scratch buffers on the model object)

## Playback / xsheet sync (weak PCs)

Do **not** interpolate joints every frame. Tahoma already interpolates column X/Y/Angle via `TStageObject`. Joints are **held cels**:

- Same `TFrameId` on many rows → reuse last skinned VBO (0 CPU).
- New fid → skin once, cache keyed by `(level*, fid, poseRevision)`.
- Playback is the same as scrubbing. No glTF animation sampler, no `QTimeLine`.

Putting each joint on `TDoubleParam` would evaluate 50–200 curves per viewer frame and fill the Function Editor. That is the heavy path; skip it.

## Threading

Load glTF on the **main thread** in MVP (same as other levels). Do not invent a worker until a 20MB+ GLB is common. ImageManager / GL contexts in Tahoma are easy to deadlock.

## Memory

Keep one `Ref3dModel` per unique path (level). Poses are ~ `jointCount * 3 floats * usedFrames` — tiny.

Do not duplicate the mesh per xsheet cell.
