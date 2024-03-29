#include "stdafx.h"

#include "math.h"

Float3 vCross(Float3 v1, Float3 v2) {
   Float3 out = {
      (v1.y * v2.z) - (v1.z*v2.y),
      (v1.z*v2.x) - (v1.x*v2.z),
      (v1.x*v2.y) - (v1.y*v2.x)
   };
   return out;
}
float vDot(Float3 v1, Float3 v2) {
   return (v1.x * v2.x) + (v1.y * v2.y) + (v1.z * v2.z);
}
Float3 vSubtract(Float3 a, Float3 b) {
   return { a.x - b.x, a.y - b.y, a.z - b.z };
}
Float3 vAdd(Float3 a, Float3 b) {
   return { a.x + b.x, a.y + b.y, a.z + b.z };
}
Float3 vScale(Float3 a, float s) {
   return { a.x * s, a.y * s, a.z * s };
}
Float3 vNormalized(Float3 v) {
   return vScale(v, sqrtf(vDot(v, v)));
}

float v2Dot(Float2 const&a, Float2 const& b) {
   return (a.x * b.x) + (a.y * b.y);
}
float v2Dist(Float2 a, Float2 b) {
   return v2Len(b - a);
}
float v2DistSquared(Float2 a, Float2 b) {
   return v2LenSquared(b - a);
}
float v2Len(Float2 v) {
   return sqrtf(v2Dot(v, v));
}
float v2LenSquared(Float2 v) {
   return v2Dot(v, v);
}
Float2 v2Subtract(Float2 a, Float2 b) {
   return { a.x - b.x, a.y - b.y };
}
Float2 v2Add(Float2 a, Float2 b) {
   return { a.x + b.x, a.y + b.y };
}
Float2 v2Scale(Float2 a, float s) {
   return { a.x * s, a.y * s };
}
Float2 v2Normalized(Float2 v) {
   auto lnsq = v2LenSquared(v);
   if (lnsq > 0.001f) {
      return v / sqrtf(lnsq);
   }
   return { 0,0 };
}
float v2Angle(Float2 v) {
   return atan2f(v.y, v.x);
}
Float2 v2Orthogonal(Float2 v) {
   return { v.y, -v.x };
}

//helper.  determines orientation of two vectors, positive vs. negative means clockwise/counterclockwise orientation
float v2Determinant(Float2 a, Float2 b) {
   return a.x * b.y - a.y * b.x;
}
//complex number rotation!!
Float2 v2Rotate(Float2 direction, Float2 rotation) {
   //first - last
   float x = direction.x * rotation.x - direction.y * rotation.y;
   //outside + inside 
   float y = direction.x * rotation.y + direction.y * rotation.x;
   return Float2{ x,y };
}

//inverts rotation of unit vector
Float2 v2Conjugate(Float2 unit) {
   return Float2{ unit.x, -unit.y };
}
Float2 v2FromAngle(float radians) {
   return { cosf(radians), sinf(radians) };
}

Float2 v2Perp(Float2 v) {
   return Float2{ -v.y, v.x };
}

Float2 v2RotateTowards(Float2 direction, Float2 target, Float2 perFrame) {

   int det = SIGN(v2Determinant(direction, target));
   if (det == 0) {

      // perfect 180 turnaround, just rotate away
      if (v2Dot(direction, target) < 0) {
         return v2Rotate(direction, perFrame);
      }
      else {
         return target; //they are exactly aligned to start with, nothing to do
      }
   }
   if (det < 0) perFrame = v2Conjugate(perFrame);  //this is being treated like a complex number and this is the conjugate.
   direction = v2Rotate(direction, perFrame);
   if (det != SIGN(v2Determinant(direction, target))) {
      //if this is true we overshot our target
      return target;
   }
   return direction;
}

Float2 v2MoveTowards(Float2 position, Float2 target, float speed) {
   auto dir = target - position;
   float length = sqrtf(v2Dot(dir, dir));

   if (length < speed) {
      return target;
   }

   return position + dir * (speed / length);
}

Float2 v2CapLength(Float2 v, float max) {
   auto lensq = v2LenSquared(v);
   if (lensq > max*max) {
      if (lensq > 0.001f) {
         return (v / sqrtf(lensq)) * max;
      }
      return { 0,0 };
   }
   return v;
}

