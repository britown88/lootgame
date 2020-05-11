#pragma once

typedef struct EGAPalette EGAPalette;
typedef struct TypeMetadata TypeMetadata;
typedef struct StructMemberMetadata StructMemberMetadata;
typedef struct ColorRGBAf ColorRGBAf;
typedef struct Int2 Int2;
typedef struct Float2 Float2;

typedef bool(*UIRenderFunc)(TypeMetadata const* type, void* data, StructMemberMetadata const* parent, const char* label);

template<typename T>
struct UIRenderer {
   static UIRenderFunc func() { return nullptr; }
};

template<typename T>
UIRenderFunc customUIRenderer() { return UIRenderer<T>::func(); }


bool customUIRender_EGAPalette(TypeMetadata const* type, void* data, StructMemberMetadata const* parent, const char* label);
bool customUIRender_ColorRGBAf(TypeMetadata const* type, void* data, StructMemberMetadata const* parent, const char* label);
bool customUIRender_Int2(TypeMetadata const* type, void* data, StructMemberMetadata const* parent, const char* label);
bool customUIRender_Float2(TypeMetadata const* type, void* data, StructMemberMetadata const* parent, const char* label);


template<> struct UIRenderer<EGAPalette> { static UIRenderFunc func() { return customUIRender_EGAPalette; } };
template<> struct UIRenderer<ColorRGBAf> { static UIRenderFunc func() { return customUIRender_ColorRGBAf; } };
template<> struct UIRenderer<Int2> { static UIRenderFunc func() { return customUIRender_Int2; } };
template<> struct UIRenderer<Float2> { static UIRenderFunc func() { return customUIRender_Float2; } };