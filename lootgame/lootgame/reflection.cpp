#include "stdafx.h"

#include "reflection.h"

#include "scf.h"

#define UNKNOWN_ENUM_ENTRY "__UNKNOWN_ENUM__"

TypeMetadata const* meta_blob;
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
   meta_blob = BASIC_TYPE_CREATE_NON_TRIVIAL(Blob);
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

   void reflectionStartup_generated();
   reflectionStartup_generated();
}

bool compareEnumValue(size_t enumSize, int64_t entryValue, void*data) {
   switch (enumSize) {
   case sizeof(int8_t)  : return *((int8_t*)data) == entryValue;
   case sizeof(int16_t) : return *((int16_t*)data) == entryValue;
   case sizeof(int32_t) : return *((int32_t*)data) == entryValue;
   case sizeof(int64_t) : return *((int64_t*)data) == entryValue;
   default: ASSERT(false);
   }
   return false;
}

bool compareBitfieldValue(size_t enumSize, int64_t entryValue, void*data) {
   switch (enumSize) {
   case sizeof(int8_t) : return (*((int8_t*)data) & (int8_t)entryValue) == (int8_t)entryValue;
   case sizeof(int16_t) : return (*((int16_t*)data) & (int16_t)entryValue) == (int16_t)entryValue;
   case sizeof(int32_t) : return (*((int32_t*)data) & (int32_t)entryValue) == (int32_t)entryValue;
   case sizeof(int64_t) : return (*((int64_t*)data) & (int64_t)entryValue) == (int64_t)entryValue;
   default: ASSERT(false);
   }
   return false;
}

void addBitfieldValue(size_t enumSize, int64_t entryValue, void*data) {
   switch (enumSize) {
   case sizeof(int8_t) : (*((int8_t*)data) |= (int8_t)entryValue); break;
   case sizeof(int16_t) : (*((int16_t*)data) |= (int16_t)entryValue); break;
   case sizeof(int32_t) : (*((int32_t*)data) |= (int32_t)entryValue); break;
   case sizeof(int64_t) : (*((int64_t*)data) |= (int64_t)entryValue); break;
   default: ASSERT(false);
   }
}

void removeBitfieldValue(size_t enumSize, int64_t entryValue, void*data) {
   switch (enumSize) {
   case sizeof(int8_t) : (*((int8_t*)data) &= ~(int8_t)entryValue); break;
   case sizeof(int16_t) : (*((int16_t*)data) &= ~(int16_t)entryValue); break;
   case sizeof(int32_t) : (*((int32_t*)data) &= ~(int32_t)entryValue); break;
   case sizeof(int64_t) : (*((int64_t*)data) &= ~(int64_t)entryValue); break;
   default: ASSERT(false);
   }
}

void assignEnumValue(size_t enumSize, int64_t entryValue, void*target) {
   switch (enumSize) {
   case sizeof(int8_t) : *((int8_t*)target) =  (int8_t)entryValue; break;
   case sizeof(int16_t): *((int16_t*)target) = (int16_t)entryValue; break;
   case sizeof(int32_t): *((int32_t*)target) = (int32_t)entryValue; break;
   case sizeof(int64_t): *((int64_t*)target) = (int64_t)entryValue; break;
   default: ASSERT(false);
   }
}

StructMemberMetadata* typeMetadataGetMemberById(TypeMetadata const* type, Symbol* id) {
   assert(type->variety == TypeVariety_Struct); // only structs can be saved as references
   for (auto&& mchild : type->structMembers) {
      if (mchild.name == id) {
         return &mchild;
      }
   }
   return nullptr;
}

void* getReflectedMember(void* data, TypeMetadata const* type, Symbol* id) {
   if (auto member = typeMetadataGetMemberById(type, id)) {
      return (byte*)data + member->offset;
   }
   return nullptr;
}

// for serializing a reference, we find the member used for the key and serialize that symbol
static void serializeReference(SCFWriter* writer, StructMemberMetadata &member, void* data) {
   StructMemberMetadata* keyMember = typeMetadataGetMemberById(member.type, member.referenceKeyMember);

   assert(keyMember); // key member not found
   assert(keyMember->type == meta_symbol); // only symbols can be used for keys
   assert(*(byte**)data); // symbol is null

   serializeEX(writer, meta_symbol, (void*)(*(byte**)data + keyMember->offset));
}

void serializeEX(SCFWriter* writer, TypeMetadata const* type, void* data) {

   switch (type->variety) {
   case TypeVariety_Basic:
      if      (type == meta_blob)   scfWriteBytes(writer, ((Blob*)data)->data, ((Blob*)data)->sz);
      else if (type == meta_bool)   scfWriteInt(writer, *(bool*)data);
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

         if (member.flags&StructMemberFlags_StaticArray) {
            scfWriteListBegin(writer);
            scfWriteInt(writer, member.staticArraySize);
            for (int i = 0; i < member.staticArraySize; ++i) {
               auto dataptr = (byte*)data + member.offset + (member.type->size * i);
               if (member.reference) {
                  serializeReference(writer, member, dataptr);
               }
               else {
                  serializeEX(writer, member.type, dataptr);
               }
            }
            scfWriteListEnd(writer);
         }
         else {
            auto dataptr = (byte*)data + member.offset;
            if (member.reference) {
               serializeReference(writer, member, dataptr);
            }
            else {
               serializeEX(writer, member.type, dataptr);
            }
         }
         
         scfWriteListEnd(writer);
      }
      scfWriteListEnd(writer);
      break;
   case TypeVariety_Enum: {
      if (type->enumFlags&EnumFlags_Bitfield) {
         scfWriteListBegin(writer);
         for (auto&&entry : type->enumEntries) {
            if (compareBitfieldValue(type->size, entry.value, data)) {
               scfWriteString(writer, entry.name);
            }
         }
         scfWriteListEnd(writer);
      }
      else {
         bool found = false;
         for (auto&&entry : type->enumEntries) {
            if (compareEnumValue(type->size, entry.value, data)) {
               scfWriteString(writer, entry.name);
               found = true;
               break;
            }
         }
         if (!found) {
            scfWriteString(writer, UNKNOWN_ENUM_ENTRY);
         }
      }
      
   }  break;
   case TypeVariety_Array:
   case TypeVariety_KVP:
      type->funcs.serialize(writer, data);
      break;
   }

}

