#include "stdafx.h"

#include "phy.h"
#include "coords.h"

static const int ResolveAttemptLimit = 32;

enum CollisionType {
   CollisionType_CircleCircle = 0,
   CollisionType_CircleSegment,
};

struct PhyCollision {
   PhyObject* a = nullptr;
   PhyObject* b = nullptr;
   float time = 0.0f;

   CollisionType type;
   Float2 collisionNormal;
};



static void _detect_CircleCircle(PhyObject*a, PhyObject*b, Array<PhyCollision>& collisions, float &timeRemaining) {   
   // first an early-out test to see if both positions plus both velocities can fall within their combioned radii
   auto combinedSize = a->circle.size + b->circle.size;
   float rangeCheck = //v2DistSquared(a->pos + a->velocity, b->pos + b->velocity);
      v2LenSquared({
      (a->pos.x + a->velocity.x) - (b->pos.x + b->velocity.x),
      (a->pos.y + a->velocity.y) - (b->pos.y + b->velocity.y)
         });

   if (rangeCheck < combinedSize*combinedSize) {
      float t;
      Float2 q = { 0,0 };

      auto avelscaled = a->velocity * timeRemaining;
      auto bvelscaled = b->velocity * timeRemaining;

      auto dirvec = avelscaled - bvelscaled;
      auto dirlen = v2Len(dirvec);
      auto dirnorm = dirvec / dirlen;

      //now we determine the time t in which a ray from obj a toward their combined velocities intersects b
      if (intersectRaySphere(a->pos, dirnorm, b->pos, combinedSize, t, q) && t <= dirlen) {
         //auto distCheck = v2Len((b->pos) - (a->pos));

         static bool errTripped = false;
         if (t == 0.0f) {
            //if (!errTripped) {
               //ERR("IntersectRaySphere dist:%0.7f", distCheck);
            //}            
            //errTripped = true;
         }
         else {

            //errTripped = false;
            //WARN("IntersectRaySphere dist:%0.7f", distCheck);
            //if (fabs(a->velocity.x) < 0.0001f) { a->velocity.x = 0.0f; }
            //if (fabs(a->velocity.y) < 0.0001f) { a->velocity.y = 0.0f; }
            //if (fabs(b->velocity.x) < 0.0001f) { b->velocity.x = 0.0f; }
            //if (fabs(b->velocity.y) < 0.0001f) { b->velocity.y = 0.0f; }

            auto dt = (t / dirlen) * timeRemaining;
            PhyCollision c = { 
               a, b, dt,
               CollisionType_CircleCircle,
               v2Normalized(b->pos  - a->pos) };

            //LOG("Collision detected t:%0.7f", c.time);
            collisions.push_back(c);
         }
         //if (t == 12.0f) {
         //   t = 0;
         //}
         
      }
   }
}

