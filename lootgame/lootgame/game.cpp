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
#define DUDE_COUNT 1


struct Game;
static Game* g_game = nullptr;
void gameStateBeginYouDied(Game* game);
void DEBUG_gameSpawnDude(Game* game);

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
   GameTextures_DudeNormals,
   GameTextures_Target,
   GameTextures_Light,
   GameTextures_Circle,
   GameTextures_ShittySword,
   GameTextures_Tile,
   GameTextures_TileNormals,
   GameTextures_GemEmpty,
   GameTextures_GemFilled,
   GameTextures_HeartEmpty,
   GameTextures_HeartFilled,

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
   SwingPhase_Lunge,
   SwingPhase_Swing,
   SwingPhase_Cooldown
};

struct AttackSwing {
   f32 lungeDist; // character will luinge forward between windup and swing
   f32 lungeSpeed; // perms

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
   Rectf hitbox = { -2.0f, -4.5f, 28, 9 };

   out.swings[0].swipeAngle =    120.0f;
   out.swings[0].lungeSpeed =    0.3f;
   out.swings[0].lungeDist =     0.0f;
   out.swings[0].windupDur =     150;
   out.swings[0].swingDur =      250;
   out.swings[0].cooldownDur =   300;
   out.swings[0].hitbox = hitbox;

   out.swings[1] = out.swings[0];
   out.swings[1].windupDur = 50;
   out.swings[1].swingDur = 150;
   out.swings[1].cooldownDur = 500;

   out.swings[2] = out.swings[0];
   out.swings[2].windupDur = 50;
   out.swings[2].swingDur = 100;
   out.swings[2].cooldownDur = 200;

   //out.swings[1].swipeAngle = 180.0f;
   //out.swings[1].lungeSpeed = 0.25f;
   //out.swings[1].windupDur = 100;
   //out.swings[1].swingDur = 250;
   //out.swings[1].cooldownDur = 250;
   //out.swings[1].hitbox = hitbox;

   //out.swings[2].swipeAngle = 0.0f;
   //out.swings[2].lungeSpeed = 0.5f;
   //out.swings[2].windupDur = 100;
   //out.swings[2].swingDur = 250;
   //out.swings[2].cooldownDur = 100;
   //out.swings[2].hitbox = hitbox;

   return out;
}

f32 cFloorHeight = 0.0f;
f32 cDudeHeight = 0.1f;
f32 cLightHeight = 0.2f;
f32 cLightConstant =    0.00f;
f32 cLightLinear =      0.01f;
f32 cLightQuadratic =   0.001f;


f32 cDudeAcceleration =     0.005f;
f32 cDudeRotationSpeed =    0.010f;
f32 cDudeMoveSpeed =        0.100f;
f32 cDudeDashSpeed =        0.300f;
f32 cDudeSpeedCapEasing =   0.0003f;
f32 cDudeBackwardsPenalty = 0.250f;
f32 cDudeDashDistance =     50.0f;
f32 cDudeKnockbackDistance = 0.0f;

Milliseconds cDudePostDashCooldown = 100;
Milliseconds cDudeBaseStaminaTickRecoveryTime = 500;

Milliseconds cCooldownOnDamagedStaminaEmpty = 1000;
Milliseconds cCooldownOnDamagedStamina = 250;
Milliseconds cCooldownOnDamagedHealth = 500;

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

   DynamicArray<Dude*> hits;
};

struct CooldownState {
   Milliseconds duration;
};

struct DashState {
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
   DudeState_DEAD
};

struct Status {
   int stamina, staminaMax;
   int health, healthMax;
};

// shove keeps track of a push of constant speed for a distance
// shoves can happen independant of state
struct Shove {
   Float2 startPos;
   Milliseconds start;
   Milliseconds dur;
   f32 speed; // only stored so we can restore speed correctly
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

   bool shoved = false;
   Shove shove;

   
   Milliseconds stateClock; // incremented by one every ms

   
   MoveSet moveset;   
};