bool circleVsAabb(Float2 co, float cr, Rectf const& aabb) {
   Float2 a = { aabb.x, aabb.y };
   Float2 b = { a.x + aabb.w, a.y + aabb.h };

   Float2 offset = { 0,0 };

   if (co.x > b.x) { offset.x = co.x - b.x; }
   else if (co.x < a.x) { offset.x = a.x - co.x; }

   if (co.y > b.y) { offset.y = co.y - b.y; }
   else if (co.y < a.y) { offset.y = a.y - co.y; }

   // if offset length is less than the radius, collision!
   return v2Len(offset) < cr;
}

int32_t int2Dot(Int2 v1, Int2 v2) {
   return v1.x * v2.x + v1.y * v2.y;
}

Int2 int2Perp(Int2 v) {
   return Int2{ -v.y, v.x };
}

Int2 int2Subtract(Int2 v1, Int2 v2) {
   return Int2{ v1.x - v2.x, v1.y - v2.y };
}

int32_t pointOnLine(Int2 l1, Int2 l2, Int2 point) {
   return int2Dot(int2Perp(int2Subtract(l2, l1)), int2Subtract(point, l1));
}

void pointClosestOnSegment(Float2 a, Float2 b, Float2 p, float& t, Float2& d) {
   Float2 ab = b - a;

   // project p onto ab, but defer the divide by dot(ab,ab)
   t = v2Dot(p - a, ab);
   if (t <= 0.0f) {
      // c projects outside the [a,b] interval on the a side, so clamp to a
      t = 0.0f;
      d = a;
   }
   else {
      float denom = v2Dot(ab, ab);
      if (t >= denom) {
         // c projects outside the [a,b] interval on the b side, so clamp to b
         t = 1.0f;
         d = b;
      }
      else {
         // c projects inside the [a,b] interval, now we do our deferred divide
         t /= denom;
         d = a + ab * t;
      }
   }
}

float pointDistToSegmentSquared(Float2 a, Float2 b, Float2 c) {
   auto ab = b - a, ac = c - a, bc = c - b;
   float e = v2Dot(ac, ab);
   // handle c projecting outside ab
   if (e <= 0.0f) return v2Dot(ac, ac);
   float f = v2Dot(ab, ab);
   if (e >= f) return v2Dot(bc, bc);
   // handle c projects onto ab
   return v2Dot(ac, ac) - e * e / f;

}

int pointSideOfSegment(Float2 a, Float2 b, Float2 p) {
   b -= a; p -= a;
   return SIGN(v2Determinant(b, p));
}

bool pointInPoly(Float2 p, Float2*pts, int vCount) {
   int lastSide = 0;
   for (int i = 0; i < vCount; ++i) {
      int next = i + 1;
      if (i == vCount - 1) { next = 0; }
      auto side = pointSideOfSegment(pts[i], pts[next], p);
      if (side && lastSide && side != lastSide) {
         return false;
      }
      lastSide = side;
   }
   return true;
}

bool polyConvex(Float2*pts, int vCount) {
   if (vCount <= 3) {
      return true;
   }

   for (int i = 0; i < vCount; ++i) {
      int iEnd = i+1;
      if (i == vCount - 1) {
         iEnd = 0;
      }
      int lastSide = 0;
      for (int j = 0; j < vCount; ++j) {
         if (j == iEnd || j == i) { continue; }

         auto side = pointSideOfSegment(pts[i], pts[iEnd], pts[j]);
         if (side && lastSide && side != lastSide) {
            return false;
         }
         lastSide = side;
      }
   }

   return true;
}

bool rayVsAABB(Float2 _p, Float2 _d, Rectf aabb, float& tmin, Float2& q) {
   tmin = 0.0f;
   float tmax = FLT_MAX;
   float* p = (float*)&_p;
   float* d = (float*)&_d;
   float min[2] = { aabb.x, aabb.y };
   float max[2] = { min[0] + aabb.w, min[1] + aabb.h };

   for (int i = 0; i < 2; ++i) {
      if (fabs(d[i]) < EPSILON) {
         if (p[i] < min[i] || p[i] > max[i]) return false;
      }
      else {
         float ood = 1.0f / d[i];
         float t1 = (min[i] - p[i]) * ood;
         float t2 = (max[i] - p[i]) * ood;

         if (t1 > t2) std::swap(t1, t2);

         tmin = MAX(tmin, t1);
         tmax = MIN(tmax, t2);

         if (tmin > tmax) return false;
      }
   }
   q = _p + _d * tmin;
   return true;
}

