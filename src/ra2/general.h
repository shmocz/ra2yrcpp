#pragma once

#ifdef __cplusplus
#include <cstdint>
#define ENUMDEF(name, etype) enum class name : etype

#else
#define ENUMDEF(name, etype) enum name
#endif

#ifdef __cplusplus
namespace ra2 {
namespace general {
#endif

#define X(s) typedef uint##s##_t u##s
X(8);
X(16);
X(32);
X(64);
#undef X
#define X(s) typedef int##s##_t i##s
X(8);
X(16);
X(32);
X(64);
#undef X

typedef u8 ybool;

ENUMDEF(AbstractFlags, unsigned int){None = 0x0, Techno = 0x1, Object = 0x2,
                                     Foot = 0x4};

// 0x7e3ee8
#ifdef __cplusplus
enum class AbstractType : unsigned int
#else
enum AbstractType
#endif
{
  None = 0,
  Unit = 1,
  Aircraft = 2,
  AircraftType = 3,
  Anim = 4,
  AnimType = 5,
  Building = 6,
  BuildingType = 7,
  Bullet = 8,
  BulletType = 9,
  Campaign = 10,
  Cell = 11,
  Factory = 12,
  House = 13,
  HouseType = 14,
  Infantry = 15,
  InfantryType = 16,
  Isotile = 17,
  IsotileType = 18,
  BuildingLight = 19,
  Overlay = 20,
  OverlayType = 21,
  Particle = 22,
  ParticleType = 23,
  ParticleSystem = 24,
  ParticleSystemType = 25,
  Script = 26,
  ScriptType = 27,
  Side = 28,
  Smudge = 29,
  SmudgeType = 30,
  Special = 31,
  SuperWeaponType = 32,
  TaskForce = 33,
  Team = 34,
  TeamType = 35,
  Terrain = 36,
  TerrainType = 37,
  Trigger = 38,
  TriggerType = 39,
  UnitType = 40,
  VoxelAnim = 41,
  VoxelAnimType = 42,
  Wave = 43,
  Tag = 44,
  TagType = 45,
  Tiberium = 46,
  Action = 47,
  Event = 48,
  WeaponType = 49,
  WarheadType = 50,
  Waypoint = 51,
  Abstract = 52,
  Tube = 53,
  LightSource = 54,
  EMPulse = 55,
  TacticalMap = 56,
  Super = 57,
  AITrigger = 58,
  AITriggerType = 59,
  Neuron = 60,
  FoggedObject = 61,
  AlphaShape = 62,
  VeinholeMonster = 63,
  NavyType = 64,
  SpawnManager = 65,
  CaptureManager = 66,
  Parasite = 67,
  Bomb = 68,
  RadSite = 69,
  Temporal = 70,
  Airstrike = 71,
  SlaveManager = 72,
  DiskLaser = 73,
  TechnoType = 74,  // These are additional helpers
  Object = 75,
  ObjectType = 76,
  Tactical = 77,
  Last = 78
};

#ifdef __cplusplus
enum class Armor : unsigned int
#else
enum Armor
#endif
{
  None = 0,
  Flak = 1,
  Plate = 2,
  Light = 3,
  Medium = 4,
  Heavy = 5,
  Wood = 6,
  Steel = 7,
  Concrete = 8,
  Special_1 = 9,
  Special_2 = 10,
  End = 11
};

ENUMDEF(SpeedType, int){None = -1,      Foot = 0,   Track = 1, Wheel = 2,
                        Hover = 3,      Winged = 4, Float = 5, Amphibious = 6,
                        FloatBeach = 7, End = 8};

ENUMDEF(BuildCat, unsigned int){DontCare = 0, Tech = 1,           Resoure = 2,
                                Power = 3,    Infrastructure = 4, Combat = 5};

ENUMDEF(BStateType, unsigned int){
    Construction = 0x0, Idle = 0x1, Active = 0x2, Full = 0x3,
    Aux1 = 0x4,         Aux2 = 0x5, Count = 0x6,  None = 0xFFFFFFFF};

ENUMDEF(NetworkEvents, unsigned char){
    Empty = 0x0,         PowerOn = 0x1,        PowerOff = 0x2,
    Ally = 0x3,          MegaMission = 0x4,    MegaMissionF = 0x5,
    Idle = 0x6,          Scatter = 0x7,        Destruct = 0x8,
    Deploy = 0x9,        Detonate = 0xA,       Place = 0xB,
    Options = 0xC,       GameSpeed = 0xD,      Produce = 0xE,
    Suspend = 0xF,       Abandon = 0x10,       Primary = 0x11,
    SpecialPlace = 0x12, Exit = 0x13,          Animation = 0x14,
    Repair = 0x15,       Sell = 0x16,          SellCell = 0x17,
    Special = 0x18,      FrameSync = 0x19,     Message = 0x1A,
    ResponseTime = 0x1B, FrameInfo = 0x1C,     SaveGame = 0x1D,
    Archive = 0x1E,      AddPlayer = 0x1F,     Timing = 0x20,
    ProcessTime = 0x21,  PageUser = 0x22,      RemovePlayer = 0x23,
    LatencyFudge = 0x24, MegaFrameInfo = 0x25, PacketTiming = 0x26,
    AboutToExit = 0x27,  FallbackHost = 0x28,  AddressChange = 0x29,
    PlanConnect = 0x2A,  PlanCommit = 0x2B,    PlanNodeDelete = 0x2C,
    AllCheer = 0x2D,     AbandonAll = 0x2E};

ENUMDEF(Mission, int){None = -1,
                      Sleep = 0,
                      Attack = 1,
                      Move = 2,
                      QMove = 3,
                      Retreat = 4,
                      Guard = 5,
                      Sticky = 6,
                      Enter = 7,
                      Capture = 8,
                      Eaten = 9,
                      Harvest = 10,
                      Area_Guard = 11,
                      Return = 12,
                      Stop = 13,
                      Ambush = 14,
                      Hunt = 15,
                      Unload = 16,
                      Sabotage = 17,
                      Construction = 18,
                      Selling = 19,
                      Repair = 20,
                      Rescue = 21,
                      Missile = 22,
                      Harmless = 23,
                      Open = 24,
                      Patrol = 25,
                      ParadropApproach = 26,
                      ParadropOverfly = 27,
                      Wait = 28,
                      AttackMove = 29,
                      SpyplaneApproach = 30,
                      SpyplaneOverfly = 31};

struct LTRBStruct {
  int Left;
  int Top;
  int Right;
  int Bottom;
};

struct RectangleStruct {
  i32 x;
  i32 y;
  i32 width;
  i32 height;
};

#ifdef __cplusplus
}  // namespace general
}  // namespace ra2
#endif
