#include "reflection.h"

TypeMetadata const* meta_bool;
TypeMetadata const* meta_byte;
TypeMetadata const* meta_sbyte;
TypeMetadata const* meta_i16;
TypeMetadata const* meta_i32;
TypeMetadata const* meta_i64;
TypeMetadata const* meta_u16;
TypeMetadata const* meta_u32;
TypeMetadata const* meta_u64;
TypeMetadata const* meta_f32;
TypeMetadata const* meta_f64;
TypeMetadata const* meta_string;
TypeMetadata const* meta_symbol;

template <typename T>
static TypeMetadata const* _createBasicType(Symbol* name) {
   TypeMetadata out = { 0 };
   out.name = name;
   out.size = sizeof(T);
   out.variety = TypeVariety_Basic;

   return new TypeMetadata(out);
}

template <typename T>
static TypeMetadata const* _createBasicTypeNonTrivial(Symbol* name) {
   TypeMetadata out = { 0 };
   out.name = name;
   out.size = sizeof(T);
   out.variety = TypeVariety_Basic;

   out.funcs.create = [](void* data) {new (data) T; };
   out.funcs.destroy = [](void* data) {((T*)data)->~T(); };

   return new TypeMetadata(out);
}

#define BASIC_TYPE_CREATE(Type) _createBasicType<Type>(intern(#Type))
#define BASIC_TYPE_CREATE_NON_TRIVIAL(Type) _createBasicTypeNonTrivial<Type>(intern(#Type))

static void _createBasicTypes() {
   meta_bool = BASIC_TYPE_CREATE(bool);

   meta_byte = BASIC_TYPE_CREATE(byte);
   meta_sbyte = BASIC_TYPE_CREATE(char);

   meta_i16 = BASIC_TYPE_CREATE(int16_t);
   meta_i32 = BASIC_TYPE_CREATE(int32_t);
   meta_i64 = BASIC_TYPE_CREATE(int64_t);

   meta_u16 = BASIC_TYPE_CREATE(uint16_t);
   meta_u32 = BASIC_TYPE_CREATE(uint32_t);
   meta_u64 = BASIC_TYPE_CREATE(uint64_t);

   meta_f32 = BASIC_TYPE_CREATE(float);
   meta_f64 = BASIC_TYPE_CREATE(double);

   meta_string = BASIC_TYPE_CREATE_NON_TRIVIAL(std::string);
   meta_symbol = BASIC_TYPE_CREATE(Symbol*);
}

void reflectionStartup() {
   _createBasicTypes();
}