static void uiEditDude(Dude& dude) {
   if (ImGui::Begin(format("Dude Editor##%p", &dude).c_str(), 0, ImGuiWindowFlags_AlwaysAutoResize)) {

      ImGui::SetNextTreeNodeOpen(true, ImGuiCond_Appearing);
      if (ImGui::CollapsingHeader("Lighting")) {
         ImGui::Indent();

         //ImGui::InputFloat("Floor Height", &cFloorHeight, 0.01f, 0.1f, 4);
         //ImGui::InputFloat("Dude Height", &cDudeHeight, 0.01f, 1.0f, 4);
         //ImGui::InputFloat("Light Height", &cLightHeight, 0.01f, 1.0f, 4);

         ImGui::DragFloat("Floor Height", &cFloorHeight, 0.01f, 0.0f, 2.0f);
         ImGui::DragFloat("Dude Height", &cDudeHeight, 0.01f, 0.0f, 2.0f);
         ImGui::DragFloat("Light Height", &cLightHeight, 0.01f, 0.0f, 2.0f);

         ImGui::DragFloat("Constant Attenuation", &cLightConstant, 0.01f, 0.0f, 10.0f);
         ImGui::DragFloat("Linear Attenuation", &cLightLinear, 0.01f, 0.0f, 10.0f);
         ImGui::DragFloat("Quadratic Attenuation", &cLightQuadratic, 0.01f, 0.0f, 10.0f);



         ImGui::Unindent();
      }

      ImGui::SetNextTreeNodeOpen(true, ImGuiCond_Appearing);
      if (ImGui::CollapsingHeader("Movement")) {
         ImGui::Indent();

         //ImGui::InputFloat("Acceleration", &cDudeAcceleration, 0.001f, 0.01f, 4);
         ImGui::InputFloat("Rotation Speed", &cDudeRotationSpeed, 0.01f, 0.1f, 4);
         if (ImGui::InputFloat("Move Speed", &dude.phy.maxSpeed, 0.01f, 0.1f, 4)) {
            cDudeMoveSpeed = dude.phy.maxSpeed;
         }
         ImGui::InputFloat("Dash Speed", &cDudeDashSpeed, 0.01f, 0.1f, 4);
         ImGui::InputFloat("Dash Distance", &cDudeDashDistance, 10.0f, 10.0f, 0);
         ImGui::InputFloat("Knockback Distance", &cDudeKnockbackDistance, 10.0f, 10.0f, 0);
         ImGui::InputFloat("Speed Cap Accel", &cDudeSpeedCapEasing, 0.001f, 0.01f, 4);
         ImGui::InputFloat("Backward Penalty", &cDudeBackwardsPenalty, 0.1f, 0.01f, 4);

         f32 ratio = dude.mv.moveSpeedCap / 0.5f;
         ImGui::ProgressBar(ratio, ImVec2(-1, 0), "Speed Cap");

         ImGui::Unindent();
      }

      ImGui::SetNextTreeNodeOpen(true, ImGuiCond_Appearing);
      if (ImGui::CollapsingHeader("Timing")) {
         ImGui::Indent();

         Milliseconds shrt = 50, lng = 100;
         ImGui::InputScalar("Post-Dash CD", ImGuiDataType_U64, &cDudePostDashCooldown, &shrt, &lng);
         ImGui::InputScalar("Stamina Base Tick Speed", ImGuiDataType_U64, &cDudeBaseStaminaTickRecoveryTime, &shrt, &lng);

         ImGui::InputScalar("Damaged Stamina Empty CD", ImGuiDataType_U64, &cCooldownOnDamagedStaminaEmpty, &shrt, &lng);
         ImGui::InputScalar("Damaged Stamina CD", ImGuiDataType_U64, &cCooldownOnDamagedStamina, &shrt, &lng);
         ImGui::InputScalar("Damaged Health CD", ImGuiDataType_U64, &cCooldownOnDamagedHealth, &shrt, &lng);
         ImGui::Unindent();

      }

      ImGui::SetNextTreeNodeOpen(true, ImGuiCond_Appearing);
      if (ImGui::CollapsingHeader("Attack")) {
         ImGui::Indent();
         int i = 0;
         for (auto &&swing : dude.moveset.swings) {
            ImGui::PushID(&swing);

            if (ImGui::CollapsingHeader(format("Swing %d", i++).c_str())) {
               ImGui::Indent();

               ImGui::InputFloat("Angle", &swing.swipeAngle, 10.0f, 10.0f, 1);
               Milliseconds shrt = 50, lng = 100;
               ImGui::InputFloat("Lunge Speed", &swing.lungeSpeed, 0.01f, 0.1f, 4);
               ImGui::InputFloat("Lunge Distance", &swing.lungeDist, 10.0f, 10.0f, 0);
               ImGui::InputScalar("Swing Duration", ImGuiDataType_U64, &swing.swingDur, &shrt, &lng);
               ImGui::InputScalar("Windup Duration", ImGuiDataType_U64, &swing.windupDur, &shrt, &lng);
               ImGui::InputScalar("Cooldown Duration", ImGuiDataType_U64, &swing.cooldownDur, &shrt, &lng);

               ImGui::Unindent();
            }
            
            ImGui::PopID();
         }

         ImGui::Unindent();
         
      }
      

      



   }
   ImGui::End();
}

void dudeBeginCooldown(Dude&d, Milliseconds duration);
void dudeSetState(Dude& d, DudeState state, Milliseconds startTimeOffset = 0);

bool dudeAlive(Dude&d) {
   return d.state != DudeState_DEAD;
}

bool dudeSpendStamina(Dude &d, int stam) {
   if (d.status.stamina == 0) {
      return false;
   }

   d.status.stamina = MAX(0, d.status.stamina - stam);
   return true;
}

void dudeDoDamage(Dude&d) {
   if (d.status.stamina > 0) {
      --d.status.stamina;

      if (d.status.stamina == 0) {
         dudeBeginCooldown(d, cCooldownOnDamagedStaminaEmpty);
      }
      else {
         // attacks only cancel your attack if youre not swinging
         if (d.state != DudeState_ATTACKING || d.atk.swingPhase != SwingPhase_Swing) {
            dudeBeginCooldown(d, cCooldownOnDamagedStamina);
         }
      }
   }
   else {
      // damaged health
      if (--d.status.health == 0) {
         dudeSetState(d, DudeState_DEAD);
      }
      else {
         dudeBeginCooldown(d, cCooldownOnDamagedHealth);
      }
   }

}

void dudeSetState(Dude& d, DudeState state, Milliseconds startTimeOffset) {
   d.state = state;
   d.stateClock = startTimeOffset;
}

