#include "stdafx.h"

#include "game.h"
#include "app.h"
#include "win.h"
#include "render.h"

#include "phy.h"

Map _defaultMap = { {1000,1000} };

#include "assets_gen.inl"

#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <stb/stb_image.h>

#define AXIS_DEADZONE 0.25f

// our aim stick normalizes so we need a harsher deadzone to prevent bounce-back
#define AXIS_AIM_DEADZONE 0.5f

GameAssets Assets;
EngineConstants& Const = Assets.constants;

EngineState Engine;
GraphicObjects Graphics;


void dudeBeginCooldown(Dude&d, Milliseconds duration);
void dudeSetState(Dude& d, DudeState state, Milliseconds startTimeOffset = 0);

bool dudeAlive(Dude&d) {
   return d.state != DudeState_DEAD;
}



bool dudeSpendStamina(Dude &d, int stam, PipState spendType) {
   auto stamAvail = 0;
   for (auto&&s : d.status.stamina) { if (s.state == PipState_Full) ++stamAvail; else break; }

   bool hasEnough = stamAvail >= stam;

   if (hasEnough) { // dont interrupt recharge if you dont have enough to spend
      for (int i = stamAvail; i < d.status.stamina.size(); ++i) {
         d.status.stamina[i].charge = 0;
      }
   }

   for (int i = stamAvail - 1; i >= 0 && stam > 0; --i, --stam) {
      d.status.stamina[i].state = spendType;
      d.status.stamina[i].charge = 0;

      Milliseconds fullCharge = 0;
      switch (spendType) {
      case PipState_Cracked:fullCharge = Const.dudeCrackedStaminaRecoveryTime; break;
      case PipState_Spent:fullCharge = Const.dudeSpentStaminaRecoveryTime; break;
      }
      d.status.stamina[i].fullCharge = fullCharge;
   }

   return hasEnough;
}

void dudeRecoverStamina(Dude&d, Milliseconds ticks) {
   for (auto&&s : d.status.stamina) {
      if (s.state == PipState_Full) {
         continue;
      }

      s.charge += ticks;
      if (s.charge > s.fullCharge) {
         s.state = PipState_Full;
         ticks = s.charge - s.fullCharge;
      }
      else {
         break;
      }
   }
}

bool dudeStaminaEmpty(Dude&d) {
   return d.status.stamina[0].state != PipState_Full;
}