static void deserializeReference(SCFReader& reader, StructMemberMetadata &member, void* data) {
   assert(member.referenceOwnerType->variety == TypeVariety_KVP); // owner must be a kvp

   Symbol* key = nullptr;
   deserializeEX(reader, meta_symbol, &key);

   member.referenceOwnerType->funcs.retrieveKVP(member.referenceOwner, &key, (void**)data);
}

void deserializeEX(SCFReader& reader, TypeMetadata const* type, void* target) {

   switch (type->variety) {
   case TypeVariety_Basic:
      if (type == meta_blob) {
         auto &b = *(Blob*)target;
         blobDestroy(b);
         b = blobCreate(scfReadBytes(reader, &b.sz), b.sz);
      }       
      else if (type == meta_bool)   *(bool*)target = (bool)*scfReadInt(reader);
      else if (type == meta_byte)   *(byte*)target = (byte)*scfReadInt(reader);
      else if (type == meta_sbyte)  *(char*)target = (char)*scfReadInt(reader);
      else if (type == meta_i16)    *(int16_t*)target = (int16_t)*scfReadInt(reader);
      else if (type == meta_i32)    *(int32_t*)target = (int32_t)*scfReadInt(reader);
      else if (type == meta_i64)    *(int64_t*)target = (int64_t)*scfReadInt(reader);
      else if (type == meta_u16)    *(uint16_t*)target = (uint16_t)*scfReadInt(reader);
      else if (type == meta_u32)    *(uint32_t*)target = (uint32_t)*scfReadInt(reader);
      else if (type == meta_u64)    *(uint64_t*)target = (uint64_t)*scfReadInt(reader);
      else if (type == meta_f32)    *(float*)target = *scfReadFloat(reader);
      else if (type == meta_f64)    *(double*)target = *scfReadDouble(reader);      
      else if (type == meta_string) ((std::string*)target)->assign(scfReadString(reader));
      else if (type == meta_symbol) *(Symbol**)target = intern(scfReadString(reader));
      break;
   case TypeVariety_Struct: {
      auto list = scfReadList(reader);
      while (!scfReaderAtEnd(list)) {
         auto mlist = scfReadList(list);
         auto name = intern(scfReadString(mlist));
         for (auto&& m : type->structMembers) {
            if (m.name == name) {
               if (m.flags&StructMemberFlags_StaticArray) {
                  auto staticList = scfReadList(mlist);
                  auto szInFile = scfReadInt(staticList);
                  auto sz = MIN((size_t)*szInFile, m.staticArraySize);
                  
                  for (int i = 0; i < sz; ++i) {
                     auto dataptr = (byte*)target + m.offset + (m.type->size * i);
                     if (m.reference) {
                        deserializeReference(staticList, m, dataptr);
                     }
                     else {
                        deserializeEX(staticList, m.type, dataptr);
                     }
                  }
               }
               else {
                  auto dataptr = (byte*)target + m.offset;
                  if (m.reference) {
                     deserializeReference(mlist, m, dataptr);
                  }
                  else {
                     deserializeEX(mlist, m.type, dataptr);
                  }
               }
               break;
            }
         }
      }
   }  break;
   case TypeVariety_Enum: {
      if (type->enumFlags&EnumFlags_Bitfield) {
         auto symlist = scfReadList(reader);
         int64_t compositeValue = 0;
         while (!scfReaderAtEnd(symlist)) {
            if (auto str = scfReadString(symlist)) {
               str = intern(str);
               for (auto&& entry : type->enumEntries) {
                  if (str == entry.name) {
                     compositeValue |= entry.value;
                     break;
                  }
               }
            }
            else {
               scfReaderSkip(symlist);
            }
         }
         assignEnumValue(type->size, compositeValue, target);
      }
      else {
         auto str = intern(scfReadString(reader));
         for (auto&& entry : type->enumEntries) {
            if (str == entry.name) {
               assignEnumValue(type->size, entry.value, target);
               break;
            }
         }
      }      
   }  break;
   case TypeVariety_Array:
   case TypeVariety_KVP:
      type->funcs.deserialize(reader, target);
      break;
   }
}

TypeMetadata const*typeMetadataImGuiPayloadPeek() {
   if (auto pload = ImGui::AcceptDragDropPayload(TypePayload, ImGuiDragDropFlags_AcceptPeekOnly)) {
      auto mdata = (MetadataPayload *)pload->Data;
      return mdata->metadata;
   }
   return nullptr;
}

static MetadataPayload *_typeMetadataImGuiPayloadAcceptEX() {
   if (auto pload = ImGui::AcceptDragDropPayload(TypePayload)) {
      return (MetadataPayload *)pload->Data;
   }
   return nullptr;
}

void* typeMetadataImGuiPayloadAcceptEX(TypeMetadata const* type) {
   if (typeMetadataImGuiPayloadPeek() == type) {
      if (auto pload = _typeMetadataImGuiPayloadAcceptEX()) {
         return pload->data;
      }
   }
   return nullptr;
}



