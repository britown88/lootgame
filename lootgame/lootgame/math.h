#pragma once
#include "defs.h"

static const float PI = 3.14159265359f;
static const float RAD2DEG = 180.0f / PI;
static const float DEG2RAD = PI / 180.0f;

typedef struct {
   i32 x, y;
} Int2;

typedef struct {
   i32 x, y, z;
} Int3;

typedef struct {
   f32 x, y;
} Float2;

typedef struct {
   f32 x, y, z;
} Float3;

Float3 vCross(Float3 a, Float3 b);
f32 vDot(Float3 a, Float3 b);
Float3 vSubtract(Float3 a, Float3 b);
Float3 vAdd(Float3 a, Float3 b);
Float3 vNormalized(Float3 v);
Float3 vScale(Float3 v, f32 s);
f32 vDistSquared(Float3 const& a, Float3 const& b);

typedef struct {
   i32 x, y, w, h;
} Recti;

static void rectiOffset(Recti *r, i32 x, i32 y) {
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

Recti getProportionallyFitRect(Float2 srcSize, Float2 destSize);
Recti getProportionallyFitRect(Int2 srcSize, Int2 destSize);



Float3 srgbToLinear(ColorRGB const&srgb);
Float3 srgbToLinear(ColorRGBA const&srgb);

ColorHSV srgbToHSV(ColorRGB const&srgb);

struct  Matrix {
   float data[16];
public:

   static Matrix identity();
   static Matrix ortho(float left, float right, float bottom, float top, float near, float far);
   static Matrix perspective(float fovy, float aspect, float zNear, float zFar);
   static Matrix lookAt(Float3 const &eye, Float3 const &center, Float3 const &up);
   static Matrix rotate2D(float degrees);
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