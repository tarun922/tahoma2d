#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_NO_EXTERNAL_IMAGE

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4018 4100 4189 4244 4267 4996)
#endif

#include "tiny_gltf.h"

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include "toonz/ref3dmodel.h"
#include "toonz/tlog.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/levelproperties.h"
#include "timage.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <cstring>
#include <map>
#include <mutex>
#include <cmath>

namespace {

std::mutex g_cacheMutex;
std::map<QString, std::shared_ptr<Ref3dModel>> g_cache;

void identity4(double *m) {
  for (int i = 0; i < 16; ++i) m[i] = 0.0;
  m[0] = m[5] = m[10] = m[15] = 1.0;
}

void mul4(double *out, const double *a, const double *b) {
  double t[16];
  for (int c = 0; c < 4; ++c)
    for (int r = 0; r < 4; ++r)
      t[c * 4 + r] = a[0 * 4 + r] * b[c * 4 + 0] + a[1 * 4 + r] * b[c * 4 + 1] +
                     a[2 * 4 + r] * b[c * 4 + 2] + a[3 * 4 + r] * b[c * 4 + 3];
  for (int i = 0; i < 16; ++i) out[i] = t[i];
}

void nodeLocalMatrix(const tinygltf::Node &n, double *m) {
  if (n.matrix.size() == 16) {
    for (int i = 0; i < 16; ++i) m[i] = n.matrix[i];
    return;
  }
  double T[16], R[16], S[16];
  identity4(T);
  identity4(R);
  identity4(S);
  if (n.translation.size() == 3) {
    T[12] = n.translation[0];
    T[13] = n.translation[1];
    T[14] = n.translation[2];
  }
  if (n.scale.size() == 3) {
    S[0]  = n.scale[0];
    S[5]  = n.scale[1];
    S[10] = n.scale[2];
  }
  if (n.rotation.size() == 4) {
    const double x = n.rotation[0], y = n.rotation[1], z = n.rotation[2],
                 w = n.rotation[3];
    const double xx = x * x, yy = y * y, zz = z * z;
    const double xy = x * y, xz = x * z, yz = y * z;
    const double wx = w * x, wy = w * y, wz = w * z;
    R[0]  = 1.0 - 2.0 * (yy + zz);
    R[1]  = 2.0 * (xy + wz);
    R[2]  = 2.0 * (xz - wy);
    R[4]  = 2.0 * (xy - wz);
    R[5]  = 1.0 - 2.0 * (xx + zz);
    R[6]  = 2.0 * (yz + wx);
    R[8]  = 2.0 * (xz + wy);
    R[9]  = 2.0 * (yz - wx);
    R[10] = 1.0 - 2.0 * (xx + yy);
  }
  double RS[16];
  mul4(RS, R, S);
  mul4(m, T, RS);
}

const unsigned char *accessorBytes(const tinygltf::Model &model,
                                   const tinygltf::Accessor &acc,
                                   size_t &stride, size_t &count) {
  if (acc.bufferView < 0 ||
      acc.bufferView >= (int)model.bufferViews.size())
    return 0;
  const tinygltf::BufferView &bv = model.bufferViews[acc.bufferView];
  if (bv.buffer < 0 || bv.buffer >= (int)model.buffers.size()) return 0;
  const tinygltf::Buffer &buf = model.buffers[bv.buffer];
  const size_t offset         = bv.byteOffset + acc.byteOffset;
  if (offset >= buf.data.size()) return 0;
  count  = acc.count;
  stride = acc.ByteStride(bv);
  if (stride == 0) return 0;
  return buf.data.data() + offset;
}

template <typename T>
T readLE(const unsigned char *p) {
  T v;
  memcpy(&v, p, sizeof(T));
  return v;
}

bool readFloat3(const tinygltf::Model &model, int accessor, size_t i, float *xyz) {
  if (accessor < 0) return false;
  const tinygltf::Accessor &acc = model.accessors[accessor];
  size_t stride = 0, count = 0;
  const unsigned char *base = accessorBytes(model, acc, stride, count);
  if (!base || i >= count) return false;
  const unsigned char *p = base + i * stride;
  if (acc.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT) {
    xyz[0] = readLE<float>(p);
    xyz[1] = readLE<float>(p + 4);
    xyz[2] = readLE<float>(p + 8);
    return true;
  }
  return false;
}

bool readVec4U16(const tinygltf::Model &model, int accessor, size_t i,
                 unsigned short *out) {
  if (accessor < 0) return false;
  const tinygltf::Accessor &acc = model.accessors[accessor];
  size_t stride = 0, count = 0;
  const unsigned char *base = accessorBytes(model, acc, stride, count);
  if (!base || i >= count) return false;
  const unsigned char *p = base + i * stride;
  if (acc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
    for (int k = 0; k < 4; ++k) out[k] = readLE<unsigned short>(p + k * 2);
    return true;
  }
  if (acc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
    for (int k = 0; k < 4; ++k) out[k] = p[k];
    return true;
  }
  return false;
}

bool readVec4F(const tinygltf::Model &model, int accessor, size_t i, float *out) {
  if (accessor < 0) return false;
  const tinygltf::Accessor &acc = model.accessors[accessor];
  size_t stride = 0, count = 0;
  const unsigned char *base = accessorBytes(model, acc, stride, count);
  if (!base || i >= count) return false;
  const unsigned char *p = base + i * stride;
  if (acc.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT) {
    for (int k = 0; k < 4; ++k) out[k] = readLE<float>(p + k * 4);
    return true;
  }
  return false;
}

bool readIndex(const tinygltf::Model &model, const tinygltf::Accessor &acc,
               size_t i, unsigned int &out) {
  size_t stride = 0, count = 0;
  const unsigned char *base = accessorBytes(model, acc, stride, count);
  if (!base || i >= count) return false;
  const unsigned char *p = base + i * stride;
  if (acc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
    out = readLE<unsigned int>(p);
  else if (acc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
    out = readLE<unsigned short>(p);
  else if (acc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
    out = *p;
  else
    return false;
  return true;
}

void appendPrimitive(const tinygltf::Model &model,
                     const tinygltf::Primitive &prim,
                     std::vector<Ref3dModel::Vertex> &verts,
                     std::vector<unsigned int> &indices) {
  auto itPos = prim.attributes.find("POSITION");
  if (itPos == prim.attributes.end()) return;
  const int posAcc = itPos->second;
  if (posAcc < 0 || posAcc >= (int)model.accessors.size()) return;
  const size_t n = model.accessors[posAcc].count;

  int nrmAcc = -1, jntAcc = -1, wgtAcc = -1;
  auto itN = prim.attributes.find("NORMAL");
  if (itN != prim.attributes.end()) nrmAcc = itN->second;
  auto itJ = prim.attributes.find("JOINTS_0");
  if (itJ != prim.attributes.end()) jntAcc = itJ->second;
  auto itW = prim.attributes.find("WEIGHTS_0");
  if (itW != prim.attributes.end()) wgtAcc = itW->second;

  const unsigned int base = (unsigned int)verts.size();
  verts.reserve(verts.size() + n);
  for (size_t i = 0; i < n; ++i) {
    Ref3dModel::Vertex v;
    memset(&v, 0, sizeof(v));
    float p[3] = {0, 0, 0};
    readFloat3(model, posAcc, i, p);
    v.px = p[0];
    v.py = p[1];
    v.pz = p[2];
    float nn[3] = {0, 1, 0};
    if (nrmAcc >= 0) readFloat3(model, nrmAcc, i, nn);
    v.nx = nn[0];
    v.ny = nn[1];
    v.nz = nn[2];
    v.joints[0] = v.joints[1] = v.joints[2] = v.joints[3] = 0;
    v.weights[0]                                          = 1.f;
    v.weights[1] = v.weights[2] = v.weights[3] = 0.f;
    if (jntAcc >= 0) readVec4U16(model, jntAcc, i, v.joints);
    if (wgtAcc >= 0) readVec4F(model, wgtAcc, i, v.weights);
    verts.push_back(v);
  }

  std::vector<unsigned int> primIdx;
  if (prim.indices >= 0 && prim.indices < (int)model.accessors.size()) {
    const tinygltf::Accessor &acc = model.accessors[prim.indices];
    primIdx.resize(acc.count);
    for (size_t i = 0; i < acc.count; ++i) {
      unsigned int id = 0;
      if (!readIndex(model, acc, i, id)) id = 0;
      primIdx[i] = base + id;
    }
  } else {
    primIdx.resize(n);
    for (size_t i = 0; i < n; ++i) primIdx[i] = base + (unsigned int)i;
  }

  const int mode =
      (prim.mode < 0) ? TINYGLTF_MODE_TRIANGLES : prim.mode;
  if (mode == TINYGLTF_MODE_TRIANGLES) {
    const size_t tcount = (primIdx.size() / 3) * 3;
    indices.insert(indices.end(), primIdx.begin(), primIdx.begin() + tcount);
  } else if (mode == TINYGLTF_MODE_TRIANGLE_STRIP && primIdx.size() >= 3) {
    for (size_t i = 0; i + 2 < primIdx.size(); ++i) {
      if (i & 1) {
        indices.push_back(primIdx[i]);
        indices.push_back(primIdx[i + 2]);
        indices.push_back(primIdx[i + 1]);
      } else {
        indices.push_back(primIdx[i]);
        indices.push_back(primIdx[i + 1]);
        indices.push_back(primIdx[i + 2]);
      }
    }
  } else if (mode == TINYGLTF_MODE_TRIANGLE_FAN && primIdx.size() >= 3) {
    for (size_t i = 1; i + 1 < primIdx.size(); ++i) {
      indices.push_back(primIdx[0]);
      indices.push_back(primIdx[i]);
      indices.push_back(primIdx[i + 1]);
    }
  }
}

bool skipImage(tinygltf::Image *image, const int, std::string *, std::string *,
               int, int, const unsigned char *, int, void *) {
  image->width      = 1;
  image->height     = 1;
  image->component  = 4;
  image->bits       = 8;
  image->pixel_type = TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE;
  image->image.assign(4, 0);
  return true;
}

}  // namespace

//-----------------------------------------------------------------------------

Ref3dModel::Ref3dModel() : m_skinCount(0) {}

Ref3dModel::~Ref3dModel() {}

bool Ref3dModel::isRef3dExtension(const std::string &ext) {
  return ext == "glb" || ext == "gltf" || ext == "GLB" || ext == "GLTF";
}

bool Ref3dModel::load(const TFilePath &fp, std::string *err) {
  m_path = fp;
  m_skinCount = 0;
  m_joints.clear();
  m_vertices.clear();
  m_indices.clear();

  const QString qpath = fp.getQString();
  QFile file(qpath);
  if (!file.open(QIODevice::ReadOnly)) {
    if (err) *err = "Cannot open " + qpath.toStdString();
    return false;
  }
  const QByteArray bytes = file.readAll();
  file.close();
  if (bytes.isEmpty()) {
    if (err) *err = "Empty file " + qpath.toStdString();
    return false;
  }

  tinygltf::TinyGLTF loader;
  loader.SetImageLoader(skipImage, 0);
  tinygltf::Model model;
  std::string loadErr, loadWarn;
  const std::string ext  = fp.getType();
  const QString baseDir  = QFileInfo(qpath).absolutePath();
  const std::string base = baseDir.toUtf8().constData();
  bool ok                = false;
  if (ext == "glb" || ext == "GLB") {
    ok = loader.LoadBinaryFromMemory(
        &model, &loadErr, &loadWarn,
        reinterpret_cast<const unsigned char *>(bytes.constData()),
        (unsigned int)bytes.size(), base);
  } else {
    const std::string json = std::string(bytes.constData(), bytes.size());
    ok = loader.LoadASCIIFromString(&model, &loadErr, &loadWarn, json.c_str(),
                                    (unsigned int)json.size(), base);
  }
  if (!ok) {
    if (err) *err = loadErr.empty() ? "tinygltf failed" : loadErr;
    return false;
  }

  m_skinCount = (int)model.skins.size();

  // Prefer the first skin; still keep unskinned meshes if there is no skin.
  std::map<int, int> nodeToJoint;
  if (!model.skins.empty()) {
    const tinygltf::Skin &skin = model.skins[0];
    m_joints.resize(skin.joints.size());
    std::vector<double> ibm;
    if (skin.inverseBindMatrices >= 0 &&
        skin.inverseBindMatrices < (int)model.accessors.size()) {
      const tinygltf::Accessor &acc =
          model.accessors[skin.inverseBindMatrices];
      size_t stride = 0, count = 0;
      const unsigned char *basePtr = accessorBytes(model, acc, stride, count);
      ibm.resize(count * 16, 0.0);
      if (basePtr && acc.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT) {
        for (size_t i = 0; i < count; ++i) {
          const unsigned char *p = basePtr + i * stride;
          for (int k = 0; k < 16; ++k)
            ibm[i * 16 + k] = readLE<float>(p + k * 4);
        }
      }
    }
    for (size_t j = 0; j < skin.joints.size(); ++j) {
      Joint jt;
      jt.node   = skin.joints[j];
      jt.parent = -1;
      identity4(jt.restLocal);
      identity4(jt.inverseBind);
      if (j * 16 + 15 < ibm.size())
        for (int k = 0; k < 16; ++k) jt.inverseBind[k] = ibm[j * 16 + k];
      if (jt.node >= 0 && jt.node < (int)model.nodes.size()) {
        jt.name = model.nodes[jt.node].name;
        nodeLocalMatrix(model.nodes[jt.node], jt.restLocal);
      }
      nodeToJoint[jt.node] = (int)j;
      m_joints[j]          = jt;
    }
    for (size_t j = 0; j < m_joints.size(); ++j) {
      const int node = m_joints[j].node;
      if (node < 0 || node >= (int)model.nodes.size()) continue;
      const std::vector<int> &ch = model.nodes[node].children;
      for (int c : ch) {
        auto it = nodeToJoint.find(c);
        if (it != nodeToJoint.end()) m_joints[it->second].parent = (int)j;
      }
    }
  }

  for (const tinygltf::Mesh &mesh : model.meshes)
    for (const tinygltf::Primitive &prim : mesh.primitives)
      appendPrimitive(model, prim, m_vertices, m_indices);

  {
    std::string msg = "Ref3dModel: " + qpath.toStdString() + " skins=" +
                      std::to_string(m_skinCount) + " joints=" +
                      std::to_string(jointCount()) + " vertices=" +
                      std::to_string(vertexCount()) + " triangles=" +
                      std::to_string(triangleCount());
    TSysLog::info(msg);
  }
  return true;
}

std::shared_ptr<Ref3dModel> Ref3dModel::loadCached(const TFilePath &fp,
                                                   std::string *err) {
  const QString key = QFileInfo(fp.getQString()).absoluteFilePath();
  {
    std::lock_guard<std::mutex> lock(g_cacheMutex);
    auto it = g_cache.find(key);
    if (it != g_cache.end()) return it->second;
  }
  auto model = std::make_shared<Ref3dModel>();
  if (!model->load(fp, err)) return std::shared_ptr<Ref3dModel>();
  std::lock_guard<std::mutex> lock(g_cacheMutex);
  g_cache[key] = model;
  return model;
}

void Ref3dModel::dropCached(const TFilePath &fp) {
  const QString key = QFileInfo(fp.getQString()).absoluteFilePath();
  std::lock_guard<std::mutex> lock(g_cacheMutex);
  g_cache.erase(key);
}
