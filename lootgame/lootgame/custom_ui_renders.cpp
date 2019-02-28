#include "defs.h"
#include "reflection.h"

#include "custom_ui_renders.h"

#include <imgui.h>

bool customUIRender_ColorRGBAf(TypeMetadata const* type, void* data, StructMemberMetadata const* parent, const char* label) {
   auto c = (ColorRGBAf*)data;
   return ImGui::ColorEdit4(label ? label : parent->name, (float*)c);
}