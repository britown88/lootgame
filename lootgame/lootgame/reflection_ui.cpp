#include "reflection.h"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

bool doTypeUIEX(TypeMetadata const* type, void* data, const char* displayName) {
   if (!type) {
      return false;
   }
   ImGui::PushID(data);
   struct DEFER{
      ~DEFER() { ImGui::PopID(); }
   } defer;

   switch (type->variety) {
   case TypeVariety_Basic: {
      int64_t i64;
      uint64_t u64;

      if (type == meta_bool)        return ImGui::Checkbox(displayName, (bool*)data);
      if (type == meta_byte) { u64 = *(byte*)data; if (ImGui::InputScalar(displayName, ImGuiDataType_U64, &u64)) { *(byte*)data = (byte)u64; return true; } return false; }
      if (type == meta_sbyte) { i64 = *(char*)data; if (ImGui::InputScalar(displayName, ImGuiDataType_S64, &i64)) { *(char*)data = (char)i64; return true; } return false; }
      if (type == meta_i16) { i64 = *(int16_t*)data; if (ImGui::InputScalar(displayName, ImGuiDataType_S64, &i64)) { *(int16_t*)data = (int16_t)i64; return true; } return false; }
      if (type == meta_i32) { i64 = *(int32_t*)data; if (ImGui::InputScalar(displayName, ImGuiDataType_S64, &i64)) { *(int32_t*)data = (int32_t)i64; return true; } return false; }
      if (type == meta_i64) { i64 = *(int64_t*)data; if (ImGui::InputScalar(displayName, ImGuiDataType_S64, &i64)) { *(int64_t*)data = (int64_t)i64; return true; } return false; }
      if (type == meta_u16) { u64 = *(uint16_t*)data; if (ImGui::InputScalar(displayName, ImGuiDataType_U64, &u64)) { *(uint16_t*)data = (uint16_t)u64; return true; } return false; }
      if (type == meta_u32) { u64 = *(uint32_t*)data; if (ImGui::InputScalar(displayName, ImGuiDataType_U64, &u64)) { *(uint32_t*)data = (uint32_t)u64; return true; } return false; }
      if (type == meta_u64) { u64 = *(uint64_t*)data; if (ImGui::InputScalar(displayName, ImGuiDataType_U64, &u64)) { *(uint64_t*)data = (uint64_t)u64; return true; } return false; }
      if (type == meta_f32) { return ImGui::InputScalar(displayName, ImGuiDataType_Float, (float*)data); }
      if (type == meta_f64) { return ImGui::InputScalar(displayName, ImGuiDataType_Double, (double*)data); }
      if (type == meta_string) { return ImGui::InputText(displayName, (std::string*)data); }
      if (type == meta_symbol) { ImGui::AlignTextToFramePadding(); ImGui::TextUnformatted(*(StringView*)data); return false; }
   }  break;
   case TypeVariety_Struct:
      if (ImGui::CollapsingHeader(displayName)) {
         ImGui::Indent();
         for (auto&& member : type->structMembers) {
            if (member.flags&StructMemberFlags_StaticArray) {
               ImGui::NewLine();
               ImGui::Indent();
               for (int i = 0; i < member.staticArraySize; ++i) {
                  ImGui::AlignTextToFramePadding(); ImGui::Text("%d:", i); ImGui::SameLine();
                  doTypeUIEX(member.type, (byte*)data + member.offset + (member.type->size * i), member.name);
               }
               ImGui::Unindent();
            }
            else {
               doTypeUIEX(member.type, (byte*)data + member.offset, member.name);
            }
         }
         ImGui::Unindent();
      }
      
      break;
   case TypeVariety_Enum: {
      //if (type->enumFlags&EnumFlags_Bitfield) {
      //   scfWriteListBegin(writer);
      //   for (auto&&entry : type->enumEntries) {
      //      if (_compareBitfieldValue(type->size, entry.value, data)) {
      //         scfWriteString(writer, entry.name);
      //      }
      //   }
      //   scfWriteListEnd(writer);
      //}
      //else {
      //   bool found = false;
      //   for (auto&&entry : type->enumEntries) {
      //      if (_compareEnumValue(type->size, entry.value, data)) {
      //         scfWriteString(writer, entry.name);
      //         found = true;
      //         break;
      //      }
      //   }
      //   if (!found) {
      //      scfWriteString(writer, UNKNOWN_ENUM_ENTRY);
      //   }
      //}

   }  break;
   case TypeVariety_Array:
   case TypeVariety_KVP:
      //type->funcs.serialize(writer, data);
      break;
   }

   return false;
}