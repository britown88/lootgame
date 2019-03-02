#pragma once
#include "defs.h"

static const float PI = 3.14159265359f;
static const float RAD2DEG = 180.0f / PI;
static const float DEG2RAD = PI / 180.0f;

//@reflect{
struct Int2 {
   int32_t x, y;
};//}

//@reflect{
struct Int3 {
   int32_t x, y, z;
};//}

//@reflect{
struct Float2 {
   float x = 0, y = 0;

   inline Float2 operator-(Float2 lhs) const { return { x - lhs.x, y - lhs.y }; }
   inline Float2 operator-() const { return { -x, -y }; }
   inline Float2 operator+(Float2 lhs) const { return { x + lhs.x, y + lhs.y }; }
   inline Float2 operator*(float s) const { return { x * s, y * s }; }
   inline Float2 operator/(float s) const { return { x / s, y /s }; }
   inline Float2 operator/(Float2 lhs) const { return { x / lhs.x, y / lhs.y }; }

   inline Float2& operator-=(Float2 lhs) { x -= lhs.x; y -= lhs.y; return *this; }
   inline Float2& operator+=(Float2 lhs) { x += lhs.x; y += lhs.y; return *this; }
   inline Float2& operator/=(float s) { x /= s; y /= s; return *this; }
   inline Float2& operator*=(float s) { x *= s; y *= s; return *this; }
};//}

//@reflect{
struct Float3 {
   float x, y, z;
};//}

Float3 vCross(Float3 a, Float3 b);
float vDot(Float3 a, Float3 b);
Float3 vSubtract(Float3 a, Float3 b);
Float3 vAdd(Float3 a, Float3 b);
Float3 vNormalized(Float3 v);
Float3 vScale(Float3 v, float s);

float v2Dot(Float2 a, Float2 b);
float v2Dist(Float2 a, Float2 b);
float v2DistSquared(Float2 a, Float2 b);
float v2Len(Float2 v);
float v2LenSquared(Float2 v);
Float2 v2Subtract(Float2 a, Float2 b);
Float2 v2Add(Float2 a, Float2 b);
Float2 v2Normalized(Float2 v);
Float2 v2Scale(Float2 v, float s);

float v2Determinant(Float2 a, Float2 b); //helper.  determines orientation of two vectors, positive vs. negative means clockwise/counterclockwise orientation
Float2 v2Rotate(Float2 direction, Float2 rotation); //complex number rotation!!
Float2 v2FromAngle(float radians);
Float2 v2Conjugate(Float2 unit);
float v2Angle(Float2 v);
Float2 v2Orthogonal(Float2 v);
Float2 v2CapLength(Float2 v, float max);

//direction is the current direction, target is the target direction
//perFrame is Float2{cos(anglePerFrame), sin(anglePerFrame)} - this can be precalculated, to avoid runtime trig
Float2 v2RotateTowards(Float2 direction, Float2 target, Float2 perFrame);
Float2 v2MoveTowards(Float2 position, Float2 target, float speed);

//@reflect{
struct Recti{
   int32_t x, y, w, h;
};//}

//@reflect{
struct Rectf{
   float x, y, w, h;

   void setPos(Float2 const& p) { x = p.x; y = p.y; }

   Float2 xy() const { return { x,y }; }
   Float2 sz() const { return { w,h }; }
   Float2 min() const { return { x,y }; }
   Float2 max() const { return { x+w,y+h }; }
};//}

static void rectiOffset(Recti *r, int32_t x, int32_t y) {
   r->x += x;
   r->y += y;
}

static bool rectiContains(Recti r, Int2 p) {
   if (p.x < r.x ||
      p.y < r.y ||
      p.x >= r.x + r.w ||
      p.y >= r.y + r.h) return false;
   return true;
}

static bool rectiIntersects(Recti a, Recti b) {
   if (a.x >= b.x + b.w ||
      a.y >= b.y + b.h ||
      b.x >= a.x + a.w ||
      b.y >= a.y + a.h) return false;
   return true;
}

// collisions
bool circleVsAabb(Float2 co, float cr, Rectf const& aabb);

Recti getProportionallyFitRect(Float2 srcSize, Float2 destSize);
Recti getProportionallyFitRect(Int2 srcSize, Int2 destSize);


ColorRGBAf sRgbToLinear(ColorRGBAf const& srgb);
ColorRGBAf sRgbToLinear(ColorRGBf const& srgb);
ColorRGBAf sRgbToLinear(ColorRGBA const& srgb);
ColorRGBAf sRgbToLinear(ColorRGB const& srgb);

ColorRGBA linearToSrgb(ColorRGBAf const& lin);
ColorRGBA srgbPremultipleAlpha(ColorRGBA const& srgb);

struct  Matrix {
   float data[16];
public:

   static Matrix identity();
   static Matrix ortho(float left, float right, float bottom, float top, float near, float far);
   static Matrix perspective(float fovy, float aspect, float zNear, float zFar);
   static Matrix lookAt(Float3 const &eye, Float3 const &center, Float3 const &up);
   static Matrix rotate2D(float rads);
   static Matrix scale2f(Float2 const &v);
   static Matrix scale3f(Float3 const &v);
   static Matrix translate2f(Float2 const &v);
   static Matrix translate3f(Float3 const &v);
   static Matrix fromBasis(Float3 const &v1, Float3 const &v2, Float3 const &v3);
   static Matrix invert(Matrix const &m);
   static Matrix transpose(Matrix const &m);


   Matrix &operator*=(Matrix const &rhs);
   Matrix operator*(Matrix const &rhs) const;

   Float3 operator*(Float3 const &rhs) const;

   float &operator[](size_t index);
   float const &operator[](size_t index) const;

};

// Intersects ray r = p + td, |d| = 1, with sphere co,cr and, if intersecting,
// returns t value of intersection and intersection point q
int intersectRaySphere(Float2 p, Float2 d, Float2 co, float cr, float& t, Float2& q);

float clamp(float f, float min, float max);

// interps
float lerp(float v1, float v2, float t);
float cosInterp(float t);
float cosInterp(float v1, float v2, float t);
float smoothstep(float v1, float v2, float t);
