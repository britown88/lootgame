#pragma once




typedef uint8_t   byte;

struct Blob {
   void* data = nullptr;
   size_t sz = 0;

   operator bool() {
      return data;
   }
};

Blob blobCreate(byte const* data, size_t sz);
void blobDestroy(Blob& self);

typedef const char *StringView;
typedef const char Symbol;

Symbol* intern(StringView str);
std::string format(StringView fmt, ...);

/**
* standard C natural string compare
* @param s1 left string
* @param s2 right string
* @return -1 when s1 < s2, 0 when s1 == s2, 1 when s1 > s2
*/
int natstrcmp(const char* s1, const char* s2);

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
//#define vecNDef ((uint32_t)-1)

//time
typedef uint64_t Microseconds;
typedef uint64_t Milliseconds;
typedef uint32_t Seconds;

#pragma pack(push, 1)
//@reflect{
struct Time {
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
};//}
#pragma pack(pop)

static Time timeMicros(Microseconds t) { return Time{ t }; }
static Time timeMillis(Milliseconds t) { return Time{ t * 1000 }; }
static Time timeSecs(Seconds t) { return Time{ t * 1000000 }; }

static Microseconds timeGetMicros(Time t) { return t.t; }
static Milliseconds timeGetMillis(Time t) { return t.t / 1000; }
static Seconds timeGetSecs(Time t) { return (Seconds)(t.t / 1000000); }

//colors
#pragma pack(push, 1)
//@reflect{
struct ColorRGB {
   byte r, g, b;
};//}
//@reflect{
struct ColorRGBA {
   byte r, g, b, a;
};//}
//@reflect{
struct ColorRGBf {
   float r, g, b;
};//}
//@reflect{
struct ColorHSV {
   float h, s, v;
};//}
//@reflect{
struct ColorRGBAf {
   float r, g, b, a;
};//}
#pragma pack(pop)

extern const ColorRGBAf Cleared;
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

//defer macro.
#define __DEFER( line ) defer__ ## line
#define _DEFER( line ) __DEFER( line )

//usage: DEFER {/* this will get executed at scope end. */};
#define DEFER auto _DEFER( __LINE__ ) = _def_deferDummy{} * [ & ]

namespace _defs {
   template< class T > struct remove_reference { typedef T type; };
   template< class T > struct remove_reference<T&> { typedef T type; };
   template< class T > struct remove_reference<T&&> { typedef T type; };

   template< class T >
   constexpr T&& forward(typename remove_reference<T>::type& t) noexcept { return (T&&)t; }

   template< class T >
   constexpr T&& forward(typename remove_reference<T>::type&& t) noexcept { return (T&&)t; }
}


template <typename F>
struct _def_Defer_Impl {
   _def_Defer_Impl(F f) : f(f) {}
   ~_def_Defer_Impl() { f(); }
   F f;
};

template <typename F>
_def_Defer_Impl<F> _def_makeDefer(F f) {
   return _def_Defer_Impl<F>(f);
};

struct _def_deferDummy { };
template<typename F>
_def_Defer_Impl<F> operator*(_def_deferDummy, F&& f)
{
   return _def_makeDefer<F>(_defs::forward<F>(f));
}