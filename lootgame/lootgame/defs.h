#pragma once

#include <stdint.h>
#include <string>



typedef uint8_t   byte;
typedef int8_t    sbyte;
typedef int16_t   i16;
typedef uint16_t  u16;
typedef int32_t   i32;
typedef uint32_t  u32;
typedef int64_t   i64;
typedef uint64_t  u64;
typedef intptr_t  iPtr;
typedef uintptr_t uPtr;
typedef float     f32;
typedef double    f64;

typedef const char *StringView;
typedef const char *InternString;
typedef InternString Symbol;

Symbol intern(StringView str);
std::string format(StringView fmt, ...);

#ifndef __cplusplus
typedef byte      bool;
#define true 1
#define false 0
#endif

#define EMPTY_STRUCT byte __UNUSED

#ifdef _DEBUG 
#define ASSERT(...) if(!(__VA_ARGS__)){ byte a = *(byte*)0; }
#else
#define ASSERT(...) 
#endif

#define SIGN(expr) ((expr==0)?0:((expr>0)?1:-1))
#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))
#define LEN(a) (sizeof(a)/sizeof(a)[0])

//sentinel values
//#define vecNDef ((u32)-1)

//time
typedef u64 Microseconds;
typedef u64 Milliseconds;
typedef u32 Seconds;

#pragma pack(push, 1)
typedef struct Time {
   Microseconds t;

   Microseconds toMicroseconds() { return t; };
   Milliseconds toMilliseconds() { return t / 1000; };
   Seconds toSeconds() { return (Seconds)(t / 1000000); };

   Time operator-(Time const&lhs) { return { t - lhs.t }; }
   Time operator+(Time const&lhs) { return { t + lhs.t }; }

   Time& operator-=(Time const&lhs) { t -= lhs.t; return *this; }
   Time& operator+=(Time const&lhs) { t += lhs.t; return *this; }

   bool operator>(Time const&lhs) { return t > lhs.t; }
   bool operator<(Time const&lhs) { return t < lhs.t; }
   bool operator>=(Time const&lhs) { return t >= lhs.t; }
   bool operator<=(Time const&lhs) { return t <= lhs.t; }
} Time;
#pragma pack(pop)

static Time timeMicros(Microseconds t) { return Time{ t }; }
static Time timeMillis(Milliseconds t) { return Time{ t * 1000 }; }
static Time timeSecs(Seconds t) { return Time{ t * 1000000 }; }

static Microseconds timeGetMicros(Time t) { return t.t; }
static Milliseconds timeGetMillis(Time t) { return t.t / 1000; }
static Seconds timeGetSecs(Time t) { return (Seconds)(t.t / 1000000); }

//colors
#pragma pack(push, 1)
typedef struct {
   byte r, g, b;
} ColorRGB;

typedef struct {
   byte r, g, b, a;
} ColorRGBA;

typedef struct {
   f32 r, g, b;
} ColorRGBf;

typedef struct {
   f32 h, s, v;
} ColorHSV;

typedef struct {
   f32 r, g, b, a;
} ColorRGBAf;
#pragma pack(pop)

extern const ColorRGBAf White;
extern const ColorRGBAf Gray;
extern const ColorRGBAf DkGray;
extern const ColorRGBAf LtGray;
extern const ColorRGBAf Black;
extern const ColorRGBAf Red;
extern const ColorRGBAf DkRed;
extern const ColorRGBAf Green;
extern const ColorRGBAf DkGreen;
extern const ColorRGBAf Blue;
extern const ColorRGBAf DkBlue;
extern const ColorRGBAf Cyan;
extern const ColorRGBAf Yellow;
extern const ColorRGBAf Magenta;
