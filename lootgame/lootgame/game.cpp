#include "game.h"
#include "app.h"
#include "win.h"
#include "render.h"
#include "imgui.h"
#include "phy.h"

#include <vector>

#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <stb/stb_image.h>

#define AXIS_DEADZONE 0.25f
#define AXIS_AIM_DEADZONE 0.5f
#define DUDE_COUNT 50

static Constants g_const;
Constants &ConstantsGet() { return g_const; }

struct Map {
   Float2 size;
};

struct Camera {
   Rectf viewport;
};



enum {
   GameTextures_Dude = 0,
   GameTextures_Target,
   GameTextures_Light,
   GameTextures_Circle,
   GameTextures_ShittySword,
   GameTextures_Tile,
   GameTextures_GemEmpty,
   GameTextures_GemFilled,

   GameTexture_COUNT
}GameTextures_;
typedef u32 GameTexture;
   
Texture g_textures[GameTexture_COUNT];

Texture gameTexture(GameTexture t) {
   return g_textures[t];
}

TextureHandle gameTextureHandle(GameTexture t) {
   return g_textures[t].handle;
}

enum SwingPhase {
   SwingPhase_Windup = 0,
   SwingPhase_Swing,
   SwingPhase_Cooldown
};

struct AttackSwing {
   f32 lungeSpeed; // character will luinge forward during swingphase
   f32 swipeAngle; // full range of the weapon swipe, in degrees
   Milliseconds swingDur; // total time for the attack
   Milliseconds windupDur; // vulnerability period before swing
   Milliseconds cooldownDur; // period before user is free again
   Rectf hitbox; // axis-aligned, The origin here is the bottom center of the attack while facing up
                 // attack will rotate around that origin
};

struct MoveSet {
   DynamicArray<AttackSwing> swings;
};

static MoveSet _createMoveSet() {
   MoveSet out;
   out.swings.resize(3);
   Rectf hitbox = { 0, -10, 50, 20 };

   out.swings[0].swipeAngle = 90.0f;
   out.swings[0].lungeSpeed = 0.0f;
   out.swings[0].windupDur = 100;
   out.swings[0].swingDur = 250;
   out.swings[0].cooldownDur = 500;
   out.swings[0].hitbox = hitbox;

   out.swings[1].swipeAngle = 180.0f;
   out.swings[1].lungeSpeed = 0.25f;
   out.swings[1].windupDur = 100;
   out.swings[1].swingDur = 250;
   out.swings[1].cooldownDur = 250;
   out.swings[1].hitbox = hitbox;

   out.swings[2].swipeAngle = 0.0f;
   out.swings[2].lungeSpeed = 0.5f;
   out.swings[2].windupDur = 100;
   out.swings[2].swingDur = 250;
   out.swings[2].cooldownDur = 100;
   out.swings[2].hitbox = hitbox;

   return out;
}


f32 cDudeAcceleration =     0.005f;
f32 cDudeRotationSpeed =    0.010f;
f32 cDudeMoveSpeed =        0.200f;
f32 cDudeDashSpeed =        1.500f;
f32 cDudeSpeedCapEasing =   0.004f;
f32 cDudeBackwardsPenalty = 0.250f;

Milliseconds cDudePostDashCooldown = 300;
Milliseconds cDudeDashDuration = 100;
Milliseconds cDudeBaseStaminaTickRecoveryTime = 1000;
Milliseconds cDudeStaminaEmptyCooldown = 1000;

struct Movement {
   f32 moveSpeedCap = 0.0f;       // updated per frame, interpolates toward moveSpeedCapTarget
   f32 moveSpeedCapTarget = 0.0f; // updated per frame, max speed based on length of move vector and velocity direction vs facing
   Float2 moveVector = { 0.0f, 0.0f };   // vector length 0-1 for movement amount/direction
   Float2 faceVector = { 0.0f, 0.0f };  // unit vector for target facing, facing will interpolate toward this angle
   Float2 facing = { 1, 0 };  // unit vector for character facing  
};

struct Dude;

struct Behavior {
   Milliseconds started;
   Dude* target = nullptr;
   int dir = 1;
   bool attack = false;
};

struct AttackState {
   Float2 weaponVector;
   
   AttackSwing swing;
   SwingPhase swingPhase;
   int swingDir;
   int combo;
};

struct CooldownState {
   Milliseconds duration;
};

struct DashState {
   Milliseconds duration;
};

struct FreeState {
   Milliseconds staminaClockStart;
   Milliseconds nextTickAt;
};

enum DudeState {
   DudeState_FREE = 0,
   DudeState_COOLDOWN,
   DudeState_DASH,
   DudeState_ATTACKING,
};

struct Status {
   int stamina, staminaMax;
};

struct Dude {
   DudeState state = DudeState_FREE;
   AttackState atk;
   CooldownState cd;
   DashState dash;
   FreeState free;


   TextureHandle texture = 0;
   Float2 renderSize;
   ColorRGBAf c;


   PhyObject phy;
   Movement mv;
   Behavior ai;
   Status status;

   
   Milliseconds stateClock; // incremented by one every ms

   
   MoveSet moveset;   
};

