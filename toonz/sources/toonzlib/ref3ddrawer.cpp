#include "toonz/ref3ddrawer.h"

#include "toonz/ref3dmodel.h"
#include "toonz/stage.h"
#include "toonz/stageplayer.h"

#include "tgl.h"

#include <algorithm>
#include <cmath>
#include <vector>

void Ref3dDrawer::draw(const Ref3dModel &model, const TAffine &viewAff,
                       const Stage::Player &player, bool camera3d) {
  const std::vector<Ref3dModel::Vertex> &vertices = model.vertices();
  const std::vector<unsigned int> &indices       = model.indices();
  if (vertices.empty() || indices.empty()) return;

  const Ref3dModel::Bounds &bounds = model.bounds();
  const double centerX = (bounds.min[0] + bounds.max[0]) * 0.5;
  const double centerY = (bounds.min[1] + bounds.max[1]) * 0.5;
  const double centerZ = (bounds.min[2] + bounds.max[2]) * 0.5;
  const double width   = bounds.max[0] - bounds.min[0];
  const double height  = bounds.max[1] - bounds.min[1];
  const double depth   = bounds.max[2] - bounds.min[2];
  const double extent =
      camera3d ? std::max(width, std::max(height, depth))
               : std::max(width, height);
  if (!std::isfinite(extent) || extent <= 0.0) return;

  // Fit the model into a five-inch guide area while preserving column scale.
  const double modelScale = Stage::inch * 5.0 / extent;
  const TAffine modelToStage =
      player.m_placement * TScale(modelScale) *
      TTranslation(-centerX, -centerY);

  glPushAttrib(GL_ALL_ATTRIB_BITS);
  glPushMatrix();
  if (camera3d) glTranslated(0, 0, player.m_z);

  tglMultMatrix(viewAff * modelToStage);
  glScaled(1.0, 1.0, camera3d ? modelScale : 0.0);
  if (camera3d) glTranslated(0, 0, -centerZ);

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_CULL_FACE);
  glColor4ub(112, 176, 216, player.m_opacity);
  if (camera3d) {
    const GLfloat lightPosition[] = {-0.4f, 0.8f, 1.0f, 0.0f};
    const GLfloat lightAmbient[]  = {0.35f, 0.35f, 0.35f, 1.0f};
    const GLfloat lightDiffuse[]  = {0.75f, 0.75f, 0.75f, 1.0f};
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);
    glEnable(GL_NORMALIZE);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glLightfv(GL_LIGHT0, GL_POSITION, lightPosition);
    glLightfv(GL_LIGHT0, GL_AMBIENT, lightAmbient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, lightDiffuse);
    glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);
  } else {
    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
    glLineWidth(1.5f);
  }
  for (int pass = 0; pass < (camera3d ? 1 : 2); ++pass) {
    const bool wireframe = !camera3d && pass == 1;
    glColor4ub(112, 176, 216,
               (camera3d || wireframe) ? player.m_opacity
                                       : player.m_opacity / 5);
    if (wireframe) glLineWidth(1.5f);
    glBegin(wireframe ? GL_LINES : GL_TRIANGLES);
    for (size_t i = 0; i + 2 < indices.size(); i += 3) {
      const unsigned int ia = indices[i];
      const unsigned int ib = indices[i + 1];
      const unsigned int ic = indices[i + 2];
      if (ia >= vertices.size() || ib >= vertices.size() ||
          ic >= vertices.size())
        continue;

      const Ref3dModel::Vertex *triangle[] = {&vertices[ia], &vertices[ib],
                                              &vertices[ic]};
      static const int triangleOrder[] = {0, 1, 2};
      static const int wireOrder[]     = {0, 1, 1, 2, 2, 0};
      const int *order = wireframe ? wireOrder : triangleOrder;
      const int count = wireframe ? 6 : 3;
      for (int j = 0; j < count; ++j) {
        const Ref3dModel::Vertex &vertex = *triangle[order[j]];
        if (camera3d) glNormal3f(vertex.nx, vertex.ny, vertex.nz);
        glVertex3f(vertex.px, vertex.py, camera3d ? vertex.pz : 0.0f);
      }
    }
    glEnd();
  }

  glPopMatrix();
  glPopAttrib();
}
