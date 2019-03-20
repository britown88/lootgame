#pragma once

#include "math.h"
#include "render.h"
#include "assets.h"
#include "phy.h"
#include "coords.h"



typedef struct GameState GameState;

struct GraphicObjects {
   ShaderHandle shader = 0;

   Mesh mesh, meshUncentered;
   FBO
      unlitScene,
      lightLayer,
      litScene,
      UI;

   void build();
   bool reloadShaders();
};

void assetsSave();
void assetsLoad();

void assetsReloadMaps();
void assetsReloadTextures();
void assetsReloadAll();

// called in Graphics.Build()
void assetsBuildTextureHandles();


//@reflect{
struct EngineConstants {
   //@readonly
   Int2 resolution = { 1920, 1080 };
   Int2 vpSize = { 426, 240 };

   float floorHeight = 0.0f;
   float dudeHeight = 0.1f;

                                    // frame equivalent to ~16ms
   float dudeRotationSpeed = 0.15f; // radian per-frame angle the character can rotate by
   float dudeMoveSpeed = 0.080f;    // default starting max movespeed, measured per frame
   float dudeDashSpeed = 0.200f;    // max move speed during dash

   float dudeAccelerationRate = 0.0015f; // linear rate the speed cap increases toward the target cap
   float dudeDeccelerationRate = 0.003f; // linear rate the speed cap decreases toward the target cap

   float dudeBackwardsPenalty = 0.250f; // percentage of movespeed max removed by moving perfectly away from facing

   float dudeDashDistance = 40.0f;
   float dudeKnockbackDistance = 0.0f;
   Milliseconds dudePostDashCooldown = 300;
   Milliseconds dudeBaseStaminaTickRecoveryTime = 500;

   Milliseconds dudeMinimumSwingCooldown = 100; // global minimum cooldown after a swing, even if a combo is queued

   Milliseconds dudeSpentStaminaRecoveryTime = 400;
   Milliseconds dudeCrackedStaminaRecoveryTime = 800;
   Milliseconds overExtendedStaminaRecoveryTime = 2000;



   Milliseconds cooldownOnDamagedStaminaEmpty = 1000;
   Milliseconds cooldownOnDamagedStamina = 250;
   Milliseconds cooldownOnDamagedHealth = 500;
};//}

//@reflect{
struct EngineState {   
   ColorRGBAf bgClearColor = { 0.45f, 0.55f, 0.60f, 1.0f };  // clear color behind all imgui windows
   bool fullScreen = false;
   bool reloadShaders = false;
};//}

extern EngineState Engine;
extern GraphicObjects Graphics;

enum GameButton_ {
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
} ;
typedef byte GameButton;

//@reflect{
struct IO {
   Coords mousePos = { { 0,0 } };

   Float2 leftStick_RAW;
   Float2 rightStick_RAW;

   Float2 leftStick;   // unit vector of left stick
   Float2 rightStick;  // unit vector of right stick

   float leftTrigger = 0.0f; // 0-1 analog trigger
   float rightTrigger = 0.0f;// 0-1 analog trigger

   bool buttonDown[GameButton_COUNT];
   bool buttonPressed[GameButton_COUNT];
   bool buttonReleased[GameButton_COUNT];
};//}

//@reflect{
struct GameCamera {
   Rectf viewport;
   
};//}

//@reflect{
struct Wall {
   ConvexPoly poly;

   //@readonly
   Rectf bb;

   //@ignore{
   Array<PhyObject> phyObjs;
   //}
}; //}

//@reflect{
struct Light {
   ColorRGBAf color = White;

   Coords pos;
   float radius = 100.0f;
   float height = 0.2f, 
      linearPortion = 0.5f, 
      smoothingFactor = 0.4f, 
      intensity = 1.0f;
}; //}

//@reflect{
struct Map {
   Float2 size; 
   Array<Wall> walls;
   Array<Light> lights;

   // @readonly
   Symbol* id = nullptr;

   //@ignore{
   bool markForDelete = false;
   //}
};//}

//@reflect{
enum SwingPhase {
   SwingPhase_Windup = 0,
   SwingPhase_Lunge,
   SwingPhase_Swing,
   SwingPhase_Cooldown
};//}

//@reflect{
struct AttackSwing {
   float lungeDist; // character will luinge forward between windup and swing
   float lungeSpeed; // perms

   float swipeAngle; // full range of the weapon swipe, in degrees
   Milliseconds swingDur; // total time for the attack
   Milliseconds windupDur; // vulnerability period before swing
   Milliseconds cooldownDur; // period before user is free again
   Rectf hitbox; // axis-aligned, The origin here is the bottom center of the attack while facing up
                 // attack will rotate around that origin

   int staminaDamage = 1;
   int healthDamage = 1;
   int armorDamage = 1;
};//}

//@reflect{
struct MoveSet {
   Array<AttackSwing> swings;

   // @readonly
   Symbol* id = nullptr;
};//}

//@reflect{
struct GameAssets {
   EngineConstants constants;
   std::unordered_map<Symbol*, Texture> textures;
   std::unordered_map<Symbol*, Map> maps;
};//}