static void uiEditDude(Dude& dude) {
   if (ImGui::Begin(format("Dude Editor##%p", &dude).c_str(), 0, ImGuiWindowFlags_AlwaysAutoResize)) {

      ImGui::InputFloat("Acceleration", &cDudeAcceleration, 0.001f, 0.01f, 4);
      ImGui::InputFloat("Rotation Speed", &cDudeRotationSpeed, 0.01f, 0.1f, 4);
      ImGui::InputFloat("Move Speed", &dude.phy.maxSpeed, 0.01f, 0.1f, 4);
      ImGui::InputFloat("Speed Cap", &cDudeSpeedCapEasing, 0.001f, 0.01f, 4);

      f32 ratio = dude.mv.moveSpeedCap / 1.0f;
      ImGui::ProgressBar(ratio);
   }
   ImGui::End();
}


bool dudeSpendStamina(Dude &d, int stam) {
   if (d.status.stamina == 0) {
      return false;
   }

   d.status.stamina = MAX(0, d.status.stamina - stam);
   return true;
}

void dudeSetState(Dude& d, DudeState state, Milliseconds startTimeOffset = 0) {
   d.state = state;
   d.stateClock = startTimeOffset;
}

Milliseconds calcNextStaminaTickTime(int stam, int max) {
   auto ratio = (f32)stam / max;
   return (Milliseconds)(cDudeBaseStaminaTickRecoveryTime * cosInterp(1 - ratio));
}

void dudeBeginFree(Dude&d) {
   dudeSetState(d, DudeState_FREE);
   d.free.staminaClockStart = 0;
   d.free.nextTickAt = calcNextStaminaTickTime(d.status.stamina, d.status.staminaMax);
}

void dudeUpdateStateFree(Dude& d) {
   // handle stamina regain
   if (d.free.staminaClockStart++ > d.free.nextTickAt && d.status.stamina < d.status.staminaMax) {
      ++d.status.stamina;
      d.free.staminaClockStart = 0;
      d.free.nextTickAt = calcNextStaminaTickTime(d.status.stamina, d.status.staminaMax);
   }
}

void dudeBeginCooldown(Dude&d, Milliseconds duration) {
   dudeSetState(d, DudeState_COOLDOWN);
   d.cd.duration = duration;
}

void dudeUpdateStateCooldown(Dude& d) {
   if (d.stateClock > d.cd.duration) {
      dudeSetState(d, DudeState_FREE);
   }
}

void dudeBeginDash(Dude& d, f32 speed, Milliseconds dur) {
   if (dudeSpendStamina(d, 1)) {
      
      dudeSetState(d, DudeState_DASH);

      d.dash.duration = dur;

      if (v2LenSquared(d.mv.moveVector) > 0.0f) {
         d.mv.moveVector = v2Normalized(d.mv.moveVector);
      }
      else {
         d.mv.moveVector = d.mv.faceVector;
      }
      
      d.phy.maxSpeed = speed;
      d.mv.moveSpeedCapTarget = d.mv.moveSpeedCap = d.phy.maxSpeed;
      d.phy.velocity = d.mv.moveVector * d.mv.moveSpeedCap;
   }
}


void dudeUpdateStateDash(Dude& d) {
   if (d.stateClock > d.dash.duration) {
      Milliseconds cd = cDudePostDashCooldown;
      if (d.status.stamina == 0) {
         cd = cDudeStaminaEmptyCooldown;
      }

      dudeBeginCooldown(d, cd);

      d.mv.moveVector = { 0,0 };
      d.phy.maxSpeed = cDudeMoveSpeed;
   }
}

void dudeBeginAttack(Dude& d, int swingDir, int combo) {
   if (dudeSpendStamina(d, 1)) {
      dudeSetState(d, DudeState_ATTACKING);

      if (combo >= d.moveset.swings.size()) {
         combo = 0;
      }

      d.atk.combo = combo;
      d.atk.swing = d.moveset.swings[combo];

      // stop, setting face to 0 is fine here because rotation checks so you stop at your current facing and cease any facevector rotation
      d.mv.moveVector = d.mv.faceVector = { 0.0f, 0.0f };

      // setup the weapon vector
      d.atk.weaponVector = v2Normalized(v2Rotate(d.mv.facing, v2FromAngle(swingDir * d.atk.swing.swipeAngle / 2.0f * DEG2RAD)));
      d.atk.swingPhase = SwingPhase_Windup;

      d.atk.swingDir = swingDir;
   }
}

void dudeUpdateStateAttack(Dude& d) {

   switch (d.atk.swingPhase) {
   case SwingPhase_Windup:
      if (d.stateClock >= d.atk.swing.windupDur) {
         d.stateClock -= d.atk.swing.windupDur;
         d.atk.swingPhase = SwingPhase_Swing;
      }
      break;
   case SwingPhase_Swing: {

      auto radsPerMs = (d.atk.swing.swipeAngle * DEG2RAD) / d.atk.swing.swingDur;
      d.atk.weaponVector = v2Rotate(d.atk.weaponVector, v2FromAngle(-d.atk.swingDir * radsPerMs));

      // mess with lunge later
      //if (dude.swing.lungeSpeed > 0.0f) {
      //   Float2 lungeVel = dude.facing * dude.swing.lungeSpeed;
      //   bool collided = true;
      //   int tries = 0;
      //   while (collided) {
      //      collided = false;
      //      for (auto& d : g_game->dudes) {
      //         if (_dudeCollision(dude.size, dude.pos, lungeVel, d.size, d.pos, dt, lungeVel)) {
      //            collided = true;
      //            ++tries;
      //            break;
      //         }
      //      }
      //      if (tries >= 3) {
      //         lungeVel = { 0,0 };
      //         break;
      //      }
      //   }
      //   dp += lungeVel * (f32)dt.toMilliseconds();
      //}

      if (d.stateClock >= d.atk.swing.swingDur) {
         d.stateClock -= d.atk.swing.swingDur;
         d.atk.weaponVector = v2Normalized(v2Rotate(d.mv.facing, v2FromAngle(-d.atk.swingDir * d.atk.swing.swipeAngle / 2.0f * DEG2RAD)));
         d.atk.swingPhase = SwingPhase_Cooldown;
      }
   }  break;
   case SwingPhase_Cooldown:
      if (d.stateClock >= d.atk.swing.cooldownDur) {
         dudeSetState(d, DudeState_FREE);
      }
      break;
   }
}

