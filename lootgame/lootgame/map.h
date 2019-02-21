#pragma once
#include "defs.h"
#include "math.h"

typedef struct Map Map;

Map* mapCreate();

struct PointLight {
   ColorRGBAf color;
   Float2 pos;
   float size;

   // linear portion, smoothing factor, intensity
   Float3 attrs;
};