extern GameAssets Assets;
extern EngineConstants& Const;
#include "assets_gen.h"



  /*
  MOVEMENT PRIMER

  // movement
   moveVector is best thought as the analogue movement stick, its length is 0-1

   Velocity accelerates by moveVector every frame, with a capped length of moveSpeedCap

   moveSpeedCap linearly grows/shrinks by Const.MoveSpeedEasing toward a targetCap
   targetCap is defined by the character's speed multiplied by the length of moveVector
   speed is the character's max speed reduced by up to dudeBackwardsPenalty% depending on face direciton vs move direction

  // rotation
   faceVector is the normalized direction to face, this is the target to face
   facing is the curent normalized vector the character is facing.  
   facing will rotate by Const.rotationSpeed radians every frame toward faceVector

  */


//@reflect{
struct Movement {
   float moveSpeedCap = 0.0f;       // updated per frame, interpolates toward moveSpeedCapTarget
   float moveSpeedCapTarget = 0.0f; // updated per frame, max speed based on length of move vector and velocity direction vs facing
   Float2 moveVector = { 0.0f, 0.0f };   // vector length 0-1 for movement amount/direction
   Float2 faceVector = { 0.0f, 0.0f };  // unit vector for target facing, facing will interpolate toward this angle
   Float2 facing = { 1, 0 };  // unit vector for character facing, interpolated every frame toward faceVector
};//}

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

   Array<Dude*> hits;
   bool overExtended = false;
   bool queuedAttacked = false; // this will need to later be the requested action
};

struct CooldownState {
   Milliseconds duration;
};

struct DashState {
   bool postDashAttack = false;
};

struct FreeState {
   //Milliseconds staminaClockStart;
   //Milliseconds nextTickAt;
};

//@reflect{
enum DudeState {
   DudeState_FREE = 0,
   DudeState_COOLDOWN,
   DudeState_DASH,
   DudeState_ATTACKING,
   DudeState_DEAD
};//}

enum PipState {
   PipState_Full,
   PipState_Spent,
   PipState_Cracked
};
struct StaminaPip {
   Milliseconds charge = 0;
   Milliseconds fullCharge = 0;
   PipState state = PipState_Full;
};

struct Status {
   Array<StaminaPip> stamina;
   int health, healthMax;
};

// shove keeps track of a push of constant speed for a distance
// shoves can happen independant of state
struct Shove {
   Float2 startPos;
   Milliseconds start;
   Milliseconds dur;
   float speed; // only stored so we can restore speed correctly
};

struct Dude {
   DudeState state = DudeState_FREE;
   AttackState atk;
   CooldownState cd;
   DashState dash;
   FreeState free;

   Texture* texture = nullptr;
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
typedef uint16_t ModeType;

struct GameModeStateAction {
   int waveSize = 1;
};

struct GameMode {
   ModeType type = ModeType_ACTION;

   GameModeStateAction action;
   Milliseconds clock;
};

//@reflect{
struct GameStateDebug {
   //bool showMovementDebugging = false;
   //bool showCollisionDebugging = false;

   //@ui(min=0 max=1)
   float ambientLight = 0.2f;

   bool AI = true;
};
//}

enum GameEditMode {
   GameEditMode_None = 0,
   GameEditMode_Walls,
   GameEditMode_Lights,
   GameEditMode_Move,
};

struct GameStateUI {
   bool fullscreen = false;
   bool focused = false;
   bool editing = false;

   bool showGameUI = true;
   Float2 gridSize = { 100.0f, 100.0f };
   bool showEditGrid = true;

   GameEditMode mode = GameEditMode_None;

   Wall newWall;
   Wall* editWall = nullptr;

   Light newLight;
   Light* editLight = nullptr;

   Coords rightClickMousePos;
};

struct PhysicsSystem {
   Array<PhyObject*> objs;
};

extern Map _defaultMap;

struct GameState {

   GameStateUI ui;
   IO io;
   Map *map = &_defaultMap;
   GameCamera camera;// = { { 0, 0, (float)Const.vpSize.x, (float)Const.vpSize.y } }; // viewport into the world
   Rectf vpScreenArea = { 0, 0, 1, 1 }; // screen coordinates or the viewer image within the UI
   GameMode mode;

   GameStateDebug DEBUG;

   Dude maindude;
   Array<Dude> baddudes;

   Time lastMouseMove;

   Time lastUpdate; // appGetTime of last gameUpdate
   bool paused = false;

   Milliseconds gameClock = 0; // ticks since game started
   Milliseconds frameClock = 0; // ticks since last frame step
   Milliseconds otherFrameClock = 0; // ticks since last 'every other frame' step

   PhysicsSystem phySys;

   int waveSize = 1;
};

struct GameInstance {
   std::string winTitle;
   GameState state;
   FBO outputFbo;
};

extern GameState* DEBUG_CurrentGame;

void gameBeginFrame(GameState& g);

typedef union SDL_Event SDL_Event;
bool gameProcessEvent(GameState& g, SDL_Event* event);

void gameUpdate(GameState& g);
void gameDraw(GameState& g, FBO& output);

bool dudeAlive(Dude&d);


void gameStartActionMode(GameState &g);
void gameUpdateActionMode(GameState &g);
void gameUpdateYouDiedMode(GameState &g);

void DEBUG_gameSpawnDude(GameState& game, Coords location);