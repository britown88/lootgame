#pragma once

#include "defs.h"
#include "math.h"
#include "dynarray.h"

struct PhyObject {
   Float2 pos = { 0.0f, 0.0f };
   Float2 velocity = { 0.0f, 0.0f };

   f32 invMass = 1.0f;
   f32 maxSpeed = 1.0f;

   // intrusive partitioning
   int island = -1;
   int islandMailbox = -1;

   union {
      struct {
         f32 size;
      } circle;
   };
};


void updatePhyPositions(DynamicArray<PhyObject*>& objs);