Milliseconds calcNextStaminaTickTime(int stam, int max) {
   auto ratio = (f32)stam / max;
   return cDudeBaseStaminaTickRecoveryTime + (Milliseconds)(cDudeBaseStaminaTickRecoveryTime * cosInterp(1 - ratio));
}

void dudeShove(Dude&d, Float2 dir, f32 speed, f32 distance) {
   if (distance <= 0.0f || speed <= 0.0f) {
      return;
   }

   d.shoved = true;
   d.shove.start = 0;
   d.shove.dur = (Milliseconds)(distance / speed);
   d.shove.startPos = d.phy.pos;
   d.shove.speed = speed;

   d.mv.moveVector = dir;
   d.phy.maxSpeed = speed;
   d.mv.moveSpeedCapTarget = d.mv.moveSpeedCap = d.phy.maxSpeed;
   d.phy.velocity = d.mv.moveVector * d.mv.moveSpeedCap;

}
void dudeUpdateShove(Dude& d) {
   if (d.shoved) {
      if (d.shove.start++ >= d.shove.dur) {
         d.mv.moveVector = { 0,0 };
         if (d.shove.speed > cDudeMoveSpeed) {
            d.phy.maxSpeed = cDudeMoveSpeed;
         }
         d.mv.moveSpeedCapTarget = d.mv.moveSpeedCap = d.phy.maxSpeed;
         d.shoved = false;
      }
   }
   
}

void dudeBeginFree(Dude&d) {
   dudeSetState(d, DudeState_FREE);
   d.free.staminaClockStart = 0;
   d.free.nextTickAt = calcNextStaminaTickTime(d.status.stamina, d.status.staminaMax);
}

void dudeUpdateStateFree(Dude& d) {
   // handle stamina regain
   if (d.free.staminaClockStart++ > d.free.nextTickAt && d.status.stamina < d.status.staminaMax) {
      //d.status.stamina = d.status.staminaMax;
      ++d.status.stamina;
      d.free.staminaClockStart = 0;
      d.free.nextTickAt = calcNextStaminaTickTime(d.status.stamina, d.status.staminaMax);
   }
}

void dudeBeginCooldown(Dude&d, Milliseconds duration) {
   dudeSetState(d, DudeState_COOLDOWN);
   d.cd.duration = duration;
   if (!d.shoved) {
      d.mv.moveVector = { 0,0 };
   }
}

void dudeUpdateStateCooldown(Dude& d) {
   if (d.stateClock > d.cd.duration) {
      dudeBeginFree(d);
   }
}

void dudeBeginDash(Dude& d, f32 speed) {
   if (dudeSpendStamina(d, 1)) {
      
      dudeSetState(d, DudeState_DASH);

      Float2 dashDir;
      if (v2LenSquared(d.mv.moveVector) > 0.0f) {
         dashDir = v2Normalized(d.mv.moveVector);
      }
      else {
         dashDir = d.mv.faceVector;
      }

      dudeShove(d, dashDir, cDudeDashSpeed, cDudeDashDistance );
   }
}


void dudeUpdateStateDash(Dude& d) {
   if (!d.shoved) {
      Milliseconds cd = cDudePostDashCooldown;
      //if (d.status.stamina == 0) {
      //   cd = cDudeStaminaEmptyCooldown;
      //}

      dudeBeginCooldown(d, cd);
   }
}

void dudeBeginAttack(Dude& d, int swingDir, int combo) {
   if (dudeSpendStamina(d, 1)) {
      dudeSetState(d, DudeState_ATTACKING);
      d.atk.hits.clear();

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
         // lock facing at time of swipe
         d.mv.faceVector = d.mv.facing;

         if (d.atk.swing.lungeDist > 0.0f) {
            d.atk.swingPhase = SwingPhase_Lunge;
            dudeShove(d, d.mv.faceVector, d.atk.swing.lungeSpeed, d.atk.swing.lungeDist);
         }
         else {
            d.atk.swingPhase = SwingPhase_Swing;
         }
      }
      d.atk.weaponVector = v2Normalized(v2Rotate(d.mv.facing, v2FromAngle(d.atk.swingDir * d.atk.swing.swipeAngle / 2.0f * DEG2RAD)));
      break;
   case SwingPhase_Lunge:
      if (!d.shoved) {
         d.atk.swingPhase = SwingPhase_Swing;
         d.stateClock = 0;
      }
      break;
   case SwingPhase_Swing: {

      auto radsPerMs = (d.atk.swing.swipeAngle * DEG2RAD) / d.atk.swing.swingDur;
      d.atk.weaponVector = v2Rotate(d.atk.weaponVector, v2FromAngle(-d.atk.swingDir * radsPerMs));

      if (d.stateClock >= d.atk.swing.swingDur) {
         d.stateClock -= d.atk.swing.swingDur;
         d.atk.weaponVector = v2Normalized(v2Rotate(d.mv.facing, v2FromAngle(-d.atk.swingDir * d.atk.swing.swipeAngle / 2.0f * DEG2RAD)));
         d.atk.swingPhase = SwingPhase_Cooldown;
      }
   }  break;
   case SwingPhase_Cooldown:
      if (d.stateClock >= d.atk.swing.cooldownDur) {
         dudeBeginFree(d);
      }
      break;
   }
}

