#include "stdafx.h"

#include "game.h"
#include "app.h"
#include "win.h"
#include "render.h"

#include "phy.h"



#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <stb/stb_image.h>

#define AXIS_DEADZONE 0.25f
#define AXIS_AIM_DEADZONE 0.5f


EngineConstants Const;
EngineState Engine;
GraphicObjects Graphics;
//Texture* Textures = Graphics.textures;
_TextureMap& TextureMap = Graphics.tmap;


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
   auto& c = Const;
   if (d.status.stamina > 0) {
      --d.status.stamina;

      if (d.status.stamina == 0) {
         dudeBeginCooldown(d, c.cooldownOnDamagedStaminaEmpty);
      }
      else {
         // attacks only cancel your attack if youre not swinging
         if (d.state != DudeState_ATTACKING || d.atk.swingPhase != SwingPhase_Swing) {
            dudeBeginCooldown(d, c.cooldownOnDamagedStamina);
         }
      }
   }
   else {
      // damaged health
      if (--d.status.health == 0) {
         dudeSetState(d, DudeState_DEAD);
      }
      else {
         dudeBeginCooldown(d, c.cooldownOnDamagedHealth);
      }
   }

}

void dudeSetState(Dude& d, DudeState state, Milliseconds startTimeOffset) {
   d.state = state;
   d.stateClock = startTimeOffset;
}

Milliseconds calcNextStaminaTickTime(int stam, int max) {
   auto ratio = (float)stam / max;
   return Const.dudeBaseStaminaTickRecoveryTime + (Milliseconds)(Const.dudeBaseStaminaTickRecoveryTime * cosInterp(1 - ratio));
}

