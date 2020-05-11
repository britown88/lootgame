AssetTextures Textures;
AssetPalettes Palettes;


void assetsStartup(){
   
   Textures.test2 = &Assets.textures[intern("test2")];
   Textures.array[Textures_test2] = Textures.test2;
   
   Palettes.test = &Assets.palettes[intern("test")];
   Palettes.array[Palettes_test] = Palettes.test;
   
}