#pragma once

#include "defs.h"
#include "math.h"
#include "dynarray.h"

struct PhyObject {
   Float2 pos;

   f32 invMass = 1.0f;

   f32 moveSpeedCap = 0.0f;       // updated per frame, interpolates toward moveSpeedCapTarget
   f32 moveSpeedCapTarget = 0.0f; // updated per frame, max speed based on length of move vector and velocity direction vs facing
   Float2 moveVector = { 0.0f, 0.0f };   // vector length 0-1 for movement amount/direction
   Float2 velocity = { 0.0f, 0.0f };      // actual delta pos per frame, moveVector*accel is added per frame, capped at moveSpeed

   Float2 faceVector = { 0.0f, 0.0f };  // unit vector for target facing, facing will interpolate toward this angle
   Float2 facing = { 1, 0 };  // unit vector for character facing  

   int island = -1;
   int islandMailbox = -1;

   union {
      struct {
         f32 size;
      } circle;
   };
};

void phyUpdateVelocity(PhyObject& p);
void phyUpdateRotation(PhyObject& p);
void updatePhyPositions(DynamicArray<PhyObject*>& objs);