void dudeShove(Dude&d, Float2 dir, float speed, float distance) {
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
         if (d.shove.speed > Const.dudeMoveSpeed) {
            d.phy.maxSpeed = Const.dudeMoveSpeed;
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

void dudeBeginDash(Dude& d, float speed) {
   if (dudeSpendStamina(d, 1)) {
      
      dudeSetState(d, DudeState_DASH);

      Float2 dashDir;
      if (v2LenSquared(d.mv.moveVector) > 0.0f) {
         dashDir = v2Normalized(d.mv.moveVector);
      }
      else {
         dashDir = d.mv.faceVector;
      }

      dudeShove(d, dashDir, Const.dudeDashSpeed, Const.dudeDashDistance );
   }
}


void dudeUpdateStateDash(Dude& d) {
   if (!d.shoved) {
      Milliseconds cd = Const.dudePostDashCooldown;
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

void mainDudeCheckAttackCollisions(Dude& dude, std::vector<Dude> &targets) {
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
         dudeShove(t, pushdir, Const.dudeDashSpeed, Const.dudeKnockbackDistance);
         dude.atk.hits.push_back(&t);
      }
   }
}

void badDudeCheckAttackCollision(GameState& g, Dude& dude, Dude& t) {
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
      dudeShove(t, pushdir, Const.dudeDashSpeed, Const.dudeKnockbackDistance);
      dude.atk.hits.push_back(&t);

      if (!dudeAlive(t)) {
         g.mode.type = ModeType_YOUDIED;
         g.mode.clock = 0;
         t.mv.moveVector = { 0,0 };
      }
   }
}




static bool rightStickActive(GameState& g) {
   auto &io = g.io;
   return fabs(io.rightStick.x) > AXIS_AIM_DEADZONE || fabs(io.rightStick.y) > AXIS_AIM_DEADZONE;
}
static bool leftStickActive(GameState& g) {
   auto &io = g.io;
   return fabs(io.leftStick.x) > AXIS_DEADZONE || fabs(io.leftStick.y) > AXIS_DEADZONE;
}

void dudeApplyInputMovement(GameState& g, Dude& d) {
   auto& io = g.io;

   d.mv.moveVector = io.leftStick;
}

void dudeApplyInputAiming(GameState& g, Dude& d) {
   auto& io = g.io;


   bool rstick = rightStickActive(g);
   bool lstick = leftStickActive(g);

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

void dudeApplyInputFreeActions(GameState& g, Dude& d) {
   auto& io = g.io;
   if (io.buttonPressed[GameButton_LT]){
      dudeBeginDash(d, Const.dudeDashSpeed);
   }
   else if (io.buttonPressed[GameButton_RT]) {
      dudeBeginAttack(d, 1, 0);
   }
}

void dudeApplyInputAttack(GameState& g, Dude& d) {

   switch (d.atk.swingPhase) {
   case SwingPhase_Windup:
      // apply aiming during windup 
      dudeApplyInputAiming(g, d);
      break;
   }

   auto& io = g.io;
   if (io.buttonPressed[GameButton_RT]) {
      switch (d.atk.swingPhase) {
      case SwingPhase_Cooldown:
         dudeBeginAttack(d, -d.atk.swingDir, d.atk.combo + 1);
         break;
      }
   }
}

void dudeApplyInput(GameState& g, Dude& d) {
   switch (d.state) {
   case DudeState_FREE:
      dudeApplyInputMovement(g, d);
      dudeApplyInputAiming(g, d);
      dudeApplyInputFreeActions(g, d);
      break;
   case DudeState_DASH:
      break;
   case DudeState_ATTACKING:
      dudeApplyInputAttack(g, d);
      break;
   }
}

void dudeUpdateVelocity(Dude& d) {
   // we assume at this point the moveVector is current

   if (!d.shoved) {
      // scale mvspeed based on facing;
      float facedot = v2Dot(v2Normalized(d.phy.velocity), d.mv.facing);
      auto scaledSpeed = (d.phy.maxSpeed * (1 - Const.dudeBackwardsPenalty)) + (d.phy.maxSpeed * Const.dudeBackwardsPenalty * facedot);

      // set the target speed
      d.mv.moveSpeedCapTarget = scaledSpeed * v2Len(d.mv.moveVector);

      // ease speed cap toward target
      if (d.mv.moveSpeedCap < d.mv.moveSpeedCapTarget) {
         d.mv.moveSpeedCap += Const.dudeSpeedCapEasing;
      }
      else {
         d.mv.moveSpeedCap -= Const.dudeSpeedCapEasing;
      }
      clamp(d.mv.moveSpeedCap, 0, d.mv.moveSpeedCapTarget);
   }

   

   // add the movevector scaled against acceleration to velocity and cap it
   d.phy.velocity = v2CapLength(d.phy.velocity + d.mv.moveVector , d.mv.moveSpeedCap);
}

// rotate facing vector toward the facevector
void dudeUpdateRotation(Dude& d) {
   if (v2LenSquared(d.mv.faceVector) > 0.0f) {
      d.mv.facing = v2Normalized(v2RotateTowards(d.mv.facing, d.mv.faceVector, v2FromAngle(Const.dudeRotationSpeed)));
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
       dude.mv.moveVector = v2Normalized(vec + v2Orthogonal(vec) * (float)dude.ai.dir);
   }
   else {
      auto unitToTarget = v2Normalized(target.phy.pos - dude.phy.pos);
      dude.mv.faceVector = unitToTarget;

      if (dist < 200.0f) {
         // too close, back up
         dude.mv.moveVector = v2Normalized(-unitToTarget + v2Orthogonal(unitToTarget) * (float)dude.ai.dir);

         //if (dist < 100.0f) {
         //   dudeBeginAttack(dude, 1, 0);
         //}
      }
      else {
         
         dude.mv.moveVector = v2Orthogonal(unitToTarget) * 0.1f * (float)dude.ai.dir;
      }
   }
}







struct FrameData {
   std::vector<PhyObject*> phyObjs;
};

void buildFrameData(GameState&game, FrameData& fd) {
   fd.phyObjs.clear();

   fd.phyObjs.push_back(&game.maindude.phy);
   for (auto && d : game.baddudes) {
      if (dudeAlive(d)) {
         fd.phyObjs.push_back(&d.phy);
      }
   }


   if (fd.phyObjs.size() == 1) {
      ++game.waveSize;
      for (int i = 0; i < game.waveSize; ++i) {
         //DEBUG_gameSpawnDude(game);
      }
   }
}


void gameUpdateMilliStep(GameState& game, FrameData& fd) {
   //static Milliseconds ms = 0;
   //if (ms++ % 2 != 0) {
   //   return;
   //}

   dudeUpdateState(game.maindude);

   if (dudeAlive(game.maindude)) {
      dudeApplyInput(game, game.maindude);
   }

   dudeUpdateRotation(game.maindude);
   dudeUpdateVelocity(game.maindude);

   mainDudeCheckAttackCollisions(game.maindude, game.baddudes);

   for (auto && d : game.baddudes) {
      dudeUpdateState(d);

      if (dudeAlive(d)) {
         dudeUpdateBehavior(d);
      }

      dudeUpdateRotation(d);
      dudeUpdateVelocity(d);

      if (dudeAlive(d)) {
         badDudeCheckAttackCollision(game, d, game.maindude);
      }
   }

   updatePhyPositions(fd.phyObjs);

   if (game.mode.type == ModeType_YOUDIED) {
      if (++game.mode.clock > 3000) {
         //_gameInitNew();
      }
   }
}


void gameBeginFrame(GameState& g) {
   auto& mPos = ImGui::GetIO().MousePos;
   auto& io = g.io;
   io.mousePos = Coords::fromScreen({ mPos.x, mPos.y }, g);

   // update buttons
   for (GameButton b = 0; b < GameButton_COUNT; ++b) {
      io.buttonPressed[b] = io.buttonReleased[b] = false;
   }
}

bool gameProcessEvent(GameState& g, SDL_Event* event) {
   auto &io = g.io;

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
      g.lastMouseMove = appGetTime();
      break;

   case SDL_MOUSEBUTTONDOWN:
   case SDL_MOUSEBUTTONUP: {
      //if (g.mouseActive) {
      //   bool pressed = event->type == SDL_MOUSEBUTTONDOWN;
      //   if (!io.buttonDown[GameButton_RT] && pressed) {
      //      io.buttonPressed[GameButton_RT] = true;
      //   }

      //   if (io.buttonDown[GameButton_RT] && !pressed) {
      //      io.buttonReleased[GameButton_RT] = true;
      //   }

      //   io.buttonDown[GameButton_RT] = pressed;
      //   return true;
      //}
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

void gameUpdate(GameState& g) {
   auto time = appGetTime();
   auto dt = time - g.lastUpdate;
   auto ms = dt.toMilliseconds();

   auto&vp = g.camera.viewport;
   auto&dudePos = g.maindude.phy.pos;
   auto&dudeFace = g.maindude.mv.facing;

   if (ms > 32) {
      // something bad happened and we spiked hard
      ms = 32;
      g.lastUpdate = time - timeMillis(ms);
   }

   // build physics system

   static FrameData fd;
   buildFrameData(g, fd);

   for (Milliseconds i = 0; i < ms; ++i) {
      gameUpdateMilliStep(g, fd);
   }

   vp.x = clamp(dudePos.x - (vp.w / 2), 0, g.map.size.x - vp.w);
   vp.y = clamp(dudePos.y - (vp.h / 2), 0, g.map.size.y - vp.h);

   g.lastUpdate += timeMillis(ms);

   // imgui output

   //gameDoUI(g);
}