void dudeUpdateState(Dude& d) {
   switch (d.state) {
   case DudeState_FREE: dudeUpdateStateFree(d); break;
   case DudeState_COOLDOWN: dudeUpdateStateCooldown(d); break;
   case DudeState_DASH: dudeUpdateStateDash(d); break;
   case DudeState_ATTACKING: dudeUpdateStateAttack(d); break;
   }
   ++d.stateClock;
}




static bool rightStickActive() {
   auto &io = gameDataGet()->io;
   return fabs(io.rightStick.x) > AXIS_AIM_DEADZONE || fabs(io.rightStick.y) > AXIS_AIM_DEADZONE;
}
static bool leftStickActive() {
   auto &io = gameDataGet()->io;
   return fabs(io.leftStick.x) > AXIS_DEADZONE || fabs(io.leftStick.y) > AXIS_DEADZONE;
}

void dudeApplyInputMovement(Dude& d) {
   auto& io = gameDataGet()->io;

   d.mv.moveVector = io.leftStick;

   bool rstick = rightStickActive();
   bool lstick = leftStickActive();

   Float2 aimStick;
   if (rstick) {
      aimStick = io.rightStick;
     // g_game->mouseActive = false;
   }
   else if (lstick) {
      aimStick = io.leftStick;
   }

   //if (g_game->mouseActive) {
      //aimStick = io.mousePos - d.phy.pos;
   //}

   if (v2LenSquared(aimStick) > 0) {
      d.mv.faceVector = v2Normalized(aimStick);
   }
}

void dudeApplyInputFreeActions(Dude& d) {
   auto& io = gameDataGet()->io;
   if (io.buttonPressed[GameButton_LT]){
      dudeBeginDash(d, cDudeDashSpeed, cDudeDashDuration);
   }
   else if (io.buttonPressed[GameButton_RT]) {
      dudeBeginAttack(d, 1, 0);
   }
}

void dudeApplyInputAttack(Dude& d) {
   auto& io = gameDataGet()->io;
   if (io.buttonPressed[GameButton_RT]) {
      switch (d.atk.swingPhase) {
      case SwingPhase_Cooldown:
         dudeBeginAttack(d, -d.atk.swingDir, d.atk.combo + 1);
         break;
      }

      
   }
}

void dudeApplyInput(Dude& d) {
   switch (d.state) {
   case DudeState_FREE:
      dudeApplyInputMovement(d);
      dudeApplyInputFreeActions(d);
      break;
   case DudeState_DASH:
      break;
   case DudeState_ATTACKING:
      dudeApplyInputAttack(d);
      break;
   }
}

void dudeUpdateVelocity(Dude& d) {
   // we assume at this point the moveVector is current

   // scale mvspeed based on facing;
   f32 facedot = v2Dot(v2Normalized(d.phy.velocity), d.mv.facing);
   auto scaledSpeed = (d.phy.maxSpeed * (1 - cDudeBackwardsPenalty)) + (d.phy.maxSpeed * cDudeBackwardsPenalty * facedot);

   // set the target speed
   d.mv.moveSpeedCapTarget = scaledSpeed * v2Len(d.mv.moveVector);

   // ease speed cap toward target
   if (d.mv.moveSpeedCap < d.mv.moveSpeedCapTarget) {
      d.mv.moveSpeedCap += cDudeSpeedCapEasing;      
   }
   else {
      d.mv.moveSpeedCap -= cDudeSpeedCapEasing;
   }
   clamp(d.mv.moveSpeedCap, 0, d.mv.moveSpeedCapTarget);

   // add the movevector scaled against acceleration to velocity and cap it
   d.phy.velocity = v2CapLength(d.phy.velocity + d.mv.moveVector * cDudeAcceleration, d.mv.moveSpeedCap);
}

// rotate facing vector toward the facevector
void dudeUpdateRotation(Dude& d) {
   if (v2LenSquared(d.mv.faceVector) > 0.0f) {
      d.mv.facing = v2Normalized(v2RotateTowards(d.mv.facing, d.mv.faceVector, v2FromAngle(cDudeRotationSpeed)));
   }
}