void dudeDoDamage(Dude&d, AttackSwing&atk, bool overExtended) {
   auto& c = Const;
   if (!dudeStaminaEmpty(d)) {
      dudeSpendStamina(d, atk.staminaDamage, PipState_Cracked);

      if (dudeStaminaEmpty(d)) {
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
      d.status.health -= atk.healthDamage;
      if (d.status.health <= 0) {
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
void dudeUpdateShove(Dude& d, Milliseconds tickSize) {
   if (d.shoved) {
      d.shove.start += tickSize;
      if (d.shove.start >= d.shove.dur) {
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
   //d.free.staminaClockStart = 0;
   //d.free.nextTickAt = calcNextStaminaTickTime(d.status.stamina, d.status.staminaMax);
}

void dudeUpdateStateFree(Dude& d, Milliseconds tickSize) {
   // handle stamina regain
   dudeRecoverStamina(d, tickSize);
   //d.free.staminaClockStart += tickSize;
   //if (d.free.staminaClockStart > d.free.nextTickAt && d.status.stamina < d.status.staminaMax) {
   //   //d.status.stamina = d.status.staminaMax;
   //   ++d.status.stamina;
   //   d.free.staminaClockStart = 0;
   //   d.free.nextTickAt = calcNextStaminaTickTime(d.status.stamina, d.status.staminaMax);
   //}
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
   // gonna try making dashing free for a while
   //if (dudeSpendStamina(d, 1)) {
      
      dudeSetState(d, DudeState_DASH);

      Float2 dashDir;
      if (v2LenSquared(d.mv.moveVector) > 0.0f) {
         dashDir = v2Normalized(d.mv.moveVector);
      }
      else {
         dashDir = d.mv.faceVector;
      }

      dudeShove(d, dashDir, Const.dudeDashSpeed, Const.dudeDashDistance );
   //}
}

void dudeBeginAttack(Dude& d, int swingDir, int combo);

// dude will begin dash state by being shoved, this will start a cooldown
// once the shove is complete
void dudeUpdateStateDash(Dude& d) {
   if (!d.shoved) {
      // cancel the cooldown by attacking before the end of the dash
      if (d.dash.postDashAttack) {
         dudeBeginAttack(d, 1, 0);
         d.dash.postDashAttack = false;
      }
      else {
         Milliseconds cd = Const.dudePostDashCooldown;
         dudeBeginCooldown(d, cd);
      }
      
   }
}

void dudeBeginAttack(Dude& d, int swingDir, int combo) {


   bool hadStamina = dudeSpendStamina(d, 1, PipState_Spent);
   bool overExtend = !hadStamina && (combo > 0 && combo < d.moveset.swings.size());
   if (hadStamina || overExtend) {
      dudeSetState(d, DudeState_ATTACKING);
      d.atk.hits.clear();

      if (!combo) {
         d.atk.overExtended = false;
      }

      if (combo > 0 && overExtend) {
         d.atk.overExtended = true;
      }

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

void dudeUpdateStateAttack(Dude& d, Milliseconds tickSize) {

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
      d.atk.weaponVector = v2Rotate(d.atk.weaponVector, v2FromAngle(-d.atk.swingDir * radsPerMs * tickSize));

      if (d.stateClock >= d.atk.swing.swingDur) {
         d.stateClock -= d.atk.swing.swingDur;
         d.atk.weaponVector = v2Normalized(v2Rotate(d.mv.facing, v2FromAngle(-d.atk.swingDir * d.atk.swing.swipeAngle / 2.0f * DEG2RAD)));

         bool executeQueued = false;
         if (d.atk.queuedAttacked) {
            d.atk.queuedAttacked = false;
            if (d.atk.combo + 1 < d.moveset.swings.size()) {
               dudeBeginAttack(d, -d.atk.swingDir, d.atk.combo + 1);
               executeQueued = true;
            }
         }


         if(!executeQueued) {
            d.atk.swingPhase = SwingPhase_Cooldown;
         }
      }
   }  break;
   case SwingPhase_Cooldown:
      if (d.stateClock >= d.atk.swing.cooldownDur) {

         // at the end of an attack cooldown, check to see if we over extended
         if (d.atk.overExtended && dudeStaminaEmpty(d)) {
            d.status.stamina[0].fullCharge += Const.overExtendedStaminaRecoveryTime;
         }
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

void dudeUpdateState(Dude& d, Milliseconds tickSize) {
   d.stateClock += tickSize;

   dudeUpdateShove(d, tickSize);

   switch (d.state) {
   case DudeState_FREE: dudeUpdateStateFree(d, tickSize); break;
   case DudeState_COOLDOWN: dudeUpdateStateCooldown(d); break;
   case DudeState_DASH: dudeUpdateStateDash(d); break;
   case DudeState_ATTACKING: dudeUpdateStateAttack(d, tickSize); break;
   }
   
}

void mainDudeCheckAttackCollisions(Dude& dude, Array<Dude> &targets) {
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
         dudeDoDamage(t, dude.atk.swing, dude.atk.overExtended);
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
      dudeDoDamage(t, dude.atk.swing, dude.atk.overExtended);
      auto pushdir = v2Normalized(t.phy.pos - dude.phy.pos);
      dudeShove(t, pushdir, Const.dudeDashSpeed, Const.dudeKnockbackDistance);
      dude.atk.hits.push_back(&t);
   }
}


static bool rightStickActive(GameState& g) { return v2LenSquared(g.io.rightStick) != 0.0f; }
static bool leftStickActive(GameState& g) { return v2LenSquared(g.io.leftStick) != 0.0f; }

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

   if (v2Len(aimStick) > AXIS_AIM_DEADZONE) {
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

void dudeApplyInputDashingAttack(GameState& g, Dude& d) {
   auto& io = g.io;
   if (io.buttonPressed[GameButton_RT]) {
      d.dash.postDashAttack = true;
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
      case SwingPhase_Windup:
      case SwingPhase_Lunge:
      case SwingPhase_Swing:
         d.atk.queuedAttacked = true;
         break;
      case SwingPhase_Cooldown:
         if (d.atk.combo + 1 < d.moveset.swings.size()) {
            dudeBeginAttack(d, -d.atk.swingDir, d.atk.combo + 1);
         }
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
      dudeApplyInputDashingAttack(g, d);
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
      auto scaledSpeed = 
         (d.phy.maxSpeed * (1 - (Const.dudeBackwardsPenalty * 0.5f))) + 
         (d.phy.maxSpeed * (Const.dudeBackwardsPenalty * 0.5f) * facedot);

      // set the target speed
      d.mv.moveSpeedCapTarget = scaledSpeed * v2Len(d.mv.moveVector);

      // ease speed cap toward target

      if (d.mv.moveSpeedCap < d.mv.moveSpeedCapTarget) {
         d.mv.moveSpeedCap += Const.dudeAccelerationRate;
         if (d.mv.moveSpeedCap > d.mv.moveSpeedCapTarget) {
            d.mv.moveSpeedCap = d.mv.moveSpeedCapTarget;
         }
      }
      else {
         d.mv.moveSpeedCap -= Const.dudeDeccelerationRate;
         if (d.mv.moveSpeedCap < d.mv.moveSpeedCapTarget) {
            d.mv.moveSpeedCap = d.mv.moveSpeedCapTarget;
         }
      }      
   }

   // add the movevector scaled against acceleration to velocity and cap it
   d.phy.velocity = v2CapLength(d.phy.velocity + d.mv.moveVector , d.mv.moveSpeedCap);
   //if (fabs(d.phy.velocity.x) < EPSILON) { d.phy.velocity.x = 0.0f; }
   //if (fabs(d.phy.velocity.y) < EPSILON) { d.phy.velocity.y = 0.0f; }
}

// rotate facing vector toward the facevector
void dudeUpdateRotation(Dude& d) {
   if (v2LenSquared(d.mv.faceVector) > 0.0f) {
      d.mv.facing = v2Normalized(v2RotateTowards(d.mv.facing, d.mv.faceVector, v2FromAngle(Const.dudeRotationSpeed)));
   }
}

void dudeUpdateBehavior(Dude& dude, Milliseconds tickSize) {

   if (dude.state != DudeState_FREE) {
      return;
   }
   dude.ai.started += tickSize;
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

   
   if (dude.ai.started > (rand() % 3000) + 3000) {
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

void _buildPhySystem(GameState&game) {
   game.phySys.objs.clear();

   game.phySys.objs.push_back(&game.maindude.phy);
   for (auto && d : game.baddudes) {
      if (dudeAlive(d)) {
         game.phySys.objs.push_back(&d.phy);
      }
   }

   for (auto && w : game.map->walls) {
      auto& pts = w.poly.points;
      auto pCount = pts.size();
      if (pCount >= 3) {
         if (w.phyObjs.size() < pCount) {
            w.phyObjs.clear();
            for (int i = 0; i < pCount; ++i) {
               auto next = (i + 1) % pCount;

               PhyObject segment;
               segment.type = PhyObject::PhyType_Segment;
               segment.pos = pts[i];
               segment.segment.b = pts[next];
               segment.maxSpeed = 0.0f;
               segment.invMass = 0.0f;
               w.phyObjs.push_back(segment);
            }
         }

         for (auto&& po : w.phyObjs) {
            game.phySys.objs.push_back(&po);
         }
      }
   }


   if (game.phySys.objs.size() == 1) {
      ++game.waveSize;
      for (int i = 0; i < game.waveSize; ++i) {
         //DEBUG_gameSpawnDude(game);
      }
   }
}


void gameUpdateMilliStep(GameState& game) {

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
      //bool pressed = event->type == SDL_KEYDOWN;
      //GameButton btn;

      //switch (event->key.keysym.scancode) {
      //case SDL_SCANCODE_W:
      //   btn = GameButton_UP;

      //   if (pressed) {
      //      if (!event->key.repeat) io.leftStick.y = -1.0f;
      //   }
      //   else if (!io.buttonDown[GameButton_DOWN]) {
      //      io.leftStick.y = 0.0f;
      //   }

      //   break;
      //case SDL_SCANCODE_A:
      //   btn = GameButton_LEFT;

      //   if (pressed) {
      //      if (!event->key.repeat) io.leftStick.x = -1.0f;
      //   }
      //   else if (!io.buttonDown[GameButton_RIGHT]) {
      //      io.leftStick.x = 0.0f;
      //   }
      //   break;
      //case SDL_SCANCODE_S:
      //   btn = GameButton_DOWN;

      //   if (pressed) {
      //      if (!event->key.repeat) io.leftStick.y = 1.0f;
      //   }
      //   else if (!io.buttonDown[GameButton_UP]) {
      //      io.leftStick.y = 0.0f;
      //   }
      //   break;
      //case SDL_SCANCODE_D:
      //   btn = GameButton_RIGHT;

      //   if (pressed) {
      //      if (!event->key.repeat) io.leftStick.x = 1.0f;
      //   }
      //   else if (!io.buttonDown[GameButton_LEFT]) {
      //      io.leftStick.x = 0.0f;
      //   }
      //   break;
      //default: return false;
      //}

      //if (!event->key.repeat) {
      //   io.leftStick = v2Normalized(io.leftStick);
      //}

      //if (!io.buttonDown[btn] && pressed) {
      //   io.buttonPressed[btn] = true;
      //}
      //if (io.buttonDown[btn] && !pressed) {
      //   io.buttonReleased[btn] = true;
      //}
      //io.buttonDown[btn] = pressed;

      //return true;
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
         //value = 0.0f;
         pressed = false;
      }

      switch (event->caxis.axis) {
      case SDL_CONTROLLER_AXIS_LEFTX: io.leftStick_RAW.x = value; btn = value < 0.0f ? GameButton_LEFT : GameButton_RIGHT; break;
      case SDL_CONTROLLER_AXIS_LEFTY: io.leftStick_RAW.y = value; btn = value < 0.0f ? GameButton_UP : GameButton_DOWN; break;
      case SDL_CONTROLLER_AXIS_RIGHTX: io.rightStick_RAW.x = value; btn = value < 0.0f ? GameButton_LEFT : GameButton_RIGHT; break;
      case SDL_CONTROLLER_AXIS_RIGHTY: io.rightStick_RAW.y = value; btn = value < 0.0f ? GameButton_UP : GameButton_DOWN; break;
      case SDL_CONTROLLER_AXIS_TRIGGERLEFT: io.leftTrigger = value; btn = GameButton_LT; break;
      case SDL_CONTROLLER_AXIS_TRIGGERRIGHT: io.rightTrigger = value; btn = GameButton_RT; break;
      }

      if (!io.buttonDown[btn] && pressed) {
         io.buttonPressed[btn] = true;
      }

      if (io.buttonDown[btn] && !pressed) {
         io.buttonReleased[btn] = true;
      }

      io.buttonDown[btn] = pressed;

   } return true;
   }

   return false;
}

static Float2 _scaleStick(Float2&stick) {
   auto len = v2Len(stick);
   if (len < AXIS_DEADZONE) {
      return { 0, 0 };
   }

   return stick;
   //else {
   //   stick = v2Normalized(stick) * ((len - AXIS_AIM_DEADZONE) / (1 / AXIS_AIM_DEADZONE));
   //}
}

static void _applyInputs(GameState&g) {
   g.io.leftStick = _scaleStick(g.io.leftStick_RAW);
   g.io.rightStick = _scaleStick(g.io.rightStick_RAW);

   if (dudeAlive(g.maindude)) {
      dudeApplyInput(g, g.maindude);
   }
}


static void _cameraFollowPlayer(GameState& g) {   
   if (!g.ui.editing) {
      auto &vp = g.camera.viewport;
      auto& dudePos = g.maindude.phy.pos;

      //camera follow on dude
      vp.x = clamp(dudePos.x - (vp.w / 2), 0, g.map->size.x - vp.w);
      vp.y = clamp(dudePos.y - (vp.h / 2), 0, g.map->size.y - vp.h);
      vp.w = (float)Const.vpSize.x;
      vp.h = (float)Const.vpSize.y;
   }
}


static void _otherFrameStep(GameState& g) {
   Milliseconds tickSize = 32;
   //LOG("32 milis step");
}

static void _updateMode(GameState&g, Milliseconds tickSize) {
   g.mode.clock += tickSize;
   switch (g.mode.type) {
   case ModeType_ACTION: gameUpdateActionMode(g); break;
   case ModeType_YOUDIED: gameUpdateYouDiedMode(g); break;
   }
}

static void _frameStep(GameState& g) {
   Milliseconds tickSize = 16;
   //LOG("16 milis step");

   dudeUpdateState(g.maindude, tickSize);
   mainDudeCheckAttackCollisions(g.maindude, g.baddudes);

   dudeUpdateRotation(g.maindude);
   dudeUpdateVelocity(g.maindude);

   for (auto && d : g.baddudes) {
      if (dudeAlive(d)) {
         dudeUpdateBehavior(d, tickSize);
         badDudeCheckAttackCollision(g, d, g.maindude);
      }

      dudeUpdateState(d, tickSize);

      dudeUpdateRotation(d);
      dudeUpdateVelocity(d);
   }

   _buildPhySystem(g);   
}

static void _milliStep(GameState& g) {
   Milliseconds tickSize = 1;
   //LOG("1 mili step");

   updatePhyPositions(g.phySys.objs); 
}

static void _perRenderStep(GameState& g, Milliseconds ms) {
   Milliseconds tickSize = ms;
   _cameraFollowPlayer(g);
   //LOG("PerFrame step");
}


static const int LastUpdateMSCap = 32;
static const int FrameLength = 16;

void gameUpdate(GameState& g) {
   auto time = appGetTime();
   auto dt = time - g.lastUpdate;
   auto ms = dt.toMilliseconds();

   _applyInputs(g);

   if (ms > LastUpdateMSCap) {
      // something bad happened and we spiked hard
      ms = LastUpdateMSCap;
      g.lastUpdate = time - timeMillis(ms);
   }

   g.lastUpdate += timeMillis(ms);   

   g.otherFrameClock += ms;
   g.frameClock += ms;
   g.gameClock += ms;

   while (g.otherFrameClock > FrameLength * 2) {
      _otherFrameStep(g);
      g.otherFrameClock -= FrameLength * 2;

      for (int i = 0; i < 2; ++i) {
         if (g.frameClock > FrameLength) {
            _frameStep(g);
            g.frameClock -= FrameLength;
         }

         for (int j = 0; j < FrameLength; ++j) {
            if (g.gameClock > 0) {
               _milliStep(g);
               --g.gameClock;
            }
         }
      }
   }

   while (g.frameClock > FrameLength) {
      _frameStep(g);
      g.frameClock -= FrameLength;

      for (int j = 0; j < FrameLength; ++j) {
         if (g.gameClock > 0) {
            _milliStep(g);
            --g.gameClock;
         }
      }
   }

   while (g.gameClock > 0) {
      _milliStep(g);
      --g.gameClock;
   }

   _updateMode(g, ms);
   _perRenderStep(g, ms);
}


