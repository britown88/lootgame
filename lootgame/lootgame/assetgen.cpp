#include "stdafx.h"
#include "app.h"

#include "defs.h"
#include "win.h"
#include "vex.h"

#include "game.h"



static void _generateHeader(Array<Symbol*> &textureIds, Array<Symbol*> &mapIds) {
   auto templt = fileReadString("assets_gen.h.temp");
   auto t = vexTemplateCreate(templt.c_str());

   for (auto&&tex : textureIds) {
      vexTemplateBeginScope(t, "texture");
      vexTemplateAddSubstitution(t, "name", tex);
      vexTemplateEndScope(t);
   }

   for (auto&&map : mapIds) {
      vexTemplateBeginScope(t, "map");
      vexTemplateAddSubstitution(t, "name", map);
      vexTemplateEndScope(t);
   }

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

static void _generateInl(Array<Symbol*> &textureIds, Array<Symbol*> &mapIds) {
   auto templt = fileReadString("assets_gen.temp");
   auto t = vexTemplateCreate(templt.c_str());
     
   for (auto&&tex : textureIds) {
      vexTemplateBeginScope(t, "texture");
      vexTemplateAddSubstitution(t, "name", tex);
      vexTemplateEndScope(t);
   }

   for (auto&&map : mapIds) {
      vexTemplateBeginScope(t, "map");
      vexTemplateAddSubstitution(t, "name", map);
      vexTemplateEndScope(t);
   }

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

   Array<Symbol*> textureIds; 
   Array<Symbol*> mapIds;

   for (auto&&tex : Assets.textures) {
      textureIds.push_back(tex.first);
   }
   for (auto&&map : Assets.maps) {
      mapIds.push_back(map.first);
   }

   std::sort(textureIds.begin(), textureIds.end(), [](Symbol*a, Symbol*b) {return natstrcmp(a, b) < 0; });
   std::sort(mapIds.begin(), mapIds.end(), [](Symbol*a, Symbol*b) {return natstrcmp(a, b) < 0; });

   _generateHeader(textureIds, mapIds);
   _generateInl(textureIds, mapIds);
   
}