void dudeUpdateBehavior(Dude& dude) {

   if (dude.state != DudeState_FREE) {
      return;
   }

   auto& target = *dude.ai.target;
   auto dist = v2Dist(dude.phy.pos, target.phy.pos);

   if (dude.ai.attack) {
      auto unitToTarget = v2Normalized(target.phy.pos - dude.phy.pos);
      dude.mv.faceVector = unitToTarget;

      if (dist > 50.0f) {
         auto vec = v2Normalized(target.phy.pos - dude.phy.pos);
         dude.mv.moveVector = dude.mv.faceVector = vec;
      }
      else {
         dudeBeginAttack(dude, 1, 0);
         dude.ai.attack = false;
      }
      return;
   }

   if (dude.ai.started++ > (rand() % 3000) + 3000) {
      dude.ai.dir = -dude.ai.dir;
      dude.ai.started = 0;

      //if (v2Dot(-target.mv.faceVector, dude.mv.faceVector) < -0.5f) {
         dude.ai.attack = true;
      //}
   }

   // move to range
   if (dist > 300.0f) {
      auto vec = v2Normalized(target.phy.pos - dude.phy.pos);
       dude.mv.faceVector = vec;
       dude.mv.moveVector = v2Normalized(vec + v2Orthogonal(vec) * (f32)dude.ai.dir);
   }
   else {
      auto unitToTarget = v2Normalized(target.phy.pos - dude.phy.pos);
      dude.mv.faceVector = unitToTarget;

      if (dist < 200.0f) {
         // too close, back up
         dude.mv.moveVector = v2Normalized(-unitToTarget + v2Orthogonal(unitToTarget) * (f32)dude.ai.dir);

         //if (dist < 100.0f) {
         //   dudeBeginAttack(dude, 1, 0);
         //}
      }
      else {
         
         dude.mv.moveVector = v2Orthogonal(unitToTarget) * 0.1f * (f32)dude.ai.dir;
      }
   }
}

//bool _attackCollision(Dude& attacker, Dude& defender) {
//   auto origin = attacker.phy.pos + attacker.weaponVector * attacker.phy.circle.size;
//   auto defendPos = v2Rotate(defender.phy.pos - origin, v2Conjugate(attacker.weaponVector));
//
//   auto wbox = attacker.moveset.swings[attacker.combo].hitbox;
//   //wbox.x += origin.x;
//   //wbox.y += origin.y;
//
//   return circleVsAabb(defendPos, defender.phy.circle.size, wbox);
//}




struct Game {
   GameData data;

   ShaderHandle shader = 0;

   Mesh mesh, meshUncentered;
   FBO unlitScene, litScene, output;

   Dude maindude;
   DynamicArray<Dude> baddudes;

   Time lastMouseMove;
   Time lastUpdate;
   bool mouseActive;

   Map map = { {10000, 10000} };
   Camera cam = { { 0, 0, 426, 240} };// 640, 360 } };
};

static Game* g_game = nullptr;
GameData* gameDataGet() {
   return &g_game->data;
}

static void _gameDataInit(GameData* game, StringView assetsFolder) {
#ifndef  _DEBUG
   game->imgui.showUI = false;
#endif //  NDEBUG
}

Game* gameCreate(StringView assetsFolder) {
   g_game = new Game();
   _gameDataInit(&g_game->data, assetsFolder);
   return g_game;
}

void gameDestroy(Game* game) {
   delete game;
}

FBO const& gameGetOutputFBO(Game* game) { return game->output; }




static Texture _textureBuildFromFile(const char* path, TextureConfig const& cfg = {}) {
   u64 sz = 0;
   i32 x, y, comp;
   x = y = comp = 0;
   auto mem = fileReadBinary(path, &sz);
   auto png = stbi_load_from_memory(mem, (i32)sz, &x, &y, &comp, 4);
   auto out = render::textureBuild((ColorRGBA*)png, { x, y }, cfg);

   free(mem);
   free(png);
   return out;
}

static void _createGraphicsObjects(Game* game){
   auto vertex = fileReadString("assets/vertex.glsl");
   auto fragment = fileReadString("assets/fragment.glsl");

   game->shader = render::shaderBuild(vertex, fragment);

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

   game->mesh = render::meshBuild(vbo, 6);
   game->meshUncentered = render::meshBuild(vboUncentered, 6);

   auto& res = ConstantsGet().resolution;
   game->unlitScene = render::fboBuild({ res.x, res.y });
   game->litScene = render::fboBuild({ res.x, res.y });
   game->output = render::fboBuild({ res.x, res.y });

   g_textures[GameTextures_Dude] = _textureBuildFromFile("assets/dude.png");
   g_textures[GameTextures_Target] = _textureBuildFromFile("assets/target.png");
   g_textures[GameTextures_Light] = _textureBuildFromFile("assets/light3.png");
   g_textures[GameTextures_Circle] = _textureBuildFromFile("assets/circle.png", { RepeatType_CLAMP , FilterType_LINEAR });
   g_textures[GameTextures_ShittySword] = _textureBuildFromFile("assets/shittysword.png");
   g_textures[GameTextures_GemEmpty] = _textureBuildFromFile("assets/gemempty.png");
   g_textures[GameTextures_GemFilled] = _textureBuildFromFile("assets/gemfilled.png");
   g_textures[GameTextures_Tile] = _textureBuildFromFile("assets/tile.png", { RepeatType_REPEAT , FilterType_NEAREST });
   
   free(vertex);
   free(fragment);
}

static Dude _createDude(Game* game) {
   Dude out;

   auto tex = gameTexture(GameTextures_Dude);

   out.c = White;
   out.moveset = _createMoveSet();
   out.phy.pos = { 50,50 };
   out.phy.circle.size = 10.0f;
   out.phy.maxSpeed = cDudeMoveSpeed;
   out.phy.invMass = 0.0f;
   out.renderSize = {(f32)tex.sz.x, (f32)tex.sz.y};
   out.texture = tex.handle;

   out.status.stamina = out.status.staminaMax = 4;

   return out;
}

