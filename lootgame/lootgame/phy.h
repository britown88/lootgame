#pragma once

#include "defs.h"
#include "math.h"



struct PhyObject {
   Float2 pos = { 0.0f, 0.0f };
   Float2 velocity = { 0.0f, 0.0f };

   float invMass = 1.0f;
   float maxSpeed = 1.0f;

   // intrusive partitioning
   int island = -1;
   int islandMailbox = -1;

   enum PhyType {
      PhyType_Circle = 0,
      PhyType_Segment      
   };

   PhyType type = PhyType_Circle;

   struct {
      Float2 b = { 0,0 };
   } segment;

   struct {
      float size = 0;
   } circle;
};


void updatePhyPositions(std::vector<PhyObject*>& objs);
