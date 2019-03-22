#pragma once

#include "defs.h"
#include "scf.h"
#include "custom_ui_renders.h"




void reflectionStartup();

enum TypeVariety {
   TypeVariety_Basic,
   TypeVariety_Struct,
   TypeVariety_Enum,
   TypeVariety_Array,
   TypeVariety_KVP,
};

enum StructMemberFlags_ {
   StructMemberFlags_File =         (1 << 0),
   StructMemberFlags_Image =        (1 << 1),
   StructMemberFlags_StaticArray =  (1 << 2),
   StructMemberFlags_ReadOnly =     (1 << 3),
};
typedef byte StructMemberFlags;

enum EnumFlags_ {
   EnumFlags_Bitfield = (1 << 0)
};
typedef byte EnumFlags;

typedef struct TypeMetadata TypeMetadata;


struct StructMemberUIOptions {
   float min = 0, max = 0, step = 0;
};

struct StructMemberMetadata {
   Symbol* name;
   size_t offset;
   TypeMetadata const* type;
   UIRenderFunc customUI = nullptr;
   StructMemberFlags flags = 0;
   size_t staticArraySize = 0;
   StructMemberUIOptions ui;

   bool reference = false;
   TypeMetadata const* referenceOwnerType = nullptr;
   void* referenceOwner = nullptr;
   Symbol* referenceKeyMember = nullptr;
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
   bool(*retrieveKVP)(void* data, void* key, void** target) = nullptr;

   Array<void*> (*listKVPKeys)(void* data) = nullptr;

   void(*serialize)(SCFWriter* writer, void* data) = nullptr;
   void(*deserialize)(SCFReader& reader, void* target) = nullptr;

   bool(*doUI)(void* data, StructMemberMetadata const* parent, const char* label);
};

struct TypeMetadata {
   size_t size;
   Symbol* name;

   TypeVariety variety;

   Array<StructMemberMetadata> structMembers;
   Array<EnumEntryMetadata> enumEntries;
   EnumFlags enumFlags = 0;

   TypeMetadata const* key;
   TypeMetadata const* value;

   TypeMetadataFunctions funcs;
};

static const char* TypePayload = "TypeMetaData";

struct MetadataPayload {
   TypeMetadata const* metadata;
   void* data;
};

TypeMetadata const*typeMetadataImGuiPayloadPeek();
void* typeMetadataImGuiPayloadAcceptEX(TypeMetadata const* type);

// used by ui
StructMemberMetadata* typeMetadataGetMemberById(TypeMetadata const* type, Symbol* id);
void* getReflectedMember(void* data, TypeMetadata const* type, Symbol* id);

void serializeEX(SCFWriter* writer, TypeMetadata const* type, void* data);
void deserializeEX(SCFReader& reader, TypeMetadata const* type, void* target);

bool doTypeUIEX(TypeMetadata const* type, void* data, StructMemberMetadata const* parent = nullptr, const char* label = nullptr);

template<typename T>
struct Reflector {
   static TypeMetadata const* type() { return nullptr; }
};

template<typename T>
TypeMetadata const*reflect() { return Reflector<T>::type(); }

template<typename T>
TypeMetadata const*reflectFromRef(T const&) { return Reflector<T>::type(); }

template<typename T>
bool doTypeUI(T*data, const char* label = nullptr) {
   return doTypeUIEX(reflect<T>(), data, nullptr, label);
}

template<typename T>
void serialize(SCFWriter* writer, T* data) {
   serializeEX(writer, reflect<T>(), data);
}

template<typename T>
void deserialize(SCFReader& reader, T* target) {
   deserializeEX(reader, reflect<T>(), target);
}

template<typename T>
void typeMetadataImGuiPayloadSet(T* t) {
   MetadataPayload pload;
   pload.metadata = reflect<T>();
   pload.data = t;
   ImGui::SetDragDropPayload(TypePayload, &pload, sizeof(MetadataPayload), ImGuiCond_Once);
}

template<typename T>
T* typeMetadataImGuiPayloadAccept() {
   return (T*)typeMetadataImGuiPayloadAcceptEX(reflect<T>());
}



#define BASIC_TYPE_REFLECT(c_type, metaname) \
extern TypeMetadata const* metaname; \
template<> \
struct Reflector<c_type> { \
   static TypeMetadata const* type() { return metaname; } \
};