static void _detect_CircleSegment(PhyObject*a, PhyObject*b, Array<PhyCollision>& collisions, float &timeRemaining) {
   float collisionRange = a->circle.size;
   auto pv = a->velocity * timeRemaining;
   auto qv = b->velocity * timeRemaining;

   Float2 p1 = a->pos;
   Float2 p2Check = { p1.x + pv.x, p1.y + pv.y };

   Float2 q1 = b->pos;
   Float2 q2 = b->segment.b;
   
   // early out check from aabb around q
   bool aabbCheck =
      (p2Check.x > MIN(q1.x + qv.x, q2.x + qv.x) - collisionRange) &&
      (p2Check.y > MIN(q1.y + qv.y, q2.y + qv.y) - collisionRange) &&
      (p2Check.x <= MAX(q1.x + qv.x, q2.x + qv.x) + collisionRange) &&
      (p2Check.y <= MAX(q1.y + qv.y, q2.y + qv.y) + collisionRange);

   if (!aabbCheck) {
      return;
   }

   // q is our segment, p is our circle
   auto qdiff = q2 - q1;
   auto qlen = v2Len(qdiff);
   auto qdir = qdiff / qlen;

   auto v = pv - qv;
   auto vlen = v2Len(v);
   auto vdir = v / vlen;

   float t; Float2 c = { 0,0 };

   float shortest = FLT_MAX;
   PhyCollision col = { a, b, 0.0f, CollisionType_CircleSegment };
   bool collision = false;

   if (intersectRaySphere(p1, vdir, q1, collisionRange, t, c) && t <= vlen && t > 0.0f) {
      shortest = t;
      collision = true;
      col.time = (t / vlen);
      col.collisionNormal = v2Normalized(q1 - p1);
   }

   if (intersectRaySphere(p1, vdir, q2, collisionRange, t, c) && t <= vlen && t > 0.0f) {
      if (t < shortest) {
         shortest = t;
         collision = true;
         col.time = (t / vlen);
         col.collisionNormal = v2Normalized(q2 - p1);
      }
   }

   auto qconj = v2Conjugate(qdir);
   // get p origin by rotating it around q1 by qconj
   auto rp1 = v2Rotate(p1 - q1, qconj);
   // just rotate the velocity normal by qconj
   auto rvdir = v2Rotate(vdir, qconj);

   // rotated aabb for q
   Rectf aabb = { 0, -collisionRange, qlen, collisionRange * 2 };
   if (rayVsAABB(rp1, rvdir, aabb, t, c) && t <= vlen && t > 0.0f) {
      if (t < shortest) {
         shortest = t;
         collision = true;
         col.time = (t / vlen);
         col.collisionNormal = v2Perp(qdir) * -(float)pointSideOfSegment(q1, q2, p1);
      }      
   }

   if (collision) {
      collisions.push_back(col);
   }




   //// ok so we're going to treat the segment as a capsule with 2 spheres and an OBB
   //// we'll check the ray against all 3 to find the closest

   //auto pdiff = p2 - p1;
   //auto plen = v2Len(pdiff);
   //auto pnorm = pdiff / plen;

   //float shortest = FLT_MAX;

   //PhyCollision col = { a, b };
   //col.type = CollisionType_CircleSegment;

   //bool collision = false;
   //float t;
   //Float2 rayConnect;
   //if (intersectRaySphere(p1, pnorm, q1, collisionRange, t, rayConnect) && t <= plen && t > 0.0f) {
   //   t = (t / plen);
   //   if (t < shortest) {
   //      shortest = t;
   //      col.time = t * timeRemaining * 0.95f;
   //      //col.collisionPoint = q1;
   //      collision = true;
   //   }    
   //}

   //if (intersectRaySphere(p1, pnorm, q2, collisionRange, t, rayConnect) && t <= plen && t > 0.0f) {
   //   t = (t / plen);
   //   if (t < shortest) {
   //      shortest = t;
   //      col.time = t * timeRemaining * 0.95f;
   //      //col.collisionPoint = q2;
   //      collision = true;
   //   }
   //}

   //auto qdiff = q2 - q1;
   //auto qlen = v2Len(qdiff);
   //auto qnorm = qdiff / qlen;

   //auto rp1 = q1 + v2Rotate(p1 - q1, v2Conjugate(qnorm));
   //auto rp2 = q1 + v2Rotate(p2 - q1, v2Conjugate(qnorm));
   //auto rnorm = v2Normalized(rp2 - rp1);

   //Rectf aabb = {
   //   q1.x, q1.y - collisionRange,
   //   qlen, collisionRange * 2
   //};
   //Float2 aabbpt;
   //if (rayVsAABB(rp1, rnorm, aabb, t, aabbpt) && t > 0.0f) {
   //   LOG("t:%0.7f plen: %0.3f", t, plen);

   //   if (t < shortest) {
   //      col.time = t * timeRemaining * 0.95f;

   //      float ct;
   //      Float2 closest;
   //      pointClosestOnSegment(q1, q2, p1 + pnorm * col.time, ct, closest);
   //      //col.collisionPoint = closest;
   //      collision = true;
   //   }
   //}
   //

   //if (collision) {
   //   collisions.push_back(col);
   //}





   //if (fabs(t2) < collisionRange*collisionRange) {
   //   float rayt;
   //   Float2 connectPoint;
   //   auto pdiff = p2 - p1;
   //   auto plen = v2Len(pdiff);
   //   if (intersectRaySphere(p1, pdiff/ plen, iPoint, collisionRange, rayt, connectPoint) && rayt <= plen && rayt > EPSILON) {
   //      float t; Float2 d;
   //      pointClosestOnSegment(q1, q2, p2, t, d);
   //      PhyCollision c = { a, b, (rayt / plen) * timeRemaining, CollisionType_CircleSegment, d
   //      };
   //      collisions.push_back(c);
   //   }
   //}

   //float s, t;
   //Float2 c1, c2;
   //auto disq = segmentSegmentDistSquared(p1, p2, q1, q2, s, t, c1, c2);

   //if (disq < collisionRange*collisionRange) {
   //   float t;
   //   Float2 q;
   //   auto dir = p2 - p1;
   //   auto dirlen = v2Len(dir);
   //   if (intersectRaySphere(p1, dir/dirlen, c2, collisionRange, t, q) && t <= dirlen && t > EPSILON) {
   //      PhyCollision c = { a, b, (t / dirlen) * timeRemaining, CollisionType_CircleSegment, c2 };
   //      collisions.push_back(c);
   //   }
   //}

}

