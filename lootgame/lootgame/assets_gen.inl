AssetTextures Textures;
AssetSprites Sprites;
AssetMaps Maps;
AssetWeapons Weapons;
AssetMoveSets MoveSets;
AssetDudeTemplates DudeTemplates;


void assetsStartup(){
   
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
   Textures.ShittySword = &Assets.textures[intern("ShittySword")];
   Textures.array[Textures_ShittySword] = Textures.ShittySword;
   Textures.SwordNormals = &Assets.textures[intern("SwordNormals")];
   Textures.array[Textures_SwordNormals] = Textures.SwordNormals;
   Textures.Tile = &Assets.textures[intern("Tile")];
   Textures.array[Textures_Tile] = Textures.Tile;
   Textures.TileNormals = &Assets.textures[intern("TileNormals")];
   Textures.array[Textures_TileNormals] = Textures.TileNormals;
   Textures.TinyHeartEmpty = &Assets.textures[intern("TinyHeartEmpty")];
   Textures.array[Textures_TinyHeartEmpty] = Textures.TinyHeartEmpty;
   Textures.TinyHeartFull = &Assets.textures[intern("TinyHeartFull")];
   Textures.array[Textures_TinyHeartFull] = Textures.TinyHeartFull;
   Textures.TinyStamCracked = &Assets.textures[intern("TinyStamCracked")];
   Textures.array[Textures_TinyStamCracked] = Textures.TinyStamCracked;
   Textures.TinyStamEmpty = &Assets.textures[intern("TinyStamEmpty")];
   Textures.array[Textures_TinyStamEmpty] = Textures.TinyStamEmpty;
   Textures.TinyStamFull = &Assets.textures[intern("TinyStamFull")];
   Textures.array[Textures_TinyStamFull] = Textures.TinyStamFull;
   
   Sprites.Dude = &Assets.sprites[intern("Dude")];
   Sprites.array[Sprites_Dude] = Sprites.Dude;
   Sprites.Sword = &Assets.sprites[intern("Sword")];
   Sprites.array[Sprites_Sword] = Sprites.Sword;
   Sprites.Tile = &Assets.sprites[intern("Tile")];
   Sprites.array[Sprites_Tile] = Sprites.Tile;
   
   Maps.test = &Assets.maps[intern("test")];
   Maps.array[Maps_test] = Maps.test;
   
   Weapons.Sword = &Assets.weapons[intern("Sword")];
   Weapons.array[Weapons_Sword] = Weapons.Sword;
   
   MoveSets.Sword = &Assets.moveSets[intern("Sword")];
   MoveSets.array[MoveSets_Sword] = MoveSets.Sword;
   
   DudeTemplates.Dude = &Assets.dudeTemplates[intern("Dude")];
   DudeTemplates.array[DudeTemplates_Dude] = DudeTemplates.Dude;
   
}