bool dudeCheckAttackCollision(Dude& attacker, Dude& defender) {
   if (defender.state == DudeState_DASH) {
      return false;
   }

   auto origin = attacker.phy.pos + attacker.atk.weaponVector * attacker.phy.circle.size;
   auto defendPos = v2Rotate(defender.phy.pos - origin, v2Conjugate(attacker.atk.weaponVector));
   auto wbox = attacker.moveset.swings[attacker.atk.combo].hitbox;

   return circleVsAabb(defendPos, defender.phy.circle.size, wbox);
}

void dudeUpdateState(Dude& d) {

   dudeUpdateShove(d);

   switch (d.state) {
   case DudeState_FREE: dudeUpdateStateFree(d); break;
   case DudeState_COOLDOWN: dudeUpdateStateCooldown(d); break;
   case DudeState_DASH: dudeUpdateStateDash(d); break;
   case DudeState_ATTACKING: dudeUpdateStateAttack(d); break;
   }
   ++d.stateClock;
}

void mainDudeCheckAttackCollisions(Dude& dude, DynamicArray<Dude> &targets) {
   if (dude.state != DudeState_ATTACKING) {
      return;
   }
   if (dude.atk.swingPhase != SwingPhase_Swing) {
      return;
   }
   
   for (auto&& t : targets) {
      if (!dudeAlive(t)) {
         continue;
      }

      bool alreadyHit = false;
      for (auto&&hit : dude.atk.hits) {
         if (hit == &t) {
            alreadyHit = true;
            break;
         }
      }

      if (alreadyHit) {
         continue;
      }

      if (dudeCheckAttackCollision(dude, t)) {
         dudeDoDamage(t);
         auto pushdir = v2Normalized(t.phy.pos - dude.phy.pos);
         dudeShove(t, pushdir, cDudeDashSpeed, cDudeKnockbackDistance);
         dude.atk.hits.push_back(&t);
      }
   }
}

void badDudeCheckAttackCollision(Dude& dude, Dude& t) {
   if (dude.state != DudeState_ATTACKING) {
      return;
   }
   if (dude.atk.swingPhase != SwingPhase_Swing) {
      return;
   }

   if (!dudeAlive(t)) {
      return;
   }

   bool alreadyHit = false;
   for (auto&&hit : dude.atk.hits) {
      if (hit == &t) {
         alreadyHit = true;
         break;
      }
   }

   if (alreadyHit) {
      return;
   }

   if (dudeCheckAttackCollision(dude, t)) {
      dudeDoDamage(t);
      auto pushdir = v2Normalized(t.phy.pos - dude.phy.pos);
      dudeShove(t, pushdir, cDudeDashSpeed, cDudeKnockbackDistance);
      dude.atk.hits.push_back(&t);

      if (!dudeAlive(t)) {
         gameStateBeginYouDied(g_game);
         t.mv.moveVector = { 0,0 };
      }
   }
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
}

void dudeApplyInputAiming(Dude& d) {
   auto& io = gameDataGet()->io;


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
      dudeBeginDash(d, cDudeDashSpeed);
   }
   else if (io.buttonPressed[GameButton_RT]) {
      dudeBeginAttack(d, 1, 0);
   }
}

