AssetTextures Textures;
AssetMaps Maps;

void assetsStartup(){
   Textures.Circle = &Assets.textures[intern("Circle")];
   Textures.array[Textures_Circle] = Textures.Circle;
   Textures.Dude = &Assets.textures[intern("Dude")];
   Textures.array[Textures_Dude] = Textures.Dude;
   Textures.DudeNormals = &Assets.textures[intern("DudeNormals")];
   Textures.array[Textures_DudeNormals] = Textures.DudeNormals;
   Textures.GemCracked = &Assets.textures[intern("GemCracked")];
   Textures.array[Textures_GemCracked] = Textures.GemCracked;
   Textures.GemEmpty = &Assets.textures[intern("GemEmpty")];
   Textures.array[Textures_GemEmpty] = Textures.GemEmpty;
   Textures.GemFilled = &Assets.textures[intern("GemFilled")];
   Textures.array[Textures_GemFilled] = Textures.GemFilled;
   Textures.HeartEmpty = &Assets.textures[intern("HeartEmpty")];
   Textures.array[Textures_HeartEmpty] = Textures.HeartEmpty;
   Textures.HeartFilled = &Assets.textures[intern("HeartFilled")];
   Textures.array[Textures_HeartFilled] = Textures.HeartFilled;
   Textures.Light = &Assets.textures[intern("Light")];
   Textures.array[Textures_Light] = Textures.Light;
   Textures.ShittySword = &Assets.textures[intern("ShittySword")];
   Textures.array[Textures_ShittySword] = Textures.ShittySword;
   Textures.SwordNormals = &Assets.textures[intern("SwordNormals")];
   Textures.array[Textures_SwordNormals] = Textures.SwordNormals;
   Textures.Target = &Assets.textures[intern("Target")];
   Textures.array[Textures_Target] = Textures.Target;
   Textures.Tile = &Assets.textures[intern("Tile")];
   Textures.array[Textures_Tile] = Textures.Tile;
   Textures.TileNormals = &Assets.textures[intern("TileNormals")];
   Textures.array[Textures_TileNormals] = Textures.TileNormals;
   

   Maps.secondmap = &Assets.maps[intern("secondmap")];
   Maps.array[Maps_secondmap] = Maps.secondmap;
   Maps.test = &Assets.maps[intern("test")];
   Maps.array[Maps_test] = Maps.test;
   
}