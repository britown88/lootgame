#pragma once

#include "math.h"
#include "render.h"
#include "assets.h"
#include "phy.h"
#include "coords.h"
#include <vector>
#include <string>

typedef struct GameState GameState;

struct GraphicObjects {
   ShaderHandle shader = 0;

   Texture textures[GameTexture_COUNT];

   Mesh mesh, meshUncentered;
   FBO
      unlitScene,
      lightLayer,
      litScene,
      UI;

   void build();
   bool reloadShaders();
};


struct EngineConstants {
   Int2 resolution = { 1920, 1080 };

   f32 floorHeight = 0.0f;
   f32 dudeHeight = 0.1f;
   f32 lightHeight = 0.2f;
   f32 lightLinearPortion = 0.0f;
   f32 lightSmoothingFactor = 0.4f;
   f32 lightIntensity = 100.0f;

   f32 dudeAcceleration = 0.005f;
   f32 dudeRotationSpeed = 0.010f;
   f32 dudeMoveSpeed = 0.100f;
   f32 dudeDashSpeed = 0.300f;
   f32 dudeSpeedCapEasing = 0.0003f;
   f32 dudeBackwardsPenalty = 0.250f;
   f32 dudeDashDistance = 50.0f;
   f32 dudeKnockbackDistance = 0.0f;

   Milliseconds dudePostDashCooldown = 100;
   Milliseconds dudeBaseStaminaTickRecoveryTime = 500;

   Milliseconds cooldownOnDamagedStaminaEmpty = 1000;
   Milliseconds cooldownOnDamagedStamina = 250;
   Milliseconds cooldownOnDamagedHealth = 500;
};


struct EngineState {   
   ColorRGBAf bgClearColor = { 0.45f, 0.55f, 0.60f, 1.0f };  // clear color behind all imgui windows
   bool fullScreen = false;
   bool reloadShaders = false;
};

extern EngineConstants Const;
extern EngineState Engine;
extern GraphicObjects Graphics;
extern Texture* Textures;

typedef enum {
   GameButton_LEFT = 0,
   GameButton_RIGHT,
   GameButton_UP,
   GameButton_DOWN,
   GameButton_A,
   GameButton_B,
   GameButton_X,
   GameButton_Y,
   GameButton_LB,
   GameButton_LT,
   GameButton_RB,
   GameButton_RT,

   GameButton_COUNT
} GameButton_;
typedef byte GameButton;


struct IO {
   Coords mousePos = { 0,0 };

   Float2 leftStick;   // unit vector of left stick
   Float2 rightStick;  // unit vector of right stick

   f32 leftTrigger = 0.0f; // 0-1 analog trigger
   f32 rightTrigger = 0.0f;// 0-1 analog trigger

   bool buttonDown[GameButton_COUNT];
   bool buttonPressed[GameButton_COUNT];
   bool buttonReleased[GameButton_COUNT];
};

struct GameCamera {
   Rectf viewport;
};



struct Map {
   Float2 size;
};

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

enum ModeType_ {
   ModeType_ACTION = 0,
   ModeType_YOUDIED
};
typedef u16 ModeType;

struct GameMode {
   ModeType type = ModeType_ACTION;

   Milliseconds clock;
};



struct GameState {
   bool fullscreen = false;

   IO io;
   Map map = { { 10000, 10000 } };
   GameCamera camera = { { 0, 0, 426, 240 } }; // viewport into the world
   Rectf vpScreenArea = { 0, 0, 1, 1 }; // screen coordinates or the viewer image within the UI
   GameMode mode;

   struct {
      bool showMovementDebugging = false;
      bool showCollisionDebugging = false;
      f32 ambientLight = 0.0f;
   } DEBUG;

   Dude maindude;
   DynamicArray<Dude> baddudes;

   Time lastMouseMove;
   Time lastUpdate;

   int waveSize = 1;
};

void gameBeginFrame(GameState& g);

typedef union SDL_Event SDL_Event;
bool gameProcessEvent(GameState& g, SDL_Event* event);

void gameUpdate(GameState& g);
void gameDraw(GameState& g, FBO& output);

bool dudeAlive(Dude&d);


void gameStartActionMode(GameState &g);