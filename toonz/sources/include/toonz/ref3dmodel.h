#pragma once

#ifndef REF3DMODEL_INCLUDED
#define REF3DMODEL_INCLUDED

#include "tcommon.h"
#include "tfilepath.h"
#include "tgeometry.h"

#include <memory>
#include <string>
#include <vector>

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//! CPU-side glTF 2.0 model: rest-pose mesh + skeleton.
//! No OpenGL, no textures, no animation-clip playback.
class DVAPI Ref3dModel {
public:
  struct Joint {
    std::string name;
    int parent;  //!< Index in joints[], or -1
    int node;    //!< glTF node index
    double inverseBind[16];
    double restLocal[16];
  };

  struct Vertex {
    float px, py, pz;
    float nx, ny, nz;
    unsigned short joints[4];
    float weights[4];
  };

  Ref3dModel();
  ~Ref3dModel();

  //! Returns false on failure. Does not throw.
  bool load(const TFilePath &fp, std::string *err = 0);

  int skinCount() const { return m_skinCount; }
  int jointCount() const { return (int)m_joints.size(); }
  int vertexCount() const { return (int)m_vertices.size(); }
  int triangleCount() const { return (int)m_indices.size() / 3; }

  const std::vector<Joint> &joints() const { return m_joints; }
  const std::vector<Vertex> &vertices() const { return m_vertices; }
  const std::vector<unsigned int> &indices() const { return m_indices; }
  const TFilePath &path() const { return m_path; }

  static bool isRef3dExtension(const std::string &ext);
  static std::shared_ptr<Ref3dModel> loadCached(const TFilePath &fp,
                                                std::string *err = 0);
  static void dropCached(const TFilePath &fp);

  //! Load glTF into a simple level: one empty fid, no TImage, no throw.
  static bool loadIntoLevel(class TXshSimpleLevel *sl, const TFilePath &fp,
                            std::string *err = 0);

private:
  TFilePath m_path;
  int m_skinCount;
  std::vector<Joint> m_joints;
  std::vector<Vertex> m_vertices;
  std::vector<unsigned int> m_indices;
};

#endif