static Dude _createEnemy(Float2 pos, f32 size) {
   Dude out;

   auto tex = gameTexture(GameTextures_Dude);

   out.moveset = _createMoveSet();
   out.c = {1.0f, 0.3f, 0.3f, 1.0f};
   out.phy.pos = pos;
   out.phy.circle.size = size;
   out.phy.velocity = { 0,0 };
   out.phy.maxSpeed = cDudeMoveSpeed;
   out.phy.invMass =(f32)(rand()%50 + 5) / 100.0f;
   out.renderSize = { (f32)tex.sz.x, (f32)tex.sz.y };
   out.texture = tex.handle;
   out.status.stamina = out.status.staminaMax = 4;

   return out;
}

void gameBegin(Game*game) {
   game->lastUpdate = appGetTime();

   _createGraphicsObjects(game);
   game->maindude = _createDude(game);

   for (int i = 0; i < DUDE_COUNT; ++i) {
      auto e = _createEnemy({ (f32)(rand() % 1820) + 100, (f32)(rand() % 980) + 100 }, 10.0f);
      e.ai.target = &game->maindude;
      game->baddudes.push_back(e);
   }

}




static void _renderSwing(Dude&dude) {
   auto origin = dude.phy.pos + dude.atk.weaponVector * dude.phy.circle.size;

   auto model = Matrix::identity();
   model *= Matrix::translate2f(origin);
   model *= Matrix::rotate2D(v2Angle(dude.atk.weaponVector));
   model *= Matrix::translate2f({dude.atk.swing.hitbox.x, dude.atk.swing.hitbox.y});
   model *= Matrix::scale2f({ dude.atk.swing.hitbox.w, dude.atk.swing.hitbox.h });

   uber::resetToDefault();
   uber::set(Uniform_ModelMatrix, model);
   render::textureBind(gameTextureHandle(GameTextures_ShittySword));
   render::meshRender(g_game->meshUncentered);
}

static void _renderDude(Dude& dude) {
   auto model = Matrix::identity();

   model *= Matrix::translate2f(dude.phy.pos);
   model *= Matrix::rotate2D(v2Angle(dude.mv.facing));
   model *= Matrix::scale2f(dude.renderSize);

   uber::resetToDefault();
   uber::set(Uniform_Color, dude.c);
   uber::set(Uniform_ModelMatrix, model);
   render::textureBind(dude.texture);
   render::meshRender(g_game->mesh);

   if (gameDataGet()->imgui.showCollisionDebugging) {

      model = Matrix::translate2f(dude.phy.pos);
      model *= Matrix::scale2f({ dude.phy.circle.size * 2, dude.phy.circle.size * 2 });
      uber::set(Uniform_Color, Cyan);
      uber::set(Uniform_Alpha, 01.0f);
      uber::set(Uniform_ModelMatrix, model);

      render::textureBind(gameTextureHandle(GameTextures_Circle));
      render::meshRender(g_game->mesh);
   }
   
   if (dude.state == DudeState_ATTACKING) {
      _renderSwing(dude);
   }
}

static void _renderTarget(Float2 pos, ColorRGBAf color, f32 sz) {
   auto model = Matrix::identity();

   model *= Matrix::translate2f(pos);
   model *= Matrix::scale2f({ sz,sz });

   uber::resetToDefault();
   uber::set(Uniform_Color, color);
   uber::set(Uniform_ModelMatrix, model);
   render::textureBind(gameTextureHandle(GameTextures_Target));
   render::meshRender(g_game->mesh);
}

static void _addLight(Float2 size, Float2 pos, ColorRGBAf c) {
   auto model = Matrix::translate2f(pos);
   model *= Matrix::scale2f(size);

   uber::set(Uniform_Color, c);
   uber::set(Uniform_ModelMatrix, model);
   render::textureBind(gameTextureHandle(GameTextures_Light));
   render::meshRender(g_game->mesh);
}
//
//static void _populateLightLayer(Game* game) {
//   auto& c = ConstantsGet();
//   //auto& res = c.resolution;
//   auto& vp = game->cam.viewport;
//
//   
//
//   //auto view = Matrix::ortho(0, (float)res.x, 0, (float)res.y, -1, 1);
//   //uber::set(Uniform_ViewMatrix, view);
//
//
//   render::fboBind(game->lightfbo);
//   render::setBlendMode(BlendMode_PURE_ADD);
//
//   render::viewport({ 0,0, game->lightfbo.sz.x, game->lightfbo.sz.y });
//   render::clear({0.f,0.f,0.f,0.0f});
//
//   uber::resetToDefault();
//
//   _addLight({ 100, 100 }, game->maindude.phy.pos, Yellow);
//
//   for (auto&& d : game->baddudes) {
//      f32 lsz = 100;
//      _addLight({ lsz, lsz }, d.phy.pos, Yellow);
//   }
//
//   //f32 lsz = 250;
//   //_addLight({ lsz, lsz }, { 200, 600 }, Yellow);
//   //_addLight({ lsz, lsz }, { 800, 300 }, Yellow);
//   //_addLight({ lsz, lsz }, { 100, 1000 }, Yellow);
//   //_addLight({ lsz, lsz }, { 1200, 500 }, Yellow);
//   //_addLight({ lsz, lsz }, { 1800, 200 }, Yellow);
//   //_addLight({ lsz, lsz }, { 1800, 800 }, Yellow);
//   //_addLight({ lsz, lsz }, { 900, 800 }, Yellow);
//
//   auto al = gameDataGet()->imgui.ambientLight;
//   uber::set(Uniform_ColorOnly, true);
//   uber::set(Uniform_Color, sRgbToLinear(ColorRGBAf{ al,al,al,al }));
//   uber::set(Uniform_ModelMatrix, Matrix::scale2f({ (f32)vp.w, (f32)vp.h }));
//   uber::set(Uniform_ViewMatrix, Matrix::ortho(0, vp.w, 0, vp.h, -1, 1));
//   render::meshRender(g_game->meshUncentered);
//}