bool rayVsAABB2(Float2 p1, Float2 p2, Rectf aabb, float &tmin) {
   float s, t;
   Float2 connectPoint;

   Float2 a = aabb.Min();
   Float2 b = { a.x + aabb.w, a.y };
   Float2 c = { a.x, a.y + aabb.h };
   Float2 d = { b.x, c.y };

   float shortest = FLT_MAX;
   bool intersect = false;

   if (segmentSegmentIntersect(p1, p2, a, b, s, t, connectPoint)) {
      if (t < shortest) {
         shortest = t;
         intersect = true;
      }
   }
   if (segmentSegmentIntersect(p1, p2, b, c, s, t, connectPoint)) {
      if (t < shortest) {
         shortest = t;
         intersect = true;
      }
   }
   if (segmentSegmentIntersect(p1, p2, c, d, s, t, connectPoint)) {
      if (t < shortest) {
         shortest = t;
         intersect = true;
      }
   }
   if (segmentSegmentIntersect(p1, p2, d, a, s, t, connectPoint)) {
      if (t < shortest) {
         shortest = t;
         intersect = true;
      }
   }

   return intersect;  

}

// Returns 1 if the lines intersect, otherwise 0. In addition, if the lines 
// intersect the intersection point may be stored in the floats i_x and i_y.
char get_line_intersection(float p0_x, float p0_y, float p1_x, float p1_y,
   float p2_x, float p2_y, float p3_x, float p3_y, float *i_x, float *i_y)
{
   float s1_x, s1_y, s2_x, s2_y;
   s1_x = p1_x - p0_x;     s1_y = p1_y - p0_y;
   s2_x = p3_x - p2_x;     s2_y = p3_y - p2_y;

   float s, t;
   s = (-s1_y * (p0_x - p2_x) + s1_x * (p0_y - p2_y)) / (-s2_x * s1_y + s1_x * s2_y);
   t = (s2_x * (p0_y - p2_y) - s2_y * (p0_x - p2_x)) / (-s2_x * s1_y + s1_x * s2_y);

   if (s >= 0 && s <= 1 && t >= 0 && t <= 1)
   {
      // Collision detected
      if (i_x != NULL)
         *i_x = p0_x + (t * s1_x);
      if (i_y != NULL)
         *i_y = p0_y + (t * s1_y);
      return 1;
   }

   return 0; // No collision
}

// Returns 1 if the lines intersect, otherwise 0. In addition, if the lines 
// intersect the intersection point may be stored in the floats i_x and i_y.
bool segmentSegmentIntersect(Float2 p1, Float2 p2, Float2 q1, Float2 q2, float&s, float& t, Float2& i) {
   auto ps = p2 - p1;
   auto qs = q2 - q1;

   s = (-ps.y * (p1.x - q1.x) + ps.x * (p1.y - q1.y)) / (-qs.x * ps.y + ps.x * qs.y);
   t = (qs.x * (p1.y - q1.y) - qs.y * (p1.x - q1.x)) / (-qs.x * ps.y + ps.x * qs.y);

   if (s >= 0 && s <= 1 && t >= 0 && t <= 1) {
      i.x = p1.x + (t * ps.x);
      i.y = p1.y + (t * ps.y);
      return true;
   }

   return false;
}

