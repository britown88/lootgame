#include "stdafx.h"


#include "app.h"
#include "win.h"
#include "game.h"
#include <stb/stb_image.h>


#include "reflection.h"
#include "reflection_gen.h"

//_MapMap MapMap;

namespace Paths {
   static const std::filesystem::path gameAssets = "chron.bin";
}

template<typename T>
static void _clearDeleted(std::unordered_map<Symbol*, T>& map) {
   Array<Symbol*> toDelete;
   for (auto&& kvp : map) {
      if (kvp.second.markForDelete) {
         toDelete.push_back(kvp.first);
      }
   }
   if (!toDelete.empty()) {
      WARN("Ignoring %d deleted %s from save.", toDelete.size(), reflect<T>()->name);
   }
   for (auto&& td : toDelete) {
      map.erase(td);
   }
}

void assetsSave() {
   auto fpath = AppConfig.assetPath / Paths::gameAssets;

   auto writer = scfWriterCreate();
   uint64_t sz = 0;

   // we clone our assets before save so we can ignore deleted objects when we save
   auto assetCopy = Assets;

   // add markForDelete maps here
   //_clearDeleted(assetCopy.textures);
   //_clearDeleted(assetCopy.maps);
   //_clearDeleted(assetCopy.sprites);
   //_clearDeleted(assetCopy.moveSets);
   //_clearDeleted(assetCopy.weapons);
   //_clearDeleted(assetCopy.dudeTemplates);

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
      render::textureRefreshFromFile(t);
   }
}

template<typename T> static void _preLoad(T& t) {}
template<> static void _preLoad<Texture>(Texture& t) { render::textureHandleDestroy(t.handle); }

template<typename T> static void _postLoad(T& t) {}
template<> static void _postLoad<Texture>(Texture& t) { _textureInitHandle(t); }

template<typename T>
static void _mergeMap(std::unordered_map<Symbol*, T>& from, std::unordered_map<Symbol*, T>& to) {
   for (auto&&kvp : from) {
      auto result = to.insert({ kvp.first, kvp.second });
      if (!result.second) {
         auto& existing = to[kvp.first];

         _preLoad(existing);

         existing = kvp.second;
         existing.markForDelete = false;

         _postLoad(existing);
      }
   }
}
void assetsReloadAll() {
   std::filesystem::path fpath;
   GameAssets reloaded;

   if (_loadAssets(reloaded, fpath)) {
      Const = reloaded.constants;

      // add reload merge maps here
      //_mergeMap(reloaded.maps, Assets.maps);
      //_mergeMap(reloaded.textures, Assets.textures);
      //_mergeMap(reloaded.sprites, Assets.sprites);
      //_mergeMap(reloaded.moveSets, Assets.moveSets);
      //_mergeMap(reloaded.weapons, Assets.weapons);
      //_mergeMap(reloaded.dudeTemplates, Assets.dudeTemplates);

      LOG("Reloaded %s", fpath.string().c_str());
   }
   else {
      ERR("Failed to reload %s", fpath.string().c_str());
   }
}

void assetsBuildTextureHandles() {
   //for (auto&&kvp : Assets.textures) {
   //   _textureInitHandle(kvp.second);
   //}
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