static void _renderFloor(Game* game) {
   auto& c = ConstantsGet();

   Float2 fres = { game->map.size.x, game->map.size.y };
   Float2 tileSize = { 16, 16 };
   
   uber::resetToDefault();
   uber::set(Uniform_TextureMatrix, Matrix::scale2f({ fres.x / tileSize.x, fres.y / tileSize.y }));
   uber::set(Uniform_ModelMatrix, Matrix::scale2f(fres));
   render::textureBind(gameTextureHandle(GameTextures_Tile));
   render::meshRender(g_game->meshUncentered);

   if (gameDataGet()->imgui.showCollisionDebugging) {
      Rectf testrect = { 300,300, 500, 200 };

      uber::set(Uniform_ColorOnly, true);
      uber::set(Uniform_Color, circleVsAabb(game->maindude.phy.pos, game->maindude.phy.circle.size, testrect) ? Red : Cyan);
      uber::set(Uniform_ModelMatrix, Matrix::translate2f({ testrect.x, testrect.y }) * Matrix::scale2f({ testrect.w, testrect.h }));
      render::meshRender(g_game->meshUncentered);
   }


   
}
//
//static void _renderLightLayer(Game* game) {
//
//   render::setBlendMode(BlendMode_LIGHTING);
//
//   uber::resetToDefault();
//   auto& vp = game->cam.viewport;
//   auto view = Matrix::ortho(0, vp.w, 0, vp.h, -1, 1);
//   uber::set(Uniform_ViewMatrix, view);
//   uber::set(Uniform_ModelMatrix, Matrix::scale2f({ (float)vp.w, (float)vp.h }));
//   render::textureBind(game->lightfbo.tex);
//   render::meshRender(g_game->meshUncentered);
//}

static void _renderGameUI(Game* game) {
   render::setBlendMode(BlendMode_NORMAL);

   uber::resetToDefault();
   auto& vp = game->cam.viewport;
   auto view = Matrix::ortho(0, vp.w, 0, vp.h, -1, 1);
   uber::set(Uniform_ViewMatrix, view);

   
   /*if (game->maindude.stamina < game->maindude.staminaMax)*/ {
      auto tsz = gameTexture(GameTextures_GemFilled).sz;
      Float2 gemSize = { (f32)tsz.x, (f32)tsz.y };
      f32 gemSpace = 0.0f;
      auto w = (gemSize.x + gemSpace) * game->maindude.status.staminaMax;

      //Float2 staminaCorner = { game->maindude.phy.pos.x - w / 2.0f, game->maindude.phy.pos.y + game->maindude.phy.circle.size + 20 };
      Float2 staminaCorner = { 10,10 };

      for (int i = 0; i < game->maindude.status.staminaMax; ++i) {
         auto model = Matrix::translate2f(staminaCorner + Float2{ (gemSize.x + gemSpace) * i, 0 }) *  Matrix::scale2f(gemSize);
         uber::set(Uniform_ModelMatrix, model);
         render::textureBind(gameTextureHandle(i < game->maindude.status.stamina ? GameTextures_GemFilled : GameTextures_GemEmpty));
         render::meshRender(g_game->meshUncentered);
      }
   }
   

}

