#pragma once

#include "defs.h"
#include "scf.h"
#include <vector>
#include <string>
#include <unordered_map>

void reflectionStartup();

enum TypeVariety {
   TypeVariety_Basic,
   TypeVariety_Struct,
   TypeVariety_Enum,
   TypeVariety_Array,
   TypeVariety_KVP,
};

typedef struct TypeMetadata TypeMetadata;

struct StructMemberMetadata {
   Symbol* name;
   size_t offset;
   TypeMetadata const* type;
};

struct EnumEntryMetadata {
   Symbol* name;
   int64_t value;
};

typedef struct SCFReader SCFReader;
typedef struct SCFWriter SCFWriter;

struct TypeMetadataFunctions {
   void(*create)(void* data) = nullptr;
   void(*destroy)(void* data) = nullptr;

   void(*clear)(void* data) = nullptr;
   void(*insert)(void* data, void* obj) = nullptr;
   void(*insertKVP)(void* data, void* key, void* value) = nullptr;

   void(*serialize)(SCFWriter* writer, void* data) = nullptr;
   void(*deserialize)(SCFReader* reader, void* target) = nullptr;
};

struct TypeMetadata {
   size_t size;
   Symbol* name;

   TypeVariety variety;

   std::vector<StructMemberMetadata> structMembers;
   std::vector<EnumEntryMetadata> enumEntries;

   TypeMetadata const* key;
   TypeMetadata const* value;

   TypeMetadataFunctions funcs;
};

void serialize(SCFWriter* writer, TypeMetadata const* type, void* data);
void deserialize(SCFReader* reader, TypeMetadata const* type, void* target);

template<typename T>
struct Reflector {
   static TypeMetadata const* type() { return nullptr; }
};

template<typename T>
TypeMetadata const*reflect() { return Reflector<T>::type(); }

#define BASIC_TYPE_REFLECT(c_type, metaname) \
extern TypeMetadata const* metaname; \
template<> \
struct Reflector<c_type> { \
   static TypeMetadata const* type() { return metaname; } \
};

BASIC_TYPE_REFLECT(bool, meta_bool)

BASIC_TYPE_REFLECT(byte, meta_byte)
BASIC_TYPE_REFLECT(char, meta_sbyte)

BASIC_TYPE_REFLECT(int16_t, meta_i16)
BASIC_TYPE_REFLECT(int32_t, meta_i32)
BASIC_TYPE_REFLECT(int64_t, meta_i64)

BASIC_TYPE_REFLECT(uint16_t, meta_u16)
BASIC_TYPE_REFLECT(uint32_t, meta_u32)
BASIC_TYPE_REFLECT(uint64_t, meta_u64)

BASIC_TYPE_REFLECT(float, meta_f32)
BASIC_TYPE_REFLECT(double, meta_f64)

BASIC_TYPE_REFLECT(std::string, meta_string)
BASIC_TYPE_REFLECT(Symbol*, meta_symbol)

#undef BASIC_TYPE_REFLECT

template<typename T>
struct Reflector<std::vector<T>> {
   static TypeMetadata const* type() { 
      static auto out = _createArrayMetadata();
      return out; 
   } 
private:
   typedef std::vector<T> ThisType;
   static TypeMetadata const* _createArrayMetadata() {
      if (TypeMetadata const*  innerType = reflect<T>()) {
         TypeMetadata out;
         out.variety = TypeVariety_Array;
         out.name = intern(format("std::vector<%s>", innerType->name).c_str());
         out.value = innerType;
         out.size = sizeof(std::vector<T>);

         //out.funcs.create = [](void* data) { new(data) ThisType(); };
         //out.funcs.destroy = [](void* data) { ((ThisType*)data)->~ThisType(); };
         //out.funcs.clear = [](void* self) { ((ThisType*)self)->clear(); };
         //out.funcs.insert = [](void* self, void* obj) { ((ThisType*)self)->push_back(*(T*)obj); };

         out.funcs.serialize = [](SCFWriter* writer, void* data) {
            scfWriteListBegin(writer);
            for (auto&& member : *((ThisType*)data)) {
               serialize(writer, reflect<T>(), &member);
            }
            scfWriteListEnd(writer);
         };

         out.funcs.deserialize = [](SCFReader* reader, void* target) {
            auto arr = scfReadList(*reader);
            while (!scfReaderAtEnd(arr)) {
               T obj;
               deserialize(&arr, reflect<T>(), &obj);
               ((ThisType*)target)->push_back(std::move(obj));
            }
         };

         return new TypeMetadata(out);
      }
      return nullptr;      
   }
};

template<typename K, typename V, typename Hash, typename Compare>

struct Reflector<std::unordered_map<K, V, Hash, Compare>> {   
   static TypeMetadata const* type() {
      static auto out = _createHashMapMetadata();
      return out;
   }
private:
   typedef std::unordered_map<K, V, Hash, Compare> ThisType;

   static TypeMetadata const* _createHashMapMetadata() {
      if (TypeMetadata const*  keyType = reflect<K>()) {
         if (TypeMetadata const*  valType = reflect<V>()) {
            TypeMetadata out;
            out.variety = TypeVariety_KVP;
            out.name = intern(format("std::unordered_map<%s, %s>", keyType->name, valType->name).c_str());
            out.key = keyType;
            out.value = valType;
            out.size = sizeof(ThisType);

            //out.funcs.create = [](void* data) { new(data) ThisType(); };
            //out.funcs.destroy = [](void* data) { ((ThisType*)data)->~ThisType(); };
            //out.funcs.clear = [](void* self) { ((ThisType*)self)->clear(); };
            //out.funcs.insertKVP = [](void* self, void* key, void* value) { ((ThisType*)self)->insert({ *(K*)key, *(V*)value }); };

            out.funcs.serialize = [](SCFWriter* writer, void* data) {
               scfWriteListBegin(writer);
               for (auto&& member : *((ThisType*)data)) {
                  scfWriteListBegin(writer);
                  serialize(writer, reflect<K>(), (void*)&member.first);
                  serialize(writer, reflect<V>(), (void*)&member.second);
                  scfWriteListEnd(writer);
               }
               scfWriteListEnd(writer);
            };

            out.funcs.deserialize = [](SCFReader* reader, void* target) {
               auto arr = scfReadList(*reader);
               while (!scfReaderAtEnd(arr)) {
                  auto kvp = scfReadList(arr);

                  K key;
                  V value;
                  deserialize(&kvp, reflect<K>(), &key);
                  deserialize(&kvp, reflect<V>(), &value);
                  ((ThisType*)target)->insert({ key,  value });
               }
            };

            return new TypeMetadata(out);
         }
      }
      return nullptr;
   }
};