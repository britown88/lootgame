#include "stdafx.h"
#include "app.h"

#include "defs.h"
#include "win.h"
#include "vex.h"

#include "game.h"
#include "reflection.h"
#include "reflection_gen.h"

template<typename T>
static void _addMapSubstitions_header(VexTemplate* t, std::unordered_map<Symbol*, T>& map, const char* strTypename) {
   Array<Symbol*> ids;
   for (auto&&kvp : map) {  ids.push_back(kvp.first); }
   std::sort(ids.begin(), ids.end(), [](Symbol*a, Symbol*b) {return natstrcmp(a, b) < 0; });

   vexTemplateBeginScope(t, "asset_def");
   vexTemplateAddSubstitution(t, "asset_typename", strTypename);
   vexTemplateAddSubstitution(t, "asset_type", reflect<T>()->name);

   if (ids.empty()) {
      vexTemplateBeginScope(t, "asset_dummy");
      vexTemplateEndScope(t);
   }
   else {
      for (auto&&id : ids) {
         vexTemplateBeginScope(t, "asset");
         vexTemplateAddSubstitution(t, "asset_name", id);

         vexTemplateEndScope(t);
      }
   }

   vexTemplateEndScope(t);
}

template<typename T>
static void _addMapSubstitions_impl(VexTemplate* t, std::unordered_map<Symbol*, T>& map, const char* strTypename, const char* strGlobalMap) {
   Array<Symbol*> ids;
   for (auto&&kvp : map) { ids.push_back(kvp.first); }
   std::sort(ids.begin(), ids.end(), [](Symbol*a, Symbol*b) {return natstrcmp(a, b) < 0; });

   vexTemplateBeginScope(t, "asset_def");
   vexTemplateAddSubstitution(t, "asset_typename", strTypename);
   vexTemplateAddSubstitution(t, "asset_globalmap", strGlobalMap);

   for (auto&&id : ids) {
      vexTemplateBeginScope(t, "asset");
      vexTemplateAddSubstitution(t, "asset_name", id);

      vexTemplateEndScope(t);
   }

   vexTemplateEndScope(t);
}



static void _generateHeader() {
   auto templt = fileReadString("assets_gen.h.temp");
   auto t = vexTemplateCreate(templt.c_str());

   //_addMapSubstitions_header(t, Assets.textures, "Textures");
   //_addMapSubstitions_header(t, Assets.sprites, "Sprites");
   //_addMapSubstitions_header(t, Assets.maps, "Maps");
   //_addMapSubstitions_header(t, Assets.weapons, "Weapons");
   //_addMapSubstitions_header(t, Assets.moveSets, "MoveSets");
   //_addMapSubstitions_header(t, Assets.dudeTemplates, "DudeTemplates");

   auto output = vexTemplateRender(t);
   std::string outFile = "assets_gen.h";

   if (fileReadString(outFile.c_str()) == output) {
      printf("File %s unchanged.\n", outFile.c_str());
   }
   else {
      writeStringFile(outFile.c_str(), output.c_str());
      printf("Generated %s.\n", outFile.c_str());
   }
}

#define _addMapSubstitions_implHelper(t, map, str) _addMapSubstitions_impl(t, map, str, #map)

static void _generateInl() {
   auto templt = fileReadString("assets_gen.temp");
   auto t = vexTemplateCreate(templt.c_str());
     
   //_addMapSubstitions_implHelper(t, Assets.textures, "Textures");
   //_addMapSubstitions_implHelper(t, Assets.sprites, "Sprites");
   //_addMapSubstitions_implHelper(t, Assets.maps, "Maps");
   //_addMapSubstitions_implHelper(t, Assets.weapons, "Weapons");
   //_addMapSubstitions_implHelper(t, Assets.moveSets, "MoveSets");
   //_addMapSubstitions_implHelper(t, Assets.dudeTemplates, "DudeTemplates");

   auto output = vexTemplateRender(t);
   std::string outFile = "assets_gen.inl";

   if (fileReadString(outFile.c_str()) == output) {
      printf("File %s unchanged.\n", outFile.c_str());
   }
   else {
      writeStringFile(outFile.c_str(), output.c_str());
      printf("Generated %s.\n", outFile.c_str());
   }
}



void runAssetGen() {
   assetsLoad();

   _generateHeader();
   _generateInl();
   
}