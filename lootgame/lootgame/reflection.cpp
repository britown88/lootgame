#include "reflection.h"

#include "scf.h"

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

void serialize(SCFWriter* writer, TypeMetadata const* type, void* data) {
   switch (type->variety) {
   case TypeVariety_Basic:
      if      (type == meta_bool)   scfWriteInt(writer, *(bool*)data);
      else if (type == meta_byte)   scfWriteInt(writer, *(byte*)data);
      else if (type == meta_sbyte)  scfWriteInt(writer, *(char*)data);
      else if (type == meta_i16)    scfWriteInt(writer, *(int16_t*)data);
      else if (type == meta_i32)    scfWriteInt(writer, *(int32_t*)data);
      else if (type == meta_i64)    scfWriteInt(writer, *(int64_t*)data);
      else if (type == meta_u16)    scfWriteInt(writer, *(uint16_t*)data);
      else if (type == meta_u32)    scfWriteInt(writer, *(uint32_t*)data);
      else if (type == meta_u64)    scfWriteInt(writer, *(uint64_t*)data);
      else if (type == meta_f32)    scfWriteFloat(writer, *(float*)data);
      else if (type == meta_f64)    scfWriteDouble(writer, *(double*)data);
      else if (type == meta_string) scfWriteString(writer, ((std::string*)data)->c_str());
      else if (type == meta_symbol) scfWriteString(writer, *(StringView*)data);
      break;
   case TypeVariety_Struct:
      scfWriteListBegin(writer);
      for (auto&& member : type->structMembers) {
         scfWriteListBegin(writer);
         scfWriteString(writer, member.name);
         serialize(writer, member.type, (byte*)data + member.offset);
         scfWriteListEnd(writer);
      }
      scfWriteListEnd(writer);
      break;
   case TypeVariety_Enum:
      break;
   case TypeVariety_Array:
   case TypeVariety_KVP:
      type->funcs.serialize(writer, data);
      break;
   }

}

void deserialize(SCFReader* reader, TypeMetadata const* type, void* target) {

   switch (type->variety) {
   case TypeVariety_Basic:
      if      (type == meta_bool)   *(bool*)target = (bool)*scfReadInt(*reader);
      else if (type == meta_byte)   *(byte*)target = (byte)*scfReadInt(*reader);
      else if (type == meta_sbyte)  *(char*)target = (char)*scfReadInt(*reader);
      else if (type == meta_i16)    *(int16_t*)target = (int16_t)*scfReadInt(*reader);
      else if (type == meta_i32)    *(int32_t*)target = (int32_t)*scfReadInt(*reader);
      else if (type == meta_i64)    *(int64_t*)target = (int64_t)*scfReadInt(*reader);
      else if (type == meta_u16)    *(uint16_t*)target = (uint16_t)*scfReadInt(*reader);
      else if (type == meta_u32)    *(uint32_t*)target = (uint32_t)*scfReadInt(*reader);
      else if (type == meta_u64)    *(uint64_t*)target = (uint64_t)*scfReadInt(*reader);
      else if (type == meta_f32)    *(float*)target = *scfReadFloat(*reader);
      else if (type == meta_f64)    *(double*)target = *scfReadDouble(*reader);      
      else if (type == meta_string) ((std::string*)target)->assign(scfReadString(*reader));
      else if (type == meta_symbol) *(Symbol**)target = intern(scfReadString(*reader));
      break;
   case TypeVariety_Struct:
      break;
   case TypeVariety_Enum:
      break;
   case TypeVariety_Array:
   case TypeVariety_KVP:
      type->funcs.deserialize(reader, target);
      break;
   }
}