float segmentSegmentDistSquared(Float2 p1, Float2 q1, Float2 p2, Float2 q2, float& s, float& t, Float2& c1, Float2& c2) {
   auto d1 = q1 - p1;
   auto d2 = q2 - p2;
   auto r = p1 - p2;
   auto a = v2Dot(d1, d1);
   auto e = v2Dot(d2, d2);
   auto f = v2Dot(d2, r);

   if (a <= EPSILON && e <= EPSILON) {
      s = t = 0.0f;
      c1 = p1;
      c2 = p2;
      return v2Dot(c1 - c2, c1 - c2);
   }

   if (a <= EPSILON) {
      s = 0.0f;
      t = f / e;
      t = clamp(t, 0.0f, 1.0f);
   }
   else {
      float c = v2Dot(d1, r);
      if (e <= EPSILON) {
         t = 0.0f;
         s = clamp(-c / a, 0.0f, 1.0f);
      }
      else {
         float b = v2Dot(d1, d2);
         float denom = a * e - b * b;

         if (denom != 0.0f) {
            s = clamp((b*f - c * e) / denom, 0.0f, 1.0f);
         }
         else {
            s = 0.0f;
         }

         t = (b*s + f) / e;

         if (t < 0.0f) {
            t = 0.0f;
            s = clamp(-c / a, 0.0f, 1.0f);
         }
         else if (t > 1.0f) {
            t = 1.0f;
            s = clamp((b - c) / a, 0.0f, 1.0f);
         }
      }
   }

   c1 = p1 + d1 * s;
   c2 = p2 + d2 * t;

   return v2Dot(c1 - c2, c1 - c2);
}

bool lineSegmentIntersectsAABBi(Int2 l1, Int2 l2, Recti *rect) {
   int32_t topleft, topright, bottomright, bottomleft;

   if (l1.x > rect->x + rect->w && l2.x > rect->x + rect->w) { return false; }
   if (l1.x < rect->x && l2.x < rect->x) { return false; }
   if (l1.y > rect->y + rect->h && l2.y > rect->y + rect->h) { return false; }
   if (l1.y < rect->y && l2.y < rect->y) { return false; }

   topleft = pointOnLine(l1, l2, Int2{ rect->x, rect->y });
   topright = pointOnLine(l1, l2, Int2{ rect->x + rect->w, rect->y });
   bottomright = pointOnLine(l1, l2, Int2{ rect->x + rect->w, rect->y + rect->h });
   bottomleft = pointOnLine(l1, l2, Int2{ rect->x, rect->y + rect->h });

   if (topleft > 0 && topright > 0 && bottomright > 0 && bottomleft > 0) {
      return false;
   }

   if (topleft < 0 && topright < 0 && bottomright < 0 && bottomleft < 0) {
      return false;
   }

   return true;

}
Recti getProportionallyFitRect(Int2 srcSize, Int2 destSize) {
   return getProportionallyFitRect(
      Float2{ (float)srcSize.x, (float)srcSize.y }, 
      Float2{ (float)destSize.x, (float)destSize.y });
}
Recti getProportionallyFitRect(Float2 srcSize, Float2 destSize) {
   float rw = (float)destSize.x;
   float rh = (float)destSize.y;
   float cw = (float)srcSize.x;
   float ch = (float)srcSize.y;

   float ratio = MIN(rw / cw, rh / ch);

   Recti out = { 0, 0, (int32_t)(cw * ratio), (int32_t)(ch * ratio) };
   out.x += (int32_t)((rw - out.w) / 2.0f);
   out.y += (int32_t)((rh - out.h) / 2.0f);

   return out;
}


Matrix Matrix::identity() {
   Matrix out = { 0 };
   out[0] = 1.0f;
   out[5] = 1.0f;
   out[10] = 1.0f;
   out[15] = 1.0f;
   return out;
}
Matrix Matrix::ortho(float left, float right, float bottom, float top, float near, float far) {
   Matrix out = Matrix::identity();

   out[1] = out[2] = out[3] = out[4] = out[6] =
      out[7] = out[8] = out[9] = out[11] = 0.0f;

   out[0] = 2.0f / (right - left);
   out[5] = 2.0f / (top - bottom);
   out[10] = -2.0f / (far - near);
   out[15] = 1.0f;

   out[12] = -((right + left) / (right - left));
   out[13] = -((top + bottom) / (top - bottom));
   out[14] = -((far + near) / (far - near));
   return out;
}
Matrix Matrix::perspective(float fovy, float aspect, float zNear, float zFar) {
   Matrix out = { 0 };

   float f = 1.0f / tanf((fovy * DEG2RAD) / 2.0f);

   out[0] = f / aspect;
   out[5] = f;
   out[10] = (zFar + zNear) / (zNear - zFar);
   out[11] = -1.0f;
   out[14] = (2 * zFar * zNear) / (zNear - zFar);

   return out;
}