BASIC_TYPE_REFLECT(Blob, meta_blob)
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
struct Reflector<Array<T>> {
   static TypeMetadata const* type() { 
      static auto out = _createArrayMetadata();
      return out; 
   } 
private:
   typedef Array<T> ThisType;
   static TypeMetadata const* _createArrayMetadata() {
      if (TypeMetadata const*  innerType = reflect<T>()) {
         TypeMetadata out;
         out.variety = TypeVariety_Array;
         out.name = intern(format("Array<>"/*, innerType->name*/).c_str());
         out.value = innerType;
         out.size = sizeof(Array<T>);

         //out.funcs.create = [](void* data) { new(data) ThisType(); };
         //out.funcs.destroy = [](void* data) { ((ThisType*)data)->~ThisType(); };
         //out.funcs.clear = [](void* self) { ((ThisType*)self)->clear(); };
         //out.funcs.insert = [](void* self, void* obj) { ((ThisType*)self)->push_back(*(T*)obj); };

         out.funcs.serialize = [](SCFWriter* writer, void* data) {
            scfWriteListBegin(writer);
            for (auto&& member : *((ThisType*)data)) {
               serializeEX(writer, reflect<T>(), &member);
            }
            scfWriteListEnd(writer);
         };

         out.funcs.deserialize = [](SCFReader& reader, void* target) {
            auto arr = scfReadList(reader);
            while (!scfReaderAtEnd(arr)) {
               T obj;
               deserializeEX(arr, reflect<T>(), &obj);
               ((ThisType*)target)->push_back(std::move(obj));
            }
         };

         out.funcs.doUI = [](void* data, StructMemberMetadata const* parent, const char* label) {
            bool changed = false;
            if (ImGui::CollapsingHeader(label ? label : parent->name, ImGuiTreeNodeFlags_AllowItemOverlap)) {
               ImGui::SameLine();
               if (ImGui::Button(ICON_FA_PLUS)) {
                  ((ThisType*)data)->push_back(T());
               }
               ImGui::Indent();
               int idx = 0;
               for (auto&&item : *((ThisType*)data)) {
                  if (doTypeUIEX(reflect<T>(), &item, parent, std::to_string(idx++).c_str())) changed = true;
               }
               ImGui::Unindent();
            }
            return changed;
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
            out.name = intern(format("std::unordered_map<>"/*, keyType->name, valType->name*/).c_str());
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
                  serializeEX(writer, reflect<K>(), (void*)&member.first);
                  serializeEX(writer, reflect<V>(), (void*)&member.second);
                  scfWriteListEnd(writer);
               }
               scfWriteListEnd(writer);
            };

            out.funcs.deserialize = [](SCFReader& reader, void* target) {
               auto arr = scfReadList(reader);
               while (!scfReaderAtEnd(arr)) {
                  auto kvp = scfReadList(arr);

                  K key;
                  V value;
                  deserializeEX(kvp, reflect<K>(), &key);
                  deserializeEX(kvp, reflect<V>(), &value);
                  ((ThisType*)target)->insert({ key,  value });
               }
            };

            out.funcs.retrieveKVP = [](void* data, void* key, void** target) {
               auto &thisObj = *((ThisType*)data);
               
               auto search = thisObj.find(*(K*)key);
               if (search == thisObj.end()) {
                  *target = nullptr;
                  return false;
               }

               *(V**)(target) = &search->second;
               return true;
               
            };

            out.funcs.listKVPKeys = [](void* data)->Array<void*> {
               auto &thisObj = *((ThisType*)data);
               Array<void*> out;
               for (auto&&kvp : thisObj) {
                  out.push_back((void*)&kvp.first);
               }
               return out;
            };

            out.funcs.doUI = [](void* data, StructMemberMetadata const* parent, const char* label) {
               bool changed = false;
               auto &thisObj = *(ThisType*)data;

               if (ImGui::CollapsingHeader(label ? label : parent->name, ImGuiTreeNodeFlags_AllowItemOverlap)) {
                  ImGui::SameLine();
                  static K newkey;
                  if (ImGui::Button(ICON_FA_PLUS)) {
                     ImGui::OpenPopup("newkey");  
                     newkey = {};
                  }

                  if (ImGui::BeginPopupModal("newkey")) {

                     doTypeUIEX(reflect<K>(), &newkey, nullptr, "key");
                     if (ImGui::Button("OK")) {
                        thisObj.insert({ newkey, V{} });
                        ImGui::CloseCurrentPopup();
                     }

                     ImGui::EndPopup();
                  }
                  ImGui::Indent();

                  Array<K*> keyList;
                  for (auto&&kvp : thisObj) {
                     keyList.push_back((K*)&kvp.first);
                  }
                  if (reflect<K>() == reflect<Symbol*>()) {
                     std::sort(keyList.begin(), keyList.end(), []( K*a, K*b) {return natstrcmp(*a, *b) < 0; });
                  }
                  
                  for (auto&&k : keyList) {
                     if (reflect<K>() == reflect<Symbol*>()) {
                        if (doTypeUIEX(reflect<V>(), &thisObj[*k], parent, (StringView)*k)) changed = true;
                     }
                  }
                  ImGui::Unindent();
               }
               return changed;
            };

            return new TypeMetadata(out);
         }
      }
      return nullptr;
   }
};