void dudeApplyInputAttack(Dude& d) {

   switch (d.atk.swingPhase) {
   case SwingPhase_Windup:
      // apply aiming during windup 
      dudeApplyInputAiming(d);
      break;
   }

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
      dudeApplyInputAiming(d);
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

   if (!d.shoved) {
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
   }

   

   // add the movevector scaled against acceleration to velocity and cap it
   d.phy.velocity = v2CapLength(d.phy.velocity + d.mv.moveVector , d.mv.moveSpeedCap);
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



enum GameState_ {
   GameState_ACTION = 0,
   GameState_YOUDIED
};
typedef u16 GameState;

struct GameStateYouDied {
   Milliseconds clock;
};


struct Game {
   GameState state = GameState_ACTION;
   GameData data;

   GameStateYouDied youdied;

   ShaderHandle shader = 0;

   Mesh mesh, meshUncentered;
   FBO 
      unlitScene, 
      lightLayer, 
      litScene,
      UI,
      output;

   Dude maindude;
   DynamicArray<Dude> baddudes;

   Time lastMouseMove;
   Time lastUpdate;
   bool mouseActive;

   Map map = { {10000, 10000} };
   Camera cam = { { 0, 0, 426, 240} };// 640, 360 } };
   bool reloadShader = false;

   int waveSize = 1;
};

void gameStateBeginYouDied(Game* game) {
   game->state = GameState_YOUDIED;
   game->youdied.clock = 0;
}



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



void gameReloadShaders(Game* game) { game->reloadShader = true; }
static int _reloadShader(Game* game) {
   game->reloadShader = false;
   auto vertex = fileReadString("assets/vertex.glsl");
   auto fragment = fileReadString("assets/fragment.glsl");

   auto s = render::shaderBuild(vertex, fragment);

   free(vertex);
   free(fragment);

   if (s == 0) {
      return 0;
   }

   if (game->shader != 0) {
      render::shaderDestroy(game->shader);
   }

   game->shader = s;
   return 1;
}


static Texture _textureBuildFromFile(const char* path, TextureFlag flags = TextureFlag_Defaults) {
   u64 sz = 0;
   i32 x, y, comp;
   x = y = comp = 0;
   auto mem = fileReadBinary(path, &sz);
   auto png = stbi_load_from_memory(mem, (i32)sz, &x, &y, &comp, 4);
   auto out = render::textureBuild({ x, y }, flags, (ColorRGBA*)png);

   free(mem);
   free(png);
   return out;
}

static void _buildGameTextures() {



   g_textures[GameTextures_Dude] = _textureBuildFromFile("assets/dude2.png");
   g_textures[GameTextures_DudeNormals] = _textureBuildFromFile("assets/dudenormal2.png", TextureFlag_ClampedNearest | TextureFlag_Color_RGBA8);
   g_textures[GameTextures_Target] = _textureBuildFromFile("assets/target.png");
   g_textures[GameTextures_Light] = _textureBuildFromFile("assets/light3.png");
   g_textures[GameTextures_Circle] = _textureBuildFromFile("assets/circle.png", TextureFlag_ClampedLinear | TextureFlag_Color_SRGBA);
   g_textures[GameTextures_ShittySword] = _textureBuildFromFile("assets/sword.png");
   g_textures[GameTextures_GemEmpty] = _textureBuildFromFile("assets/gemempty.png");
   g_textures[GameTextures_GemFilled] = _textureBuildFromFile("assets/gemfilled.png");
   g_textures[GameTextures_HeartEmpty] = _textureBuildFromFile("assets/heartempty.png");
   g_textures[GameTextures_HeartFilled] = _textureBuildFromFile("assets/heartfilled.png");
   g_textures[GameTextures_Tile] = _textureBuildFromFile("assets/toldold.png", TextureFlag_RepeatedNearest | TextureFlag_Color_SRGBA);
   g_textures[GameTextures_TileNormals] = _textureBuildFromFile("assets/tilenormal.png", TextureFlag_RepeatedNearest | TextureFlag_Color_RGBA8 | TextureFlag_DisablePremultiply);
}

static void _createGraphicsObjects(Game* game){
   assert(_reloadShader(game));

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
   game->unlitScene = render::fboBuild(res, { 
      render::textureBuild(res, TextureFlag_FBODefaults),   // diffuse
      render::textureBuild(res, TextureFlag_FBODefaults)    // normals
   });
   game->lightLayer = render::fboBuild(res);
   game->litScene   = render::fboBuild(res);
   game->UI         = render::fboBuild(res);
   game->output     = render::fboBuild(res);
}

static Dude _createDude(Game* game) {
   Dude out;

   auto tex = gameTexture(GameTextures_Dude);

   out.c = White;
   out.moveset = _createMoveSet();
   out.phy.pos = { 10,10 };
   out.phy.circle.size = 10.0f;
   out.phy.maxSpeed = cDudeMoveSpeed;
   out.phy.invMass = 0.0f;
   out.renderSize = {(f32)tex.sz.x, (f32)tex.sz.y};
   out.texture = tex.handle;

   out.status.stamina = out.status.staminaMax = 4;
   out.status.health = out.status.healthMax = 1;

   return out;
}

static Dude _createEnemy(Float2 pos) {
   Dude out;

   auto tex = gameTexture(GameTextures_Dude);

   out.moveset = _createMoveSet();
   out.c = White;// {1.0f, 0.3f, 0.3f, 1.0f};
   out.phy.pos = pos;
   out.phy.circle.size = 10.0f;
   out.phy.velocity = { 0,0 };
   out.phy.maxSpeed = cDudeMoveSpeed;
   out.phy.invMass =(f32)(rand()%50 + 5) / 100.0f;
   out.renderSize = { (f32)tex.sz.x, (f32)tex.sz.y };
   out.texture = tex.handle;
   out.status.stamina = out.status.staminaMax = 3;
   out.status.health = out.status.healthMax = 1;

   return out;
}

void DEBUG_gameSpawnDude(Game* game) {
   auto e = _createEnemy({ (f32)(rand() % 1820) + 100, (f32)(rand() % 980) + 100 });
   e.ai.target = &game->maindude;
   game->baddudes.push_back(e);
}


static void _gameInitNew() {
   auto lastUpdate = g_game->lastUpdate;
   *g_game = Game();
   _gameDataInit(&g_game->data, nullptr);
   _createGraphicsObjects(g_game);
   g_game->lastUpdate = lastUpdate;
   g_game->maindude = _createDude(g_game);

   auto e = _createEnemy({ 1000, 1000 });
   e.ai.target = &g_game->maindude;
   g_game->baddudes.push_back(e);

   //for (int i = 0; i < DUDE_COUNT; ++i) {
   //   auto e = _createEnemy({ (f32)(rand() % 1820) + 100, (f32)(rand() % 980) + 100 }, 10.0f);
   //   e.ai.target = &game->maindude;
   //   game->baddudes.push_back(e);
   //}
}

void gameBegin(Game*game) {
   _buildGameTextures();
   game->lastUpdate = appGetTime();
   _gameInitNew();
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
   uber::bindTexture(Uniform_DiffuseTexture, gameTextureHandle(GameTextures_ShittySword));

   render::meshRender(g_game->meshUncentered);
}

static void _renderDude(Dude& dude) {
   auto model = Matrix::identity();

   Float2 rotate = dude.mv.facing;
   if (dude.state == DudeState_ATTACKING) {
      rotate += dude.atk.weaponVector;
      rotate /= 2.0f;
   }

   rotate = v2Normalized(rotate);

   model *= Matrix::translate2f(dude.phy.pos);
   model *= Matrix::rotate2D(v2Angle(rotate));
   model *= Matrix::scale2f(dude.renderSize);


   uber::resetToDefault();
   uber::set(Uniform_Height, cDudeHeight);

   if (dudeAlive(dude)) {
      uber::set(Uniform_Color, dude.c);
   }
   else {
      uber::set(Uniform_Color, DkRed);
   }

   // rotate the normals map with our texture
   uber::set(Uniform_TransformNormals, true);
   uber::set(Uniform_NormalTransform, Matrix::rotate2D(v2Angle({ rotate.x, -rotate.y })) );

   uber::set(Uniform_ModelMatrix, model);
   uber::bindTexture(Uniform_DiffuseTexture, dude.texture);
   uber::bindTexture(Uniform_NormalsTexture, gameTextureHandle(GameTextures_DudeNormals));
   render::meshRender(g_game->mesh);

   //uber::set(Uniform_OutlineOnly, true);
   //render::meshRender(g_game->mesh);

   if (gameDataGet()->imgui.showCollisionDebugging) {

      uber::resetToDefault();
      model = Matrix::translate2f(dude.phy.pos);
      model *= Matrix::scale2f({ dude.phy.circle.size * 2, dude.phy.circle.size * 2 });
      uber::set(Uniform_Color, Cyan);
      uber::set(Uniform_Alpha, 0.25f);
      uber::set(Uniform_ModelMatrix, model);

      uber::bindTexture(Uniform_DiffuseTexture, gameTextureHandle(GameTextures_Circle));
      render::meshRender(g_game->mesh);
   }
   
   if (dude.state == DudeState_ATTACKING) {
      _renderSwing(dude);
   }
}



static void _renderTarget(Float2 pos, ColorRGBAf color) {
   auto model = Matrix::identity();

   auto sz = gameTexture(GameTextures_Target).sz;

   model *= Matrix::translate2f(pos);
   model *= Matrix::scale2f({ (f32)sz.x, (f32)sz.y });

   uber::resetToDefault();
   uber::set(Uniform_Color, color);
   uber::set(Uniform_ModelMatrix, model);
   uber::bindTexture(Uniform_DiffuseTexture, gameTextureHandle(GameTextures_Target));
   render::meshRender(g_game->mesh);
}

static void _renderFloor(Game* game) {
   auto& c = ConstantsGet();

   Float2 fres = { game->map.size.x, game->map.size.y };
   Float2 tileSize = { 16, 16 };
   
   uber::resetToDefault();
   uber::set(Uniform_Height, cFloorHeight);
   uber::set(Uniform_TextureMatrix, Matrix::scale2f({ fres.x / tileSize.x, fres.y / tileSize.y }));
   uber::set(Uniform_ModelMatrix, Matrix::scale2f(fres));
   uber::bindTexture(Uniform_DiffuseTexture, gameTextureHandle(GameTextures_Tile));
   uber::bindTexture(Uniform_NormalsTexture, gameTextureHandle(GameTextures_TileNormals));
   //uber::set(Uniform_ColorOnly, true);

   render::meshRender(g_game->meshUncentered);

   //if (gameDataGet()->imgui.showCollisionDebugging) {
   //   Rectf testrect = { 300,300, 500, 200 };

   //   uber::set(Uniform_ColorOnly, true);
   //   uber::set(Uniform_Color, circleVsAabb(game->maindude.phy.pos, game->maindude.phy.circle.size, testrect) ? Red : Cyan);
   //   uber::set(Uniform_ModelMatrix, Matrix::translate2f({ testrect.x, testrect.y }) * Matrix::scale2f({ testrect.w, testrect.h }));
   //   render::meshRender(g_game->meshUncentered);
   //}
}


/*
Render Pipe

FBO: UnlitScene[2] = {Diffuse, Normals}
   BlendMode(DISABLED)
   DiscardAlpha
   OutputNormals
   Bind uDiffuse
   Bind uNormals
   Set uHeight
   Render Floor, Dudes, etc

FBO: Lights[1] = {Diffuse}
   BlendMode(ADDITIVE)
   Bind UnlitScene.Normals as uNormals
   PointLight
   NormalLighting
   Set uHeight
   Clear(AmbientLight)
   Render Each Light

FBO: LitScene[1] = {Diffuse}
   BlendMode(DISABLED)
   Bind UnlitScene.Diffuse as uDiffuse
   Render UnlitScene

   BlendMode(MULTIPLY)
   Bind Lights.Diffuse as uDiffuse
   Render Lights

   (can now re-use Lights to draw a vignette mask and draw it over with multiply)

FBO: Output[1] = {Diffuse}
   BlendMode(ALPHA_BLENDING)
   Clear(Black)
   Render LitScene
   Render UI

*/


void renderUnlitScene(Game* game) {
   auto vp = game->cam.viewport;
   auto view = Matrix::ortho(vp.x, vp.x + vp.w, vp.y, vp.y + vp.h, 1, -1);   
   
   render::fboBind(game->unlitScene);
   uber::set(Uniform_ViewMatrix, view, true);
   uber::set(Uniform_DiscardAlpha, true, true);
   uber::set(Uniform_OutputNormals, true, true);
   uber::resetToDefault();

   render::setBlendMode(BlendMode_DISABLED);   
   render::clear(Black);

   
   _renderFloor(game);

   
   for (auto&& d : game->baddudes) {
      _renderDude(d);
   }
   _renderDude(game->maindude);

   // restore these flags
   uber::set(Uniform_DiscardAlpha, false, true);
   uber::set(Uniform_OutputNormals, false, true);
}

static void _addLight(f32 size, Float2 pos, ColorRGBAf c) {
   uber::set(Uniform_Color, c);
   uber::set(Uniform_Alpha, 1.0f);
   uber::set(Uniform_PointLightSize, size);
   uber::set(Uniform_LightFalloff, Float3{ 
      cLightConstant, 
      cLightLinear,
      cLightQuadratic  });

   //uber::set(Uniform_LightIntensity, 1.0f);
   uber::set(Uniform_ModelMatrix, Matrix::translate2f(pos) * Matrix::scale2f({size, size}));
   render::meshRender(g_game->mesh);
}

void renderLightLayer(Game* game) {
   auto vp = game->cam.viewport;
   auto al = gameDataGet()->imgui.ambientLight;

   render::fboBind(game->lightLayer);
   uber::set(Uniform_ViewMatrix, Matrix::ortho(0, vp.w, 0, vp.h, 1, -1), true);   
   uber::resetToDefault();

   render::setBlendMode(BlendMode_ADDITION);   
   render::clear({ al, al, al, al });

   // bind deferred normals map
   uber::bindTexture(Uniform_NormalsTexture, game->unlitScene.out[1].handle);
   uber::set(Uniform_PointLight, true);
   uber::set(Uniform_NormalLighting, true);

   uber::set(Uniform_Height, cLightHeight);

   
   //uber::set(Uniform_LightIntensity, cLightIntensity);

   //_addLight({ vp.w,vp.w }, { vp.w / 2.0f, vp.h / 2.0f }, Yellow);
   //_addLight({ 120, 120 }, game->maindude.phy.pos - Float2{ vp.x, vp.y }, Yellow);

   _addLight(150, Float2{200, 200} - Float2{ vp.x, vp.y }, White);

   int i = 0;
   for (auto&& d : game->baddudes) {
      static ColorRGBAf c[] = { Red, Green, Blue };
      _addLight(80, d.phy.pos - Float2{ vp.x, vp.y }, c[i++ % 3]);
   }

   _addLight(300, game->data.io.mousePos - Float2{vp.x, vp.y}, DkGreen);
}

void renderLitScene(Game* game) {
   auto vp = game->cam.viewport;

   render::fboBind(game->litScene);   
   uber::resetToDefault();
   render::clear(Black);

   render::setBlendMode(BlendMode_DISABLED);
   uber::set(Uniform_ModelMatrix, Matrix::scale2f({ (f32)vp.w, (f32)vp.h }));
   uber::bindTexture(Uniform_DiffuseTexture, game->unlitScene.out[0].handle);
   render::meshRender(game->meshUncentered);

   render::setBlendMode(BlendMode_MULITPLY);
   uber::bindTexture(Uniform_DiffuseTexture, game->lightLayer.out[0].handle);
   render::meshRender(game->meshUncentered);

   
}

void renderUI(Game* game) {
   auto& vp = game->cam.viewport;

   render::fboBind(game->UI);   

   uber::resetToDefault();
   render::clear(Cleared);

   render::setBlendMode(BlendMode_NORMAL);   

   /*if (game->maindude.stamina < game->maindude.staminaMax)*/ {
      auto tsz = gameTexture(GameTextures_GemFilled).sz;
      Float2 gemSize = { (f32)tsz.x, (f32)tsz.y };
      f32 gemSpace = 0.0f;
      auto w = (gemSize.x + gemSpace) * game->maindude.status.staminaMax;

      //Float2 staminaCorner = { game->maindude.phy.pos.x - w / 2.0f, game->maindude.phy.pos.y + game->maindude.phy.circle.size + 20 };
      Float2 staminaCorner = { 10,10 };

      if (game->maindude.status.stamina == 0) {
         uber::set(Uniform_Alpha, 1.0f);
         uber::set(Uniform_Color, Red);
      }
      for (int i = 0; i < game->maindude.status.staminaMax; ++i) {
         
         auto model = Matrix::translate2f(staminaCorner) *  Matrix::scale2f(gemSize);
         uber::set(Uniform_ModelMatrix, model);
         uber::bindTexture(Uniform_DiffuseTexture, gameTextureHandle(i < game->maindude.status.stamina ? GameTextures_GemFilled : GameTextures_GemEmpty));
         render::meshRender(g_game->meshUncentered);

         staminaCorner.x += gemSize.x + gemSpace;
      }

      uber::resetToDefault();   
      if (game->maindude.status.stamina != 0) {
         uber::set(Uniform_Alpha, 0.5f);
         //uber::set(Uniform_Color, Red);
      }

      for (int i = 0; i < game->maindude.status.healthMax; ++i) {

         auto model = Matrix::translate2f(staminaCorner) *  Matrix::scale2f(gemSize);
         uber::set(Uniform_ModelMatrix, model);
         uber::bindTexture(Uniform_DiffuseTexture, gameTextureHandle(i < game->maindude.status.health ? GameTextures_HeartFilled : GameTextures_HeartEmpty));
         render::meshRender(g_game->meshUncentered);

         staminaCorner.x += gemSize.x + gemSpace;
      }
   }


   if (game->data.imgui.showMovementDebugging) {
      const static f32 moveTargetDist = 50.0f;
      const static f32 aimTargetDist = 100.0f;
      auto &io = game->data.io;
      auto &dude = game->maindude;

      auto pos = dude.phy.pos - Float2{(f32)vp.x, (f32)vp.y};

      _renderTarget(pos + dude.phy.velocity * 100, Cyan);
      _renderTarget(pos + v2Normalized(dude.mv.facing) * aimTargetDist, Red);

      _renderTarget(pos + io.leftStick * moveTargetDist, DkGreen);
      _renderTarget(pos + dude.mv.moveVector * 10, Magenta);

      //_renderTarget(pos + io.rightStick * aimTargetDist, Yellow);
      _renderTarget(pos + dude.mv.faceVector * aimTargetDist, LtGray);
   }
}

static void renderOutput(Game* game) {
   auto& vp = game->cam.viewport;

   render::fboBind(game->output);

   uber::resetToDefault();
   render::clear(Black);

   render::setBlendMode(BlendMode_NORMAL);
   uber::set(Uniform_ModelMatrix, Matrix::scale2f({ (f32)vp.w, (f32)vp.h }));

   uber::bindTexture(Uniform_DiffuseTexture, game->litScene.out[0].handle);
   render::meshRender(game->meshUncentered);

   uber::bindTexture(Uniform_DiffuseTexture, game->UI.out[0].handle);
   render::meshRender(game->meshUncentered);

   if (game->state == GameState_YOUDIED) {
      render::setBlendMode(BlendMode_MULITPLY);
      auto ratio = cosInterp(1.0f - ((f32)game->youdied.clock / 3000));
      uber::set(Uniform_Color, ColorRGBAf{ 1.0f, ratio, ratio, 1.0f });
      uber::set(Uniform_ColorOnly, true);
      render::meshRender(game->meshUncentered);
   }
}

static void _renderScene(Game* game) {
   auto& c = ConstantsGet();

   render::shaderSetActive(game->shader);
   uber::resetToDefault(true);
   
   renderUnlitScene(game);
   renderLightLayer(game);
   renderLitScene(game);
   renderUI(game);

   renderOutput(game);

   render::fboBind({});
}



void gameRender(Game*game) {
   
   _renderScene(game);
   if (game->reloadShader) {
      if (!_reloadShader(game)) {
         ImGui::OpenPopup("shadererr");
      }
   }

   if (ImGui::BeginPopupModal("shadererr")) {
      ImGui::Text("Shader reload failed");
      ImGui::EndPopup();
   }
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
      if (game->data.imgui.mouseHovering) {
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

struct FrameData {
   DynamicArray<PhyObject*> phyObjs;
};

void buildFrameData(Game*game, FrameData& fd) {
   fd.phyObjs.clear();

   fd.phyObjs.push_back(&game->maindude.phy);
   for (auto && d : game->baddudes) {
      if (dudeAlive(d)) {
         fd.phyObjs.push_back(&d.phy);
      }
   }


   if (fd.phyObjs.size() == 1) {
      ++game->waveSize;
      for (int i = 0; i < game->waveSize; ++i) {
         DEBUG_gameSpawnDude(game);
      }
   }
}


void gameUpdateMilliStep(Game* game, FrameData& fd) {
   //static Milliseconds ms = 0;
   //if (ms++ % 2 != 0) {
   //   return;
   //}

   dudeUpdateState(game->maindude);

   if (dudeAlive(game->maindude)) {
      dudeApplyInput(game->maindude);
   }

   dudeUpdateRotation(game->maindude);
   dudeUpdateVelocity(game->maindude);

   mainDudeCheckAttackCollisions(game->maindude, game->baddudes);

   for (auto && d : game->baddudes) {
      dudeUpdateState(d);

      if (dudeAlive(d)) {
         dudeUpdateBehavior(d);
      }

      dudeUpdateRotation(d);
      dudeUpdateVelocity(d);

      if (dudeAlive(d)) {
         badDudeCheckAttackCollision(d, game->maindude);
      }
   }

   updatePhyPositions(fd.phyObjs);

   if (game->state == GameState_YOUDIED) {
      if (++game->youdied.clock > 3000) {
         _gameInitNew();
      }
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
   
   static FrameData fd;
   buildFrameData(game, fd);

   for (Milliseconds i = 0; i < ms; ++i) {
      gameUpdateMilliStep(game, fd);
   }

   vp.x = clamp(dudePos.x - (vp.w / 2), 0, game->map.size.x - vp.w);
   vp.y = clamp(dudePos.y - (vp.h / 2), 0, game->map.size.y - vp.h);

   game->lastUpdate += timeMillis(ms);

   // imgui output
   uiEditDude(game->maindude);
   gameDoUI(game);
}

