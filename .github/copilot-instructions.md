# Tahoma2D Copilot Instructions

Tahoma2D is a C++/Qt desktop animation application based on OpenToonz. The root `README.md` links to platform setup guides; use the matching `doc/how_to_build_*.md` for dependency and environment details.

## Architecture

- The CMake project is rooted at `toonz/sources/CMakeLists.txt`. It builds foundational libraries (`tnzcore`, `tnzbase`, `tnzext`), the scene/rendering library (`toonzlib`), image and FX modules (`image`, `stdfx`, `colorfx`, `sound`), Qt UI and tools (`toonzqt`, `tnztools`), then applications such as `toonz`.
- Scenes organize levels in Xsheet columns and connect effects into render graphs. The Stage turns the active scene/Xsheet into `Stage::Player` instances; visitors in `toonzlib/stagevisitor.cpp` dispatch those players to raster, Toonz, vector, or OpenGL drawing paths. `SceneViewer` selects viewer-specific painters. Rendering/export follows a separate FX and renderer path (`rendercommand.cpp`, `movierenderer.cpp`, `trenderer.cpp`); viewer-only content must not accidentally enter final output.
- FX computation is tile-based: `TRasterFx::compute()` coordinates cached resources and calls each effect's `doCompute()`. Render commands configure frame/output settings and hand work to the movie or multimedia renderer.
- Level support crosses file-type registration, image I/O declarations, scene level creation/loading, and simple-level behavior. When changing a level format, trace all of those paths rather than adding support at only the file-dialog or loader layer.
- For Ref3D-specific work, read `doc/3d_reference_layer/README.md` and `PHASES.md` first. Ref3D is a simple-level type without a `TImage`; keep it distinct from `MESH_XSHLEVEL`, and preserve the documented boundary that viewer drawing is not preview/render output.

## Build, test, and format

The checked-in build tree on Windows is `toonz/build`; use the configuration that matches the generated Visual Studio solution:

```powershell
cmake --build toonz\build --config RelWithDebInfo --parallel
cmake --build toonz\build --config RelWithDebInfo --target toonzlib --parallel
```

The first command builds the solution; the second is a focused incremental build for scene/rendering library changes. Substitute the affected CMake target when working elsewhere. For a fresh Windows setup, follow `doc/how_to_build_win.md` and the repository's `ci-scripts/windows/tahoma-build.bat` for the generator and dependency settings.

On Linux, the documented build flow is:

```sh
cmake -S toonz/sources -B toonz/build
cmake --build toonz/build --parallel
```

macOS setup and Xcode/command-line build instructions are in `doc/how_to_build_macosx.md`.

The application CMake project does not register an application unit-test suite with CTest, so there is no project single-test command to run. Some vendored third-party libraries define their own tests; do not treat those as Tahoma2D application tests. Validate behavior with a focused build and the relevant manual workflow; `doc/how_to_test_prs.md` describes testing PR builds.

Format C++ with the repository configuration at `toonz/sources/.clang-format`. Contribution instructions recommend running `beautification.bat` from `toonz/sources` on Windows or `beautification.sh` there on Unix-like systems; prefer formatting only the files changed when practical.

## Repository conventions

- CMake source/header lists are maintained in each component's `CMakeLists.txt`; add new implementation files to the owning target.
- Follow the checked-in `.clang-format` (Google-derived, C++17, no tabs). Keep changes within the owning library/module and use existing project types and dispatch paths.
- The contribution workflow is documented in `CONTRIBUTING.md`; platform-specific build requirements are documented rather than duplicated here.
- Root `AGENTS.md` and `CLAUDE.md` permit autonomous code generation when requested. `AI_POLICY.md` currently contains no additional policy text.
