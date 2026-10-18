#pragma once

#ifndef REF3DDRAWER_INCLUDED
#define REF3DDRAWER_INCLUDED

#include "tcommon.h"
#include "tgeometry.h"

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class Ref3dModel;

namespace Stage {
class Player;
}

class DVAPI Ref3dDrawer {
public:
  static void draw(const Ref3dModel &model, const TAffine &viewAff,
                   const Stage::Player &player, bool camera3d);
};

#endif