Matrix Matrix::fromBasis(Float3 const &v1, Float3 const &v2, Float3 const &v3) {
   Matrix out = identity();

   out[0] = v1.x; out[1] = v1.y; out[2] = v1.z;
   out[4] = v2.x; out[5] = v2.y; out[6] = v2.z;
   out[8] = v3.x; out[9] = v3.y; out[10] = v3.z;

   return out;
}

Matrix Matrix::lookAt(Float3 const &eye, Float3 const &center, Float3 const &up) {
   Matrix out = { 0 };
   Float3 n = vNormalized(vSubtract(eye, center));
   Float3 u = vNormalized(vCross(up, n));
   Float3 v = vNormalized(vCross(n, u));

   out[0] = u.x;
   out[4] = u.y;
   out[8] = u.z;

   out[1] = v.x;
   out[5] = v.y;
   out[9] = v.z;

   out[2] = n.x;
   out[6] = n.y;
   out[10] = n.z;

   out[12] = -vDot(eye, u);
   out[13] = -vDot(eye, v);
   out[14] = -vDot(eye, n);

   out[15] = 1.0f;

   return out;
}

Matrix Matrix::rotate2D(float rads) {
   Matrix out = Matrix::identity();

   out[0] = out[5] = cosf(rads);
   out[1] = sinf(rads);
   out[4] = -out[1];

   return out;
}

Matrix Matrix::scale2f(Float2 const &v) {
   Matrix out = Matrix::identity();
   out[0] = v.x;
   out[5] = v.y;
   return out;
}
Matrix Matrix::scale3f(Float3 const &v) {
   Matrix out = Matrix::identity();
   out[0] = v.x;
   out[5] = v.y;
   out[10] = v.z;
   return out;
}
Matrix Matrix::translate2f(Float2 const &v) {
   Matrix out = Matrix::identity();
   out[12] = v.x;
   out[13] = v.y;
   return out;
}
Matrix Matrix::translate3f(Float3 const &v) {
   Matrix out = Matrix::identity();
   out[12] = v.x;
   out[13] = v.y;
   out[14] = v.z;
   return out;
}
Matrix Matrix::invert(Matrix const &m) {
   Matrix inv;
   float det;
   int i;

   inv[0] = m[5] * m[10] * m[15] -
      m[5] * m[11] * m[14] -
      m[9] * m[6] * m[15] +
      m[9] * m[7] * m[14] +
      m[13] * m[6] * m[11] -
      m[13] * m[7] * m[10];

   inv[4] = -m[4] * m[10] * m[15] +
      m[4] * m[11] * m[14] +
      m[8] * m[6] * m[15] -
      m[8] * m[7] * m[14] -
      m[12] * m[6] * m[11] +
      m[12] * m[7] * m[10];

   inv[8] = m[4] * m[9] * m[15] -
      m[4] * m[11] * m[13] -
      m[8] * m[5] * m[15] +
      m[8] * m[7] * m[13] +
      m[12] * m[5] * m[11] -
      m[12] * m[7] * m[9];

   inv[12] = -m[4] * m[9] * m[14] +
      m[4] * m[10] * m[13] +
      m[8] * m[5] * m[14] -
      m[8] * m[6] * m[13] -
      m[12] * m[5] * m[10] +
      m[12] * m[6] * m[9];

   inv[1] = -m[1] * m[10] * m[15] +
      m[1] * m[11] * m[14] +
      m[9] * m[2] * m[15] -
      m[9] * m[3] * m[14] -
      m[13] * m[2] * m[11] +
      m[13] * m[3] * m[10];

   inv[5] = m[0] * m[10] * m[15] -
      m[0] * m[11] * m[14] -
      m[8] * m[2] * m[15] +
      m[8] * m[3] * m[14] +
      m[12] * m[2] * m[11] -
      m[12] * m[3] * m[10];

   inv[9] = -m[0] * m[9] * m[15] +
      m[0] * m[11] * m[13] +
      m[8] * m[1] * m[15] -
      m[8] * m[3] * m[13] -
      m[12] * m[1] * m[11] +
      m[12] * m[3] * m[9];

   inv[13] = m[0] * m[9] * m[14] -
      m[0] * m[10] * m[13] -
      m[8] * m[1] * m[14] +
      m[8] * m[2] * m[13] +
      m[12] * m[1] * m[10] -
      m[12] * m[2] * m[9];

   inv[2] = m[1] * m[6] * m[15] -
      m[1] * m[7] * m[14] -
      m[5] * m[2] * m[15] +
      m[5] * m[3] * m[14] +
      m[13] * m[2] * m[7] -
      m[13] * m[3] * m[6];

   inv[6] = -m[0] * m[6] * m[15] +
      m[0] * m[7] * m[14] +
      m[4] * m[2] * m[15] -
      m[4] * m[3] * m[14] -
      m[12] * m[2] * m[7] +
      m[12] * m[3] * m[6];

   inv[10] = m[0] * m[5] * m[15] -
      m[0] * m[7] * m[13] -
      m[4] * m[1] * m[15] +
      m[4] * m[3] * m[13] +
      m[12] * m[1] * m[7] -
      m[12] * m[3] * m[5];

   inv[14] = -m[0] * m[5] * m[14] +
      m[0] * m[6] * m[13] +
      m[4] * m[1] * m[14] -
      m[4] * m[2] * m[13] -
      m[12] * m[1] * m[6] +
      m[12] * m[2] * m[5];

   inv[3] = -m[1] * m[6] * m[11] +
      m[1] * m[7] * m[10] +
      m[5] * m[2] * m[11] -
      m[5] * m[3] * m[10] -
      m[9] * m[2] * m[7] +
      m[9] * m[3] * m[6];

   inv[7] = m[0] * m[6] * m[11] -
      m[0] * m[7] * m[10] -
      m[4] * m[2] * m[11] +
      m[4] * m[3] * m[10] +
      m[8] * m[2] * m[7] -
      m[8] * m[3] * m[6];

   inv[11] = -m[0] * m[5] * m[11] +
      m[0] * m[7] * m[9] +
      m[4] * m[1] * m[11] -
      m[4] * m[3] * m[9] -
      m[8] * m[1] * m[7] +
      m[8] * m[3] * m[5];

   inv[15] = m[0] * m[5] * m[10] -
      m[0] * m[6] * m[9] -
      m[4] * m[1] * m[10] +
      m[4] * m[2] * m[9] +
      m[8] * m[1] * m[6] -
      m[8] * m[2] * m[5];

   det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
   det = 1.0f / det;

   Matrix out = { 0 };
   if (det == 0) {
      return out;
   }

   for (i = 0; i < 16; i++) {
      out[i] = inv[i] * det;
   }

   return out;
}