static void _renderScene(Game* game) {
   auto& c = ConstantsGet();
   //auto& res = ConstantsGet().resolution;   

   auto vp = game->cam.viewport;
   auto view = Matrix::ortho(vp.x, vp.x + vp.w, vp.y, vp.y + vp.h, 1, -1);
   render::shaderSetActive(game->shader);

   uber::resetToDefault(true);
   uber::set(Uniform_ViewMatrix, view, true);

   
   // render unlit scene
   render::fboBind(game->unlitScene);

   render::setBlendMode(BlendMode_NORMAL);
   render::viewport({ 0,0, game->unlitScene.sz.x, game->unlitScene.sz.y });
   render::clear(Black);

   _renderFloor(game);

   for (auto&& d : game->baddudes) {
      _renderDude(d);
   }

   _renderDude(game->maindude);

   //_renderDude(*game->maindude);

   if (game->data.imgui.showMovementDebugging) {
      const static f32 moveTargetDist = 100.0f;
      const static f32 aimTargetDist = 200.0f;
      auto &io = game->data.io;
      auto &dude = game->maindude;

      _renderTarget(dude.phy.pos + dude.phy.velocity * moveTargetDist, Cyan, 40);
      _renderTarget(dude.phy.pos + v2Normalized(dude.mv.facing) * aimTargetDist, Red, 40);

      _renderTarget(dude.phy.pos + io.leftStick * moveTargetDist, DkGreen, 30);
      _renderTarget(dude.phy.pos + dude.mv.moveVector * moveTargetDist, Magenta, 30);

      _renderTarget(dude.phy.pos + io.rightStick * aimTargetDist, Yellow, 30);
      _renderTarget(dude.phy.pos + dude.mv.faceVector * aimTargetDist, LtGray, 30);
   }
   

   render::fboBind(game->output);
   render::setBlendMode(BlendMode_ADDITIVE);
   render::viewport({ 0,0, game->output.sz.x, game->output.sz.y });
   render::clear({0,0,0,0});

   view = Matrix::ortho(0, vp.w, 0, vp.h, 1, -1);
   uber::set(Uniform_ViewMatrix, view, true);
   uber::resetToDefault();
   
   uber::set(Uniform_ModelMatrix, Matrix::scale2f({ (f32)vp.w, (f32)vp.h }));
   uber::set(Uniform_SceneSize, Float2{ (f32)vp.w, (f32)vp.h });
   uber::set(Uniform_PointLight, true);
   //uber::set(Uniform_PointLightIntensity, 0.1f);

   //{
   //   auto pos = (game->maindude.phy.pos - Float2{ vp.x, vp.y });
   //   auto sz = 100.0f;

   //   uber::set(Uniform_PointLightPos, pos);
   //   uber::set(Uniform_PointLightSize, sz);
   //   render::textureBind(game->unlitScene.tex);
   //   render::meshRender(game->meshUncentered);
   //}

   int i = 0;

   for (auto&& d : game->baddudes) {
      auto pos = (d.phy.pos - Float2{ vp.x, vp.y });
      auto sz = 40.0f;

      static ColorRGBAf c[] = {Red, Green, Blue};

      uber::set(Uniform_Color, c[i++ % 3]);
      //uber::set(Uniform_Alpha, 0.01f);
      uber::set(Uniform_PointLightPos, pos);
      uber::set(Uniform_PointLightSize, sz);

      render::textureBind(game->unlitScene.tex);
      render::meshRender(game->meshUncentered);

   }   

   render::fboBind({});



   //_renderLightLayer(game);

   //_renderGameUI(game);

   

   //render::setBlendMode(BlendMode_NORMAL);
   //glBlendFunc(GL_ONE, GL_ZERO);

   //render::shaderSetActive(game->shader);
   //render::fboBind( game->outputfbo );
   //
   //render::viewport({ 0,0, res.x, res.y });
   //render::clear({ 1, 1, 1, 1 });

   //render::uSetColor(u::color, {1, 1, 1, 1});
   //render::uSetMatrix(u::viewMatrix, Matrix::ortho(0, (float)res.x, 0, (float)res.y, -1, 1));
   //render::uSetMatrix(u::texMatrix, Matrix::identity());
   //render::uSetMatrix(u::modelMatrix, Matrix::scale2f({ (float)game->outputfbo.sz.x, (float)game->outputfbo.sz.y }));
   //render::uSetTextureSlot(u::diffuse, 0);
   //render::textureBind(game->fbo.tex);

   //glEnable(GL_FRAMEBUFFER_SRGB);
   //render::meshRender(g_game->meshUncentered);
   //glDisable(GL_FRAMEBUFFER_SRGB);

   

   

   //render::shaderSetActive(game->shader);
   //render::viewport({ 0, 0, res.x, res.y });
   //render::clear({ 1, 1, 1, 1 });
   //render::uSetColor(u::color, { 1, 1, 1, 1 });
   //render::uSetMatrix(u::viewMatrix, Matrix::ortho(0, (float)res.x, (float)res.y, 0, -1, 1));
   //render::uSetMatrix(u::texMatrix, Matrix::identity());
   //render::uSetMatrix(u::modelMatrix, Matrix::scale2f({ (float)game->outputfbo.sz.x, (float)game->outputfbo.sz.y }));
   //render::uSetTextureSlot(u::diffuse, 0);
   //render::textureBind(game->outputfbo.tex);

   //
   //render::meshRender(g_game->meshUncentered);
   

}

void gameRender(Game*game) {
   
   _renderScene(game);
   
}




