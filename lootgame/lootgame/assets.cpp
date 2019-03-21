#include "stdafx.h"


#include "app.h"
#include "win.h"
#include "game.h"
#include <stb/stb_image.h>

#include "reflection_gen.h"

//_MapMap MapMap;

namespace Paths {
   static const std::filesystem::path gameAssets = "lootgame.bin";
}

static void _clearDeleted(GameAssets& assets) {
   Array<Symbol*> texToDelete;
   for (auto&& kvp : assets.textures) {
      if (kvp.second.markForDelete) {
         texToDelete.push_back(kvp.first);
      }
   }
   if (!texToDelete.empty()) {
      WARN("Ignoring %d deleted textures from save.", texToDelete.size());
   }
   for (auto&& td : texToDelete) {
      assets.textures.erase(td);
   }

   Array<Symbol*> mapsToDelete;
   for (auto&& kvp : assets.maps) {
      if (kvp.second.markForDelete) {
         mapsToDelete.push_back(kvp.first);
      }
   }
   if (!mapsToDelete.empty()) {
      WARN("Ignoring %d deleted maps from save.", mapsToDelete.size());
   }
   for (auto&& td : mapsToDelete) {
      assets.maps.erase(td);
   }
}

void assetsSave() {
   auto fpath = AppConfig.assetPath / Paths::gameAssets;

   auto writer = scfWriterCreate();
   uint64_t sz = 0;

   // we clone our assets before save so we can ignore deleted objects when we save
   auto assetCopy = Assets;
   _clearDeleted(assetCopy);

   serialize(writer, &assetCopy);
   auto output = scfWriteToBuffer(writer, &sz);

   writeBinaryFile(fpath.string().c_str(), (byte*)output, sz);
   scfWriterDestroy(writer);
   delete[] output;

   LOG("%s Saved", fpath.string().c_str());
}

static bool _loadAssets(GameAssets& target, std::filesystem::path& fpath) {
   fpath = Paths::gameAssets;
   if (!std::filesystem::exists(fpath)) {
      fpath = AppConfig.assetPath / fpath;
   }

   if (auto content = fileReadBinary(fpath.string().c_str(), nullptr)) {
      auto reader = scfView(content);
      deserialize(reader, &target);
      delete[] content;
      return true;
   }

   return false;
}

void assetsLoad() {
   std::filesystem::path fpath;
   if (_loadAssets(Assets, fpath)) {
      LOG("Loaded %s", fpath.string().c_str());
   }
   else {
      ERR("Error %s", fpath.string().c_str());
   }
}

// either builds from existing data or refreshes
static void _textureInitHandle(Texture&t) {
   if (t.storedImageData) {
      t.handle = render::buildTextureHandle(t.sz, t.flags, (ColorRGBA const*)t.storedImageData.data);
   }
   else {
      render::textureRefresh(t);
   }
}


static void _mergeMaps(GameAssets& from) {
   for (auto&&kvp : from.maps) {
      auto result = Assets.maps.insert({ kvp.first, kvp.second });
      if (!result.second) {
         auto& existing = Assets.maps[kvp.first];
         existing = kvp.second;
         existing.markForDelete = false;
      }
   }
}

static void _mergeTextures(GameAssets& from) {
   for (auto&&kvp : from.textures) {
      auto result = Assets.textures.insert({ kvp.first, kvp.second });
      if (!result.second) {
         auto& existing = Assets.textures[kvp.first];
         render::textureDestroy(existing.handle);
         existing = kvp.second;
         existing.markForDelete = false;
         _textureInitHandle(existing);
      }
   }
}


void assetsReloadMaps() {
   std::filesystem::path fpath;
   GameAssets reloaded;

   if (_loadAssets(reloaded, fpath)) {
      _mergeMaps(reloaded);
      LOG("Reloaded maps from %s", fpath.string().c_str());
   }
   else {
      ERR("Failed to reload maps from %s", fpath.string().c_str());
   }
}


void assetsReloadTextures() {
   std::filesystem::path fpath;
   GameAssets reloaded;

   if (_loadAssets(reloaded, fpath)) {
      _mergeTextures(reloaded);
      LOG("Reloaded textures from %s", fpath.string().c_str());
   }
   else {
      ERR("Failed to reload textures from %s", fpath.string().c_str());
   }

}
void assetsReloadAll() {
   std::filesystem::path fpath;
   GameAssets reloaded;

   if (_loadAssets(reloaded, fpath)) {
      Const = reloaded.constants;
      _mergeMaps(reloaded);
      _mergeTextures(reloaded);
      LOG("Reloaded %s", fpath.string().c_str());
   }
   else {
      ERR("Failed to reload %s", fpath.string().c_str());
   }
}

void assetsBuildTextureHandles() {
   for (auto&&kvp : Assets.textures) {
      _textureInitHandle(kvp.second);
   }
}


static int _reloadShader(GraphicObjects &gfx) {

   auto vertex = fileReadString("assets/vertex.glsl");
   auto fragment = fileReadString("assets/fragment.glsl");

   auto s = render::shaderBuild(vertex.c_str(), fragment.c_str());

   if (s == 0) {
      return 0;
   }

   if (gfx.shader != 0) {
      render::shaderDestroy(gfx.shader);
   }

   gfx.shader = s;
   return 1;
}


static Texture _textureBuildFromFile(const char* path, TextureFlag flags = TextureFlag_Defaults) {
   uint64_t sz = 0;
   int32_t x, y, comp;
   x = y = comp = 0;
   auto mem = fileReadBinary(path, &sz);
   auto png = stbi_load_from_memory(mem, (int32_t)sz, &x, &y, &comp, 4);
   auto out = render::textureBuild({ x, y }, flags, (ColorRGBA*)png);

   free(mem);
   free(png);
   return out;
}

void GraphicObjects::build() {
   assetsBuildTextureHandles();

   assert(_reloadShader(*this));

   Vertex vbo[] = {
      { { -0.5f, -0.5f },{ 0.0f, 0.0f } },
      { {  0.5f, -0.5f },{ 1.0f, 0.0f } },
      { { -0.5f,  0.5f },{ 0.0f, 1.0f } },

      { {  0.5f, -0.5f },{ 1.0f, 0.0f } },
      { {  0.5f,  0.5f },{ 1.0f, 1.0f } },
      { { -0.5f,  0.5f },{ 0.0f, 1.0f } },
   };

   Vertex vboUncentered[] = {
      { { 0.0f, 0.0f },{ 0.0f, 0.0f } },
      { { 1.0f, 0.0f },{ 1.0f, 0.0f } },
      { { 0.0f, 1.0f },{ 0.0f, 1.0f } },

      { { 1.0f, 0.0f },{ 1.0f, 0.0f } },
      { { 1.0f, 1.0f },{ 1.0f, 1.0f } },
      { { 0.0f, 1.0f },{ 0.0f, 1.0f } },
   };

   mesh = render::meshBuild(vbo, 6);
   meshUncentered = render::meshBuild(vboUncentered, 6);

   auto& res = Const.resolution;
   unlitScene = render::fboBuild(res, {
      render::textureBuild(res, TextureFlag_FBODefaults),   // diffuse
      render::textureBuild(res, TextureFlag_FBODefaults)    // normals
      });
   lightLayer = render::fboBuild(res);
   litScene = render::fboBuild(res);
   UI = render::fboBuild(res);
}

bool GraphicObjects::reloadShaders() {
   return _reloadShader(*this);
}