Matrix Matrix::transpose(Matrix const &m) {
   Matrix out;

   out[0] = m[0];
   out[1] = m[4];
   out[2] = m[8];
   out[3] = m[12];
   out[4] = m[1];
   out[5] = m[5];
   out[6] = m[9];
   out[7] = m[13];
   out[8] = m[2];
   out[9] = m[6];
   out[10] = m[10];
   out[11] = m[14];
   out[12] = m[3];
   out[13] = m[7];
   out[14] = m[11];
   out[15] = m[15];

   return out;
}

Matrix &Matrix::operator*=(Matrix const &rhs) {
   *this = *this * rhs;
   return *this;
}
Matrix Matrix::operator*(Matrix const &rhs) const {
   Matrix out;
   int x, y, i;
   for (y = 0; y < 4; ++y)
   {
      float v1[4] = { this->data[y], this->data[y + 4], this->data[y + 8], this->data[y + 12] };

      for (x = 0; x < 4; ++x)
      {
         const float *v2 = &(rhs[x * 4]);

         float v = 0.0f;
         for (i = 0; i < 4; ++i)
            v += v1[i] * v2[i];

         out[x * 4 + y] = v;
      }
   }

   return out;
}

Float3 Matrix::operator*(Float3 const &rhs) const {
   Float3 out;

   out.x = vDot(rhs, { data[0] , data[4] , data[8] });
   out.y = vDot(rhs, { data[1] , data[5] , data[9] });
   out.z = vDot(rhs, { data[2] , data[6] , data[10] });

   return out;
}

float &Matrix::operator[](size_t index) {
   return data[index];
}
float const &Matrix::operator[](size_t index) const {
   return data[index];
}

static float s2lin(float x) {
   if (x <= 0.04045f) {
      return x * (1.0f / 12.92f);
   }
   else {
      return powf((x + 0.055f) * (1.0f / 1.055f), 2.4f);
   }
}

static float lin2s(float x) {
   if (x <= 0.0031308f) {
      return x * 12.92f;
   }
   else {
      return 1.055f * pow(x, 1.0f / 2.4f) - 0.055f;
   }
}

static float i255 = 1.0f / 255.0f;

ColorRGBAf sRgbToLinear(ColorRGBAf const& srgb) {
   return { s2lin(srgb.r), s2lin(srgb.g), s2lin(srgb.b), srgb.a };
}
ColorRGBAf sRgbToLinear(ColorRGBf const& srgb) {
   return { s2lin(srgb.r), s2lin(srgb.g), s2lin(srgb.b), 1.0f };
}
ColorRGBAf sRgbToLinear(ColorRGBA const& srgb){
   return { s2lin(srgb.r * i255), s2lin(srgb.g * i255), s2lin(srgb.b * i255), srgb.a * i255 };
}
ColorRGBAf sRgbToLinear(ColorRGB const& srgb) {
   return { s2lin(srgb.r * i255), s2lin(srgb.g * i255), s2lin(srgb.b * i255), 1.0f };
}

ColorRGBAf colorLerp(ColorRGBAf const& a, ColorRGBAf const& b, float t) {
   return { 
      clamp(a.r + lerp(a.r, b.r, t), 0.0f, 1.0f),
      clamp(a.g + lerp(a.g, b.g, t), 0.0f, 1.0f),
      clamp(a.b + lerp(a.b, b.b, t), 0.0f, 1.0f),
      clamp(a.a + lerp(a.a, b.a, t), 0.0f, 1.0f) };
}

ColorRGBA linearToSrgb(ColorRGBAf const& lin) {
   return { 
      (byte)(lin2s(lin.r) * 255), 
      (byte)(lin2s(lin.g) * 255),
      (byte)(lin2s(lin.b) * 255),
      (byte)(lin.a * 255) };
}

ColorRGBAf linearToSrgbf(ColorRGBAf const& lin) {
   return { lin2s(lin.r), lin2s(lin.g), lin2s(lin.b), lin.a };
}

ColorRGBA srgbPremultipleAlpha(ColorRGBA const& srgb) {
   auto alphaScale = srgb.a * i255;

   return 
   {  (byte)(lin2s(s2lin(srgb.r * i255) * alphaScale) * 255),
      (byte)(lin2s(s2lin(srgb.g * i255) * alphaScale) * 255),
      (byte)(lin2s(s2lin(srgb.b * i255) * alphaScale) * 255),
      srgb.a,
      };
}

// Intersects ray r = p + td, |d| = 1, with sphere co,cr and, if intersecting,
// returns t value of intersection and intersection point q
int intersectRaySphere(Float2 p, Float2 d, Float2 co, float cr, float& t, Float2& q) {
   auto m = p - co;
   auto b = v2Dot(m, d);
   auto c = v2Dot(m, m) - cr * cr;

   // exit if r's origin outside s (c>0) and r pointing away from s (b>0)
   if (c > 0.0f && b > 0.0f) return 0;
   auto discr = b * b - c;
   // a negative discriminant corresponds to ray missing sphere
   if (discr < 0.0f) return 0;
   // ray now found to intersect sphere, compute smallest t value of intersection
   t = -b - sqrtf(discr);
   // if t is negative, ray started inside sphere so clamp t to zero
   if (t < 0.0f)  t = 0.0f;
   q = p + d * t;

   return 1;
}

float clamp(float f, float min, float max) {
   return f > max ? max : (f < min ? min : f);
}

// interps
float lerp(float v1, float v2, float t) {
   return (v2 - v1) * t;
}
float cosInterp(float t) {
   return (1.0f - cosf(t * PI)) / 2;
}
float cosInterp(float v1, float v2, float t) {
   float t2 = cosInterp(t);
   return lerp(v1, v2, t2);
}
float smoothstep(float v1, float v2, float t) {
   t = clamp((t - v1) / (v2 - v1), 0.0f, 1.0f);
   return t * t * (3.0f - 2.0f * t);
}