// THis is the collision preventative component to complenet overlap resolution
// Based on the propsed new positions of two objects given their velocities, push back a collision object to the list
// marking the time 0-1 it will happen as both objs move forward one frame
void getAllPhyCollisions(Array<PhyObject*>& objs, Array<PhyCollision>& collisions, float &timeRemaining) {

   auto oe = objs.end();
   for (auto i = objs.begin(); i != oe; ++i) {
      for (auto j = i + 1; j != oe; ++j) {
         auto& a = *i;
         auto& b = *j;

         if (a->type == b->type) {
            if (a->type == PhyObject::PhyType_Circle) {
               _detect_CircleCircle(a, b, collisions, timeRemaining);
            }
         }
         else{
            if (a->type == PhyObject::PhyType_Circle) {
               _detect_CircleSegment(a, b, collisions, timeRemaining);
            }
            else {
               _detect_CircleSegment(b, a, collisions, timeRemaining);
            }
         }
      }
   }
}

struct IslandPartition {
   int mailbox = 0;
   Array<PhyObject*> objs;
};

struct IslandPartitionSet {
   int mailbox = 0;
   Array<IslandPartition> islands;

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


// returns whether an overlap correction occured
// islandconnected = is updated to if a nad b should share an island
static bool _resolveOverlap_CircleCircle(PhyObject*a, PhyObject*b, bool& islandConnected) {

   float collisionRange = a->circle.size + b->circle.size;
   float connectRange = collisionRange + (a->maxSpeed + b->maxSpeed);

   auto diff = b->pos - a->pos;
   float distSquared = v2LenSquared(diff);

   // set whether these belong in the same island
   islandConnected = distSquared < (connectRange*connectRange);

   // now check not just against connection range but for current overlap
   if (islandConnected && distSquared < collisionRange * collisionRange) {

      // this is the slow part, generate an impulse against the normalized vector 
      // between the centers
      // the sqrt and / are spendy here but this should rarely be hit

      auto dist = sqrtf(distSquared);
      auto overlappedDist = collisionRange - dist;

      if (overlappedDist > 0.1f) {
         auto impulse = (diff / dist) * (overlappedDist * 1.1f) / (a->invMass + b->invMass);

         a->pos -= impulse * a->invMass;
         b->pos += impulse * b->invMass;
         //a->velocity = { 0.0f, 0.0f };
         //b->velocity = { 0.0f, 0.0f };
         return true;
      }
   }

   return false;
}

// returns whether an overlap correction occured
// islandconnected = is updated to if a and b should share an island
static bool _resolveOverlap_CircleSegment(PhyObject*a, PhyObject*b, bool& islandConnected) {

   float collisionRange = a->circle.size; // only a's radius matters for collision here
   float connectRange = collisionRange + (a->maxSpeed + b->maxSpeed);

   auto ba = b->pos;
   auto bb = b->segment.b;
   auto ap = a->pos;

   bool aabbCheck =
      (ap.x > MIN(ba.x, bb.x) - connectRange) &&
      (ap.y > MIN(ba.y, bb.y) - connectRange) &&
      (ap.x <= MAX(ba.x, bb.x) + connectRange) &&
      (ap.y <= MAX(ba.y, bb.y) + connectRange);

   if (!aabbCheck) {
      islandConnected = false;
      return false;
   }

   if (pointSideOfSegment(ba, bb, ap) > 0) {
      std::swap(ba, bb);
   }

   // for segments, the point we're watching for for collisions is the point on b closest to a
   float bt;
   Float2 bClosest;
   pointClosestOnSegment(b->pos, b->segment.b, a->pos, bt, bClosest);

   Float2 diff = { bClosest.x - a->pos.x,  bClosest.y - a->pos.y };
   float distSquared = v2LenSquared(diff);

   // set whether these belong in the same island
   islandConnected = distSquared < (connectRange*connectRange);

   // now check not just against connection range but for current overlap
   if (islandConnected && distSquared < collisionRange * collisionRange) {

      // this is the slow part, generate an impulse against the normalized vector 
      // between the a's center and b's closest point
      // the sqrt and / are spendy here but this should rarely be hit

      auto dist = sqrtf(distSquared);
      auto overlappedDist = collisionRange - dist;

      if (overlappedDist > 0.1f) {
         auto impulse = (diff / dist) * (overlappedDist  * 1.1f) / (a->invMass + b->invMass);

         //auto side = pointSideOfSegment(ba, bb, ap);
         //if(side != 0)
         //   impulse *= -side;

         //ERR("circle segment overlap");

         a->pos -= impulse * a->invMass;
         b->pos += impulse * b->invMass;
         return true;
      }
   }

   return false;
}

/*
This serves two purposes: 1 is attempting to separate overlapping PhyObjs, 
and the other is to build the island aprtition table

Table building happens in the first attempt automatically, where a and b are 'island connected' if they could potentially
colide this frame (based on positions and max velocity)

overlap resolution is a failsafe for collision restituion failing and causing an overlap. velocity is not factored in here
because the objs are alrerady overlapped, this attempts multiple times to solve the system (resolving one overlap may cause a new overlap)
*/
void resolveOverlaps(Array<PhyObject*>& objs, IslandPartitionSet* pSet) {
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

            bool abIslandConnected = false;

            if (a->type == b->type) {
               if (a->type == PhyObject::PhyType_Circle) {
                  if (_resolveOverlap_CircleCircle(a, b, abIslandConnected)) { overlap = true; }
               }
               else {
                  //_segmentSegment(a, b, collisions, timeRemaining);
               }
            }
            else {
               if (a->type == PhyObject::PhyType_Circle) {
                  if (_resolveOverlap_CircleSegment(a, b, abIslandConnected)) { overlap = true; }
               }
               else {
                  if (_resolveOverlap_CircleSegment(b, a, abIslandConnected)) { overlap = true; }
               }
            }

            // first try, build out the partition table
            if (!attemptsToResolve) {
               partitionSetAddObjs(pSet, a, b, abIslandConnected);
            }
         }
      }
      // continue to resolve until there are no overlaps (or we hit a attempt cap)
   } while (overlap && attemptsToResolve++ < ResolveAttemptLimit);

   if (attemptsToResolve > 10) {
      ERR("Overlaps found, resolved after %d attempts", attemptsToResolve);
   }
   else if(attemptsToResolve > 1) {
      WARN("Overlaps found, resolved after %d attempts", attemptsToResolve);
   }
}

void applyCollisionImpulse(Float2 normal, Float2& vel1, Float2& vel2, float invMass1, float invMass2, float restitution)
{
   Float2 relativeVelocity = vel2 - vel1;

   float d = v2Dot(normal, relativeVelocity);
   if (d > 0.0f) //moving away from each other?  nothing to do?
      return;

   float moveAmount = -(1 + restitution) * d / (invMass1 + invMass2);

   Float2 impulse = normal * moveAmount;
   //WARN("Impulse: %0.7f", v2Len(impulse));
   

   vel1 -= impulse * invMass1;
   vel2 += impulse * invMass2;
}


void updatePhyPositions(Array<PhyObject*>& objs) {

   static IslandPartitionSet pSet;
   Array<PhyCollision> collisions;

   resolveOverlaps(objs, &pSet);
   
   for (auto&& island : pSet.islands) {
      if (island.mailbox != pSet.mailbox) {
         continue;

      }

      float timeRemaining = 1.0f;
      int attemptsToResolve = 0;

      while (timeRemaining > EPSILON /*&& attemptsToResolve < ResolveAttemptLimit*/) {
         collisions.clear();
         getAllPhyCollisions(island.objs, collisions, timeRemaining);         

         auto cCount = collisions.size();

         if (!cCount) {

            //if (attemptsToResolve > 0 && island.objs.size() > 1) {
            //   auto a = island.objs[0];
            //   auto b = island.objs[1];

            //   LOG("Resolved in %d", attemptsToResolve);
            //   LOG("Stepping final by t:%0.7f", timeRemaining);
            //   WARN("aVel=%07f bVel=%07f", v2Len(a->velocity), v2Len(b->velocity));
            //}

            for (auto&& o : island.objs) {
               o->pos += o->velocity * timeRemaining;
               if (o->type == PhyObject::PhyType_Segment) {
                  o->segment.b += o->velocity * timeRemaining;
               }
            }

            //if (attemptsToResolve > 0 && island.objs.size() > 1) {
            //   auto a = island.objs[0]->pos;
            //   auto b = island.objs[1]->pos;
            //   auto distCheck = v2Len(b - a);
            //   LOG("Final A-B Distance: %0.7f", distCheck);
            //   if (distCheck < island.objs[0]->circle.size * 2) {
            //      ERR("Overlap!");
            //   }
            //   LOG("-------------------------------------------", cCount);
            //}

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
         auto step = c.time * 0.95f;

         //LOG("Stepping by t:%0.7f", step);
         for (auto&& d : island.objs) {            
            d->pos += d->velocity * step;
            if (d->type == PhyObject::PhyType_Segment) {
               d->segment.b += d->velocity * step;
            }
         }

         applyCollisionImpulse(c.collisionNormal, a.velocity, b.velocity, a.invMass, b.invMass, 0.01f);

         timeRemaining -= c.time;

         //{
         //   auto distCheck = v2Len((b.pos + b.velocity * timeRemaining) - (a.pos + a.velocity * timeRemaining));
         //   WARN("After A-B impulse, dist should be: %0.7f", distCheck);
         //   WARN("Assuming aVel=%07f && bVel=%07f", v2Len(a.velocity), v2Len(b.velocity));
         //}

         
         ++attemptsToResolve;

         //if (attemptsToResolve == ResolveAttemptLimit) {
         //   ERR("Failed to resolve system");
         //   break;

         //}
      }
   }


}


