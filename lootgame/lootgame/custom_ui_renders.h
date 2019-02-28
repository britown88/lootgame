#pragma once

typedef struct TypeMetadata TypeMetadata;
typedef struct StructMemberMetadata StructMemberMetadata;
typedef struct ColorRGBAf ColorRGBAf;

typedef bool(*UIRenderFunc)(TypeMetadata const* type, void* data, StructMemberMetadata const* parent, const char* label);

template<typename T>
struct UIRenderer {
   static UIRenderFunc func() { return nullptr; }
};

template<typename T>
UIRenderFunc customUIRenderer() { return UIRenderer<T>::func(); }


bool customUIRender_ColorRGBAf(TypeMetadata const* type, void* data, StructMemberMetadata const* parent, const char* label);


template<> struct UIRenderer<ColorRGBAf> { static UIRenderFunc func() { return customUIRender_ColorRGBAf; } };