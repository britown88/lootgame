#pragma once

#include "phy.h"




struct PhyCollision {
   PhyObject* a = nullptr;
   PhyObject* b = nullptr;
   float time = 0.0f;
};


void getAllPhyCollisions(std::vector<PhyObject*>& objs, std::vector<PhyCollision>& collisions, float &timeRemaining) {

   auto oe = objs.end();
   for (auto i = objs.begin(); i != oe; ++i) {
      for (auto j = i + 1; j != oe; ++j) {
         auto& a = *i;
         auto& b = *j;

         auto combinedSize = a->circle.size + b->circle.size;
         float rangeCheck = //v2DistSquared(a->pos + a->velocity, b->pos + b->velocity);
            v2LenSquared({
            (a->pos.x + a->velocity.x) - (b->pos.x + b->velocity.x),
            (a->pos.y + a->velocity.y) - (b->pos.y + b->velocity.y)
               });

         if (rangeCheck < combinedSize*combinedSize) {
            float t = 0.0f;
            Float2 q = { 0,0 };

            auto avelscaled = a->velocity * timeRemaining;
            auto bvelscaled = b->velocity * timeRemaining;

            auto dirvec = avelscaled - bvelscaled;
            auto len = v2Len(dirvec);
            dirvec /= len;

            if (intersectRaySphere(a->pos, dirvec, b->pos, combinedSize, t, q) && t <= len && t > 0.00001f) {
               PhyCollision c = { a, b, (t / len) * timeRemaining };
               collisions.push_back(c);
            }
         }
      }
   }
}

struct IslandPartition {
   int mailbox = 0;
   std::vector<PhyObject*> objs;
};

struct IslandPartitionSet {
   int mailbox = 0;
   std::vector<IslandPartition> islands;

   int next() {
      int i = 0;
      for (auto && island : islands) {
         if (island.mailbox != mailbox) {
            island.mailbox = mailbox;
            island.objs.clear();
            return i;
         }
         else if (island.objs.empty()) {
            return i;
         }
         ++i;
      }

      islands.push_back({ mailbox,{} });
      return i;
   }
};

void partitionSetReset(IslandPartitionSet* pSet) {
   ++pSet->mailbox;
}

void partitionSetAddObjs(IslandPartitionSet* pSet, PhyObject*a, PhyObject* b, bool connected) {

   // if updating a for first time, add it to its own island
   if (a->islandMailbox != pSet->mailbox) {
      a->islandMailbox = pSet->mailbox;
      a->island = pSet->next();
      pSet->islands[a->island].objs.push_back(a);
   }

   if (connected) {

      // if updating b for first time and its connected, add it to a's island
      if (b->islandMailbox != pSet->mailbox) {
         b->islandMailbox = pSet->mailbox;
         b->island = a->island;
         pSet->islands[a->island].objs.push_back(b);
      }

      // if b is part of an island, merge them to a
      else if (a->island != b->island) {
         int from = 0, to = 0;
         if (pSet->islands[a->island].objs.size() > pSet->islands[b->island].objs.size()) {
            from = b->island;
            to = a->island;
         }
         else {
            from = a->island;
            to = b->island;
         }

         for (auto&& obj : pSet->islands[from].objs) {
            obj->island = to;
         }
         pSet->islands[to].objs.insert(pSet->islands[to].objs.begin(), pSet->islands[from].objs.begin(), pSet->islands[from].objs.end());
         pSet->islands[from].objs.clear();
      }
   }

   // if updating b for first time and not connected, add to its own island
   else if (b->islandMailbox != pSet->mailbox) {
      b->islandMailbox = pSet->mailbox;
      b->island = pSet->next();
      pSet->islands[b->island].objs.push_back(b);
   }
}

void resolveOverlaps(std::vector<PhyObject*>& objs, IslandPartitionSet* pSet) {
   bool overlap = false;
   int attemptsToResolve = 0;

   partitionSetReset(pSet);

   if (objs.size() == 1) {
      auto&& a = objs[0];
      a->islandMailbox = pSet->mailbox;
      a->island = pSet->next();
      pSet->islands[a->island].objs.push_back(a);
      return;
   }

   do {
      overlap = false;

      auto oe = objs.end();
      for (auto i = objs.begin(); i != oe; ++i) {
         for (auto j = i + 1; j != oe; ++j) {
            auto& a = *i;
            auto& b = *j;

            auto combinedSize = a->circle.size + b->circle.size;
            auto connectRange = combinedSize + (a->maxSpeed + b->maxSpeed);
            float distanceSquared = v2LenSquared({ a->pos.x - b->pos.x, a->pos.y - b->pos.y });

            bool connected = distanceSquared < connectRange * connectRange;

            if (!attemptsToResolve) {
               partitionSetAddObjs(pSet, a, b, connected);
            }

            if (connected) {

               if (distanceSquared < combinedSize * combinedSize) {
                  auto dist = sqrtf(distanceSquared);

                  auto overlap = combinedSize - dist;
                  auto impulse = v2Normalized(b->pos - a->pos) * (overlap + 0.0001f) / (a->invMass + b->invMass);

                  a->pos -= impulse * a->invMass;
                  b->pos += impulse * b->invMass;
                  overlap = true;
               }
            }
         }
      }

   } while (overlap && attemptsToResolve++ < 32);
}

void applyCollisionImpulse(Float2 pos1, Float2 pos2, Float2& vel1, Float2& vel2, float invMass1, float invMass2, float restitution)
{
   Float2 direction = v2Normalized(pos2 - pos1);
   Float2 relativeVelocity = vel2 - vel1;

   float d = v2Dot(direction, relativeVelocity);
   if (d > 0.0f) //moving away from each other?  nothing to do?
      return;

   float moveAmount = -(1 + restitution) * d / (invMass1 + invMass2);

   Float2 impulse = direction * moveAmount;

   vel1 -= impulse * invMass1;
   vel2 += impulse * invMass2;
}


void updatePhyPositions(std::vector<PhyObject*>& objs) {

   static IslandPartitionSet pSet;
   std::vector<PhyCollision> collisions;

   resolveOverlaps(objs, &pSet);

   for (auto&& island : pSet.islands) {
      if (island.mailbox != pSet.mailbox) {
         continue;

      }

      float timeRemaining = 1.0f;
      int attemptsToResolve = 0;

      while (timeRemaining > 0.0f && attemptsToResolve < 32) {
         collisions.clear();
         getAllPhyCollisions(island.objs, collisions, timeRemaining);

         auto cCount = collisions.size();

         if (!cCount) {
            for (auto&& o : island.objs) {
               o->pos += o->velocity * timeRemaining;
            }

            timeRemaining = 0.0f;
            break; //...yatta
         }

         // get soonest collision
         PhyCollision c = { nullptr, nullptr, FLT_MAX };
         if (cCount == 1) {
            c = collisions[0];
         }
         else {
            for (auto&& dc : collisions) {
               if (dc.time < c.time) {
                  c = dc;
               }
            }
         }

         auto&a = *c.a;
         auto&b = *c.b;

         for (auto&& d : island.objs) {
            d->pos += d->velocity * c.time;
         }

         applyCollisionImpulse(a.pos, b.pos, a.velocity, b.velocity, a.invMass, b.invMass, 0.01f);

         timeRemaining -= c.time;
         ++attemptsToResolve;

         if (attemptsToResolve == 32) {
            break;
         }
      }
   }



}