bool gameProcessEvent(Game*game, SDL_Event* event) {
   auto &io = game->data.io;

   switch (event->type) {
   case SDL_KEYUP:
   case SDL_KEYDOWN: {
      bool pressed = event->type == SDL_KEYDOWN;
      GameButton btn;

      switch (event->key.keysym.scancode) {
      case SDL_SCANCODE_W:
         btn = GameButton_UP;

         if (pressed) {
            if (!event->key.repeat) io.leftStick.y = -1.0f;
         }
         else if (!io.buttonDown[GameButton_DOWN]) {
            io.leftStick.y = 0.0f;
         }

         break;
      case SDL_SCANCODE_A:
         btn = GameButton_LEFT;

         if (pressed) {
            if (!event->key.repeat) io.leftStick.x = -1.0f;
         }
         else if (!io.buttonDown[GameButton_RIGHT]) {
            io.leftStick.x = 0.0f;
         }
         break;
      case SDL_SCANCODE_S:
         btn = GameButton_DOWN;

         if (pressed) {
            if (!event->key.repeat) io.leftStick.y = 1.0f;
         }
         else if (!io.buttonDown[GameButton_UP]) {
            io.leftStick.y = 0.0f;
         }
         break;
      case SDL_SCANCODE_D:
         btn = GameButton_RIGHT;

         if (pressed) {
            if (!event->key.repeat) io.leftStick.x = 1.0f;
         }
         else if (!io.buttonDown[GameButton_LEFT]) {
            io.leftStick.x = 0.0f;
         }
         break;
      default: return false;
      }

      if (!event->key.repeat) {
         io.leftStick = v2Normalized(io.leftStick);
      }

      if (!io.buttonDown[btn] && pressed) {
         io.buttonPressed[btn] = true;
      }
      if (io.buttonDown[btn] && !pressed) {
         io.buttonReleased[btn] = true;
      }
      io.buttonDown[btn] = pressed;

      return true;
   }  break;

   case SDL_MOUSEMOTION:
      game->lastMouseMove = appGetTime();
      game->mouseActive = true;
      break;

   case SDL_MOUSEBUTTONDOWN:
   case SDL_MOUSEBUTTONUP: {
      bool pressed = event->type == SDL_MOUSEBUTTONDOWN;
      if (!io.buttonDown[GameButton_RT] && pressed) {
         io.buttonPressed[GameButton_RT] = true;
      }

      if (io.buttonDown[GameButton_RT] && !pressed) {
         io.buttonReleased[GameButton_RT] = true;
      }

      io.buttonDown[GameButton_RT] = pressed;
      return true;
   }


   case SDL_CONTROLLERDEVICEADDED:
      SDL_GameControllerOpen(0);
      return true;

   case SDL_CONTROLLERBUTTONUP:
   case SDL_CONTROLLERBUTTONDOWN:
      switch (event->cbutton.button) {
      case SDL_CONTROLLER_BUTTON_A: break;
      case SDL_CONTROLLER_BUTTON_B: break;
      case SDL_CONTROLLER_BUTTON_X: break;
      case SDL_CONTROLLER_BUTTON_Y: break;
      case SDL_CONTROLLER_BUTTON_BACK: break;
      case SDL_CONTROLLER_BUTTON_START: break;
      case SDL_CONTROLLER_BUTTON_LEFTSTICK: break;
      case SDL_CONTROLLER_BUTTON_RIGHTSTICK: break;
      case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: break;
      case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: break;
      case SDL_CONTROLLER_BUTTON_DPAD_UP: break;
      case SDL_CONTROLLER_BUTTON_DPAD_DOWN: break;
      case SDL_CONTROLLER_BUTTON_DPAD_LEFT: break;
      case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: break;
      default: return false;
      }
      return true;

   case SDL_CONTROLLERAXISMOTION:
   {
      auto value = event->caxis.value / (float)(INT16_MAX + 1);
      bool pressed = true;
      GameButton btn;
      if (fabs(value) < AXIS_DEADZONE) {
         value = 0.0f;
         pressed = false;
      }

      switch (event->caxis.axis) {
      case SDL_CONTROLLER_AXIS_LEFTX: io.leftStick.x = value; btn = value < 0.0f ? GameButton_LEFT : GameButton_RIGHT; break;
      case SDL_CONTROLLER_AXIS_LEFTY: io.leftStick.y = value; btn = value < 0.0f ? GameButton_UP : GameButton_DOWN; break;
      case SDL_CONTROLLER_AXIS_RIGHTX: io.rightStick.x = value; btn = value < 0.0f ? GameButton_LEFT : GameButton_RIGHT; break;
      case SDL_CONTROLLER_AXIS_RIGHTY: io.rightStick.y = value; btn = value < 0.0f ? GameButton_UP : GameButton_DOWN; break;
      case SDL_CONTROLLER_AXIS_TRIGGERLEFT: io.leftTrigger = value; btn = GameButton_LT; break;
      case SDL_CONTROLLER_AXIS_TRIGGERRIGHT: io.rightTrigger = value; btn = GameButton_RT; break;
      default: return false;
      }

      if (!io.buttonDown[btn] && pressed) {
         io.buttonPressed[btn] = true;
      }

      if (io.buttonDown[btn] && !pressed) {
         io.buttonReleased[btn] = true;
      }

      io.buttonDown[btn] = pressed;
   } return true;;
   }

   return false;
}

void gameHandleInput(Game*game) {
   //auto& res = ConstantsGet().resolution;
   auto& vpScreen = game->data.imgui.vpScreenArea;
   auto& vpCamera = game->cam.viewport;

   auto &imio = ImGui::GetIO();
   auto& mPos = ImGui::GetIO().MousePos;

   auto& io = game->data.io;

   io.mousePos = {
      vpCamera.x + (mPos.x - vpScreen.x) / vpScreen.w * vpCamera.w,
      vpCamera.y + (mPos.y - vpScreen.y) / vpScreen.h * vpCamera.h
   };

   // update buttons
   for (GameButton b = 0; b < GameButton_COUNT; ++b) {
      io.buttonPressed[b] = io.buttonReleased[b] = false;
   }
}




void gameUpdate(Game* game) {   
   auto time = appGetTime();
   auto dt = time - game->lastUpdate;
   auto ms = dt.toMilliseconds();

   auto&vp = game->cam.viewport;
   auto&dudePos = game->maindude.phy.pos;
   auto&dudeFace = game->maindude.mv.facing;

   

   if (ms > 32) {
      // something bad happened and we spiked hard
      ms = 32;
      game->lastUpdate = time - timeMillis(ms);
   }

   // build physics system
   DynamicArray<PhyObject*> pObjs;
   pObjs.push_back(&game->maindude.phy);
   for (auto && d : game->baddudes) {
      pObjs.push_back(&d.phy);
   }

   for (Milliseconds i = 0; i < ms; ++i) {


      dudeUpdateState(game->maindude);
      dudeApplyInput(game->maindude);   
      dudeUpdateRotation(game->maindude);
      dudeUpdateVelocity(game->maindude);
      

      for (auto && d : game->baddudes) {
         dudeUpdateState(d);
         dudeUpdateBehavior(d);  
         dudeUpdateRotation(d);
         dudeUpdateVelocity(d);
      }

      updatePhyPositions(pObjs);
   }

   vp.x = clamp(dudePos.x - (vp.w / 2), 0, game->map.size.x - vp.w);
   vp.y = clamp(dudePos.y - (vp.h / 2), 0, game->map.size.y - vp.h);

   game->lastUpdate += timeMillis(ms);

   // imgui output
   uiEditDude(game->maindude);
   gameDoUI(game);
}
