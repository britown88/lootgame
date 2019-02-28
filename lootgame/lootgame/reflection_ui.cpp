#include "reflection.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <misc/cpp/imgui_stdlib.h>

static bool _doTypeUIEX(TypeMetadata const* type, void* data, StructMemberMetadata const* parent) {
   ImGui::PushID(data);

   bool readOnly = parent && parent->flags&StructMemberFlags_ReadOnly;

   if (readOnly) {
      ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
      ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
   }

   DEFER{ 
      if (readOnly) {
         ImGui::PopItemFlag();
         ImGui::PopStyleVar();
      }
      ImGui::PopID(); 
   };

   switch (type->variety) {
   case TypeVariety_Basic: {
      int64_t i64;
      uint64_t u64;

      if (type == meta_bool)        return ImGui::Checkbox(parent->name, (bool*)data);
      if (type == meta_byte) { u64 = *(byte*)data; if (ImGui::DragScalar(parent->name, ImGuiDataType_U64, &u64, parent->uiStepSpeed)) { *(byte*)data = (byte)u64; return true; } return false; }
      if (type == meta_sbyte) { i64 = *(char*)data; if (ImGui::DragScalar(parent->name, ImGuiDataType_S64, &i64, parent->uiStepSpeed)) { *(char*)data = (char)i64; return true; } return false; }
      if (type == meta_i16) { i64 = *(int16_t*)data; if (ImGui::DragScalar(parent->name, ImGuiDataType_S64, &i64, parent->uiStepSpeed)) { *(int16_t*)data = (int16_t)i64; return true; } return false; }
      if (type == meta_i32) { i64 = *(int32_t*)data; if (ImGui::DragScalar(parent->name, ImGuiDataType_S64, &i64, parent->uiStepSpeed)) { *(int32_t*)data = (int32_t)i64; return true; } return false; }
      if (type == meta_i64) { i64 = *(int64_t*)data; if (ImGui::DragScalar(parent->name, ImGuiDataType_S64, &i64, parent->uiStepSpeed)) { *(int64_t*)data = (int64_t)i64; return true; } return false; }
      if (type == meta_u16) { u64 = *(uint16_t*)data; if (ImGui::DragScalar(parent->name, ImGuiDataType_U64, &u64, parent->uiStepSpeed)) { *(uint16_t*)data = (uint16_t)u64; return true; } return false; }
      if (type == meta_u32) { u64 = *(uint32_t*)data; if (ImGui::DragScalar(parent->name, ImGuiDataType_U64, &u64, parent->uiStepSpeed)) { *(uint32_t*)data = (uint32_t)u64; return true; } return false; }
      if (type == meta_u64) { u64 = *(uint64_t*)data; if (ImGui::DragScalar(parent->name, ImGuiDataType_U64, &u64, parent->uiStepSpeed)) { *(uint64_t*)data = (uint64_t)u64; return true; } return false; }
      if (type == meta_f32) { return ImGui::DragScalar(parent->name, ImGuiDataType_Float, (float*)data, parent->uiStepSpeed); }
      if (type == meta_f64) { return ImGui::DragScalar(parent->name, ImGuiDataType_Double, (double*)data, parent->uiStepSpeed); }
      if (type == meta_string) { return ImGui::InputText(parent->name, (std::string*)data); }
      if (type == meta_symbol) { ImGui::AlignTextToFramePadding(); ImGui::TextUnformatted(*(StringView*)data); return false; }
   }  break;
   case TypeVariety_Struct: {
      bool shown = true;
      if (parent) {
         shown = ImGui::CollapsingHeader(parent->name, ImGuiTreeNodeFlags_DefaultOpen);
      }

      if (shown) {
         if (parent) ImGui::Indent();
         for (auto&& member : type->structMembers) {
            if (member.flags&StructMemberFlags_StaticArray) {
               ImGui::NewLine();
               ImGui::Indent();
               for (int i = 0; i < member.staticArraySize; ++i) {
                  ImGui::AlignTextToFramePadding(); ImGui::Text("%d:", i); ImGui::SameLine();
                  _doTypeUIEX(member.type, (byte*)data + member.offset + (member.type->size * i), &member);
               }
               ImGui::Unindent();
            }
            else {
               _doTypeUIEX(member.type, (byte*)data + member.offset, &member);
            }
         }
      
         if (parent) ImGui::Unindent();
      }

   }  break;
   case TypeVariety_Enum: {
      if (type->enumFlags&EnumFlags_Bitfield) {

         for (auto&&entry : type->enumEntries) {
            bool compareBitfieldValue(size_t enumSize, int64_t entryValue, void*data);
            if (compareBitfieldValue(type->size, entry.value, data)) {

            }
         }
      }
      else {
         EnumEntryMetadata const* entry = nullptr;
         for (auto&&e : type->enumEntries) {
            bool compareEnumValue(size_t enumSize, int64_t entryValue, void*data);
            if (compareEnumValue(type->size, e.value, data)) {
               entry = &e;
               break;
            }
         }

         if (ImGui::BeginCombo(parent->name, entry ? entry->name : "")) {
            for (auto&&e : type->enumEntries) {
               if (ImGui::MenuItem(e.name)) {
                  void assignEnumValue(size_t enumSize, int64_t entryValue, void*target);
                  assignEnumValue(type->size, e.value, data);
               }
            }
            ImGui::EndCombo();
         }

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
      }

   }  break;
   case TypeVariety_Array:
   case TypeVariety_KVP:
      //type->funcs.serialize(writer, data);
      break;
   }

   return false;
}



bool doTypeUIEX(TypeMetadata const* type, void* data) {
   if (!type) {
      return false;
   }
   return _doTypeUIEX(type, data, nullptr);
}