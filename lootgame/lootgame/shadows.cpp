#include "stdafx.h"

#include "math.h"
#include "render.h"
#include "coords.h"
#include "game.h"

enum ShadowRayType {
   ShadowRayType_LeftAligned = 0,
   ShadowRayType_RightAligned,
   ShadowRayType_Straddling
};

enum ShadowNodeType {
   ShadowNode_EndPoint,
   ShadowNode_OnSegment,
   ShadowNode_OnEdge
};


struct ShadowSegment {
   Float2 a, b;
   bool aclipped, bclipped;
   float adistSq = 0.0f, bdistSq = 0.0f;

   int asegment = -1, bsegment = -1;
};

struct ShadowRay {
   Float2 p;
   float dist;
   int asegment = -1, bsegment = -1;
   Float2 asegEnd, bsegEnd;
   int asegSide, bsegSide;
   ShadowRayType type;

   Float2 dir;
   float anglef;

   bool onEdge = false;
};

struct ShadowNode {
   Float2 p;
   float dist;

   ShadowNodeType type;
   int segment;
   Float2 segmentEnd; // clockwise endpoint of active segment 
   float segEndDist;
};

struct ShadowSystem {
   Array<ShadowSegment> segments;
   Array<ShadowRay> rays;
};

static Float2 _clipSegment(Float2 inside, Float2 outside, Float2 cpos, float radius) {

   // cast from b to a to intersect sphere
   auto segdiff = inside - outside;
   auto seglen = v2Len(segdiff);
   auto segdir = segdiff / seglen;

   float t; Float2 q;
   intersectRaySphere(outside, segdir, cpos, radius, t, q);
   return q;
}

static ShadowSystem _buildSystem(Float2 center, float radius, Array<ConvexPoly>& blockers, int circleSegmentCount) {
   ShadowSystem out;

   // build the data set
   Array<ShadowSegment> orphanedSegments;
   auto radiusSq = radius * radius;

   // clip every blocker and add them to the segements list
   for (auto&& poly : blockers) {
      auto segStart = out.segments.size();

      // evaluate each segment of the blocker
      // we want to store out all clipped-in-circle segments and also save some precalculated stats
      // about distance and how the segments connect to each other
      for (auto iter = poly.points.begin(); iter != poly.points.end(); ++iter) {
         auto next = iter + 1;
         if (next == poly.points.end()) next = poly.points.begin();

         auto a = *iter;
         auto b = *next;

         auto bdiff = b - center;

         auto adistSq = v2LenSquared(a - center);
         auto bdistSq = v2LenSquared(bdiff);

         bool ain = adistSq < radiusSq;
         bool bin = bdistSq < radiusSq;

         if (ain && bin) {
            // whole segment in range
            out.segments.push_back({ a, b, false, false, adistSq, bdistSq });
         }
         else if (!ain && !bin) {
            // both endpoints are outside the radius, 
            // we still need to cast rays from both points to potentially find an overlapping segment
            auto segdiff = b - a;
            auto seglen = v2Len(segdiff);
            auto segdir = segdiff / seglen;

            float t; Float2 qa, qb;
            if (intersectRaySphere(a, segdir, center, radius, t, qa) &&
               intersectRaySphere(b, -segdir, center, radius, t, qb)) {

               orphanedSegments.push_back({ qa, qb, true, true, v2LenSquared(center - qa), v2LenSquared(center - qb), -1, -1 });
            }
         }
         else {
            // segment straddles the edge of the circle, clip it
            if (bin) {
               a = _clipSegment(b, a, center, radius);
               out.segments.push_back({ a, b, true, false, v2LenSquared(a - center), bdistSq });
            }
            else {
               b = _clipSegment(a, b, center, radius);
               out.segments.push_back({ a, b, false, true, adistSq, v2LenSquared(b - center) });
            }
         }
      }

      // assuming the blocker is correctly convex, we can iterate over all the segments we just added
      // and link up their a-b connections so that each segment has appropriate indices into the final segment list for
      // segments they attach to
      auto segCount = out.segments.size() - segStart;
      if (segCount > 0) {
         for (size_t s = segStart; s < segStart + segCount; ++s) {
            auto &seg = out.segments[s];

            if (!seg.aclipped) {
               seg.asegment = s - 1;
            }
            if (!seg.bclipped) {
               seg.bsegment = s + 1;
               if (seg.bsegment >= segStart + segCount) {
                  seg.bsegment = segStart;
                  out.segments[segStart].asegment = s;
               }
            }
         }
      }
   }

   // orphaned segments are ones that got clipped on both ends so they dont connect to anything, push them on after the fact
   // to not mess with the above loops
   out.segments.push_back_array(orphanedSegments);

   // iterating over the segment list, we want to create a ray for every _vertex_ in the system
   // being careful nmot to make duplicate rays on connected segments
   // these rays contain the vertex point, that point's distance to center, and 1 or more 
   // segment indices that the ray is an endpoint for
   int i = 0;
   for (auto &segment : out.segments) {
      bool hasAConnect = segment.asegment >= 0;
      bool hasBConnect = segment.bsegment >= 0;

      if (!hasAConnect && !hasBConnect) {
         out.rays.push_back({ segment.a, sqrtf(segment.adistSq), -1, i });
         out.rays.push_back({ segment.b, sqrtf(segment.bdistSq), i, -1 });
      }
      else if (!hasAConnect) {
         out.rays.push_back({ segment.a, sqrtf(segment.adistSq), -1, i });
      }
      else if (hasAConnect && hasBConnect) {
         out.rays.push_back({ segment.a, sqrtf(segment.adistSq), segment.asegment, i });
      }
      else {
         out.rays.push_back({ segment.a, sqrtf(segment.adistSq), segment.asegment, i });
         out.rays.push_back({ segment.b, sqrtf(segment.bdistSq), i, -1 });
      }

      ++i;
   }

   // ray post-process:
   // for attached segments in the ray, save out the opposiute endpoint ofd that segment and 
   // whether that endpoint sits to the left or right of the ray point
   // if both segments extend to the left, the ray is marked as right-aligned
   // if they both extend to the right, the ray is marked as left-aligned
   // we also store the normalized ray vector and its angle in radians
   for (auto&& ray : out.rays) {
      if (ray.asegment >= 0) {
         ray.asegEnd = out.segments[ray.asegment].a;
         ray.asegSide = pointSideOfSegment(center, ray.p, ray.asegEnd);
      }
      if (ray.bsegment >= 0) {
         ray.bsegEnd = out.segments[ray.bsegment].b;
         ray.bsegSide = pointSideOfSegment(center, ray.p, ray.bsegEnd);
      }

      if (ray.asegment >= 0 && ray.bsegment >= 0) {
         if (ray.asegSide == ray.bsegSide) {
            ray.type = ray.asegSide >= 0 ? ShadowRayType_LeftAligned : ShadowRayType_RightAligned;
         }
         else {
            ray.type = ShadowRayType_Straddling;
         }
      }
      else if (ray.asegment >= 0) {
         ray.type = ray.asegSide >= 0 ? ShadowRayType_LeftAligned : ShadowRayType_RightAligned;
      }
      else if (ray.bsegment >= 0) {
         ray.type = ray.bsegSide >= 0 ? ShadowRayType_LeftAligned : ShadowRayType_RightAligned;
      }

      ray.dir = (ray.p - center) / ray.dist;
      ray.anglef = v2Angle(ray.dir);
   }


   // we add our own automatic set of extra rays to fill the circle out
   // these are marked as edge rays so they are automatically ignored during traversal except when necessary
   out.rays.reserve(out.rays.size() + circleSegmentCount);
   auto angleMax = TAU * ((float)circleSegmentCount) / (float)circleSegmentCount;
   for (int i = 0; i < circleSegmentCount; ++i) {
      auto a = (i / (float)circleSegmentCount) * angleMax;

      ShadowRay ray;
      ray.dir = Float2{ cosf(a), sinf(a) };
      ray.p = center + ray.dir * radius;
      ray.dist = radius;
      ray.anglef = v2Angle(ray.dir);

      ray.onEdge = true;
      out.rays.push_back(ray);
   }

   // sort all the arrays in polar order
   std::sort(out.rays.begin(), out.rays.end(), [](ShadowRay const&a, ShadowRay const&b) {return a.anglef < b.anglef; });

   // solving the system is simpler if our starting ray does not intersect any segments
   // we check each reay against each segment until we find one
   // and then rotate the rays vector so its first.
   int clearRay = -1;
   for (int i = 0; i < out.rays.size(); ++i) {
      auto&ray = out.rays[i];
      if (ray.onEdge) {
         continue;
      }

      bool intersect = false;
      for (auto s = 0; s < out.segments.size(); ++s) {
         if (s == ray.asegment || s == ray.bsegment) {
            continue;
         }
         auto&seg = out.segments[s];
         float u, t; Float2 q;
         if (segmentSegmentIntersect(center, ray.p, seg.a, seg.b, u, t, q)) {
            intersect = true;
            break;
         }
      }
      if (!intersect) {
         clearRay = i;
         break;
      }
   }

   if (clearRay >= 0) {
      std::rotate(out.rays.begin(), out.rays.begin() + clearRay, out.rays.end());
   }


   return out;
}




static int _rayInnerSegment(ShadowRay&ray, Float2 ppos) {
   if (ray.asegment < 0) { return ray.bsegment; }
   if (ray.bsegment < 0) { return ray.asegment; }
   if (ray.asegment < 0 && ray.bsegment < 0) { return -1; }

   if (ray.type == ShadowRayType_LeftAligned) {
      // both head right, if the determinate between them is positive
      bool aInner = v2Determinant(ray.asegEnd - ray.p, ray.bsegEnd - ray.p) < 0;
      return aInner ? ray.asegment : ray.bsegment;
   }
   else {
      // return the one thats on the right
      return ray.asegSide >= 0 ? ray.asegment : ray.bsegment;
   }
}

static void _commitTriangle(Array<Float2> &buffer, Float2 a, Float2 b, Float2 c) {
   buffer.push_back_array({ a, b, c });
}

static void _segmentClockwiseEnd(ShadowSegment& segment, Float2 ppos, Float2&endOut) {
   if (v2Determinant(segment.a - ppos, segment.b - ppos) < 0) {
      endOut = segment.a;
   }
   else {
      endOut = segment.b;
   }

}

void _solveSystem(ShadowSystem&sys, Float2 center, float radius, Array<Float2> &buffer) {
   ShadowNode active;
   auto& firstRay = sys.rays[0];

   Array<int> openList;

   // use first ray to setup our initial node

   // any segments the ray intersects needs to be added to the open lis
   int shortestSegment = -1;
   float shortestT = FLT_MAX;
   Float2 shortestPoint;

   auto endPoint = center + firstRay.dir * radius;

   for (int s = 0; s < sys.segments.size(); ++s) {
      if (s == firstRay.asegment || s == firstRay.bsegment) {
         continue;
      }
      auto& seg = sys.segments[s];

      float u, t; Float2 q;
      if (segmentSegmentIntersect(center, endPoint, seg.a, seg.b, u, t, q)) {
         openList.push_back(s);
         if (t < shortestT) {
            shortestT = t;
            shortestSegment = s;
            shortestPoint = q;
         }
      }
   }

   if (firstRay.onEdge) {
      active.p = firstRay.p;
      active.dist = firstRay.dist;
      active.segment = -1;
      active.type = ShadowNode_OnEdge;
   }
   else {
      if (firstRay.type == ShadowRayType_RightAligned) {
         if (shortestSegment >= 0) {
            openList.erase(std::find(openList.begin(), openList.end(), shortestSegment));

            active.p = shortestPoint;
            active.dist = v2Dist(active.p, center);
            active.segment = shortestSegment;
            active.type = ShadowNode_OnSegment;

            _segmentClockwiseEnd(sys.segments[active.segment], center, active.segmentEnd);
         }
         else {
            active.p = endPoint;
            active.dist = radius;
            active.segment = shortestSegment;
            active.type = ShadowNode_OnEdge;

            _segmentClockwiseEnd(sys.segments[active.segment], center, active.segmentEnd);
         }
      }
      else {
         active.p = firstRay.p;
         active.dist = firstRay.dist;
         active.segment = _rayInnerSegment(firstRay, center);
         active.type = ShadowNode_EndPoint;

         _segmentClockwiseEnd(sys.segments[active.segment], center, active.segmentEnd);

         if (firstRay.type == ShadowRayType_LeftAligned) {
            auto otherSeg = -1;
            if (firstRay.asegment >= 0 && firstRay.asegment != active.segment) { otherSeg = firstRay.asegment; }
            else if (firstRay.bsegment >= 0 && firstRay.bsegment != active.segment) { otherSeg = firstRay.bsegment; }
            if (otherSeg >= 0) {
               openList.push_back(otherSeg);
            }
         }
      }
   }

   // now iterate over our rays
   for (auto rayiter = sys.rays.begin() + 1; ; ++rayiter) {
      bool finish = false;
      if (rayiter == sys.rays.end()) {
         rayiter = sys.rays.begin();
         finish = true;
      }
      auto&ray = *rayiter;

      // if active and ray are both on circle's edge
      if (active.type == ShadowNode_OnEdge && ray.onEdge) {
         if (!openList.empty()) {
            continue;
         }

         _commitTriangle(buffer, center, active.p, ray.p);
         active.p = ray.p;
         active.dist = ray.dist;
         active.segment = -1;
         active.type = ShadowNode_OnEdge;
      }
      // if active is on edge and ray is not
      else if (active.type == ShadowNode_OnEdge) {

         _commitTriangle(buffer, center, active.p, center + ray.dir * radius);
         active.p = ray.p;
         active.dist = ray.dist;
         active.segment = _rayInnerSegment(ray, center);
         active.type = ShadowNode_EndPoint;
         _segmentClockwiseEnd(sys.segments[active.segment], center, active.segmentEnd);

         int otherSeg = -1;
         if (ray.asegment >= 0 && ray.asegment != active.segment) { otherSeg = ray.asegment; }
         else if (ray.bsegment >= 0 && ray.bsegment != active.segment) { otherSeg = ray.bsegment; }
         if (otherSeg >= 0) {
            auto found = std::find(openList.begin(), openList.end(), otherSeg);
            if (found != openList.end()) {
               openList.erase(found);
            }
            else {
               openList.push_back(otherSeg);
            }
         }

      }
      // if ray is on edge and active is not
       else if (ray.onEdge) {

       }
      // ray closes active segment
      else if (ray.asegment == active.segment || ray.bsegment == active.segment) {
         _commitTriangle(buffer, center, active.p, ray.p);
         auto otherSeg = -1;
         if (ray.asegment >= 0 && ray.asegment != active.segment) { otherSeg = ray.asegment; }
         else if (ray.bsegment >= 0 && ray.bsegment != active.segment) { otherSeg = ray.bsegment; }
         if (otherSeg >= 0) {
            auto found = std::find(openList.begin(), openList.end(), otherSeg);
            if (found != openList.end()) {
               openList.erase(found);
            }
            else {
               active.p = ray.p;
               active.dist = ray.dist;
               active.segment = otherSeg;
               active.type = ShadowNode_EndPoint;
               _segmentClockwiseEnd(sys.segments[active.segment], center, active.segmentEnd);
            }
         }
         if (ray.type == ShadowRayType_RightAligned) {
            int shortestSegment = -1;
            float shortestT = FLT_MAX;
            Float2 shortestPoint;

            auto endPoint = center + ray.dir *radius;

            for (auto s : openList) {
               if (s == ray.asegment || s == ray.bsegment) {
                  continue;
               }
               auto& seg = sys.segments[s];
               float u, t; Float2 q;
               if (segmentSegmentIntersect(center, endPoint, seg.a, seg.b, u, t, q)) {
                  if (t < shortestT) {
                     shortestT = t;
                     shortestSegment = s;
                     shortestPoint = q;
                  }
               }
            }

            if (shortestSegment >= 0) {
               openList.erase(std::find(openList.begin(), openList.end(), shortestSegment));
               active.p = shortestPoint;
               active.dist = v2Dist(active.p, center);
               active.segment = shortestSegment;
               active.type = ShadowNode_OnSegment;
               _segmentClockwiseEnd(sys.segments[active.segment], center, active.segmentEnd);
            }
            else {
               active.p = endPoint;

               active.dist = radius;
               active.segment = -1;
               active.type = ShadowNode_OnEdge;
            }
         }
      }
      else {
         bool rayBehind = v2Determinant(ray.p - active.p, active.segmentEnd - active.p) >= 0;

         // ray.p is closer to center than active.p
         if (!rayBehind) {

            auto endPoint = center + ray.dir * radius;
            auto &activeSegment = sys.segments[active.segment];

            float u, t; Float2 q;
            segmentSegmentIntersect(center, endPoint, activeSegment.a, activeSegment.b, u, t, q);

            _commitTriangle(buffer, center, active.p, q);
            openList.push_back(active.segment);

            active.p = ray.p;
            active.dist = ray.dist;
            active.segment = _rayInnerSegment(ray, center);
            active.type = ShadowNode_EndPoint;
            _segmentClockwiseEnd(sys.segments[active.segment], center, active.segmentEnd);

            int otherSeg = -1;
            if (ray.asegment >= 0 && ray.asegment != active.segment) { otherSeg = ray.asegment; }
            else if (ray.bsegment >= 0 && ray.bsegment != active.segment) { otherSeg = ray.bsegment; }
            if (otherSeg >= 0) {
               auto found = std::find(openList.begin(), openList.end(), otherSeg);
               if (found != openList.end()) {
                  openList.erase(found);
               }
               else {
                  openList.push_back(otherSeg);
               }
            }
         }
         // ray.p is further from center than active.p
         else {
            if (ray.asegment >= 0) {
               auto found = std::find(openList.begin(), openList.end(), ray.asegment);
               if (found != openList.end()) {
                  openList.erase(found);
               }
               else {
                  openList.push_back(ray.asegment);
               }
            }
            if (ray.bsegment >= 0) {
               auto found = std::find(openList.begin(), openList.end(), ray.bsegment);
               if (found != openList.end()) {
                  openList.erase(found);
               }
               else {
                  openList.push_back(ray.bsegment);
               }
            }
         }
      }

      if (finish) {
         break;
      }
   }
}

Array<Float2> render::buildShadowsBuffer(Float2 center, float radius, Array<ConvexPoly>& blockers, int circleSegmentCount) {   
   auto sys = _buildSystem(center, radius, blockers, circleSegmentCount);
   Array<Float2> out;
   out.reserve(circleSegmentCount * 3);
   _solveSystem(sys, center, radius, out);
   return out;
}