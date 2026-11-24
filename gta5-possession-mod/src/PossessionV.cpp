// PossessionV - Script Hook V .asi for the GTA V iOS port (and PC Script Hook V).
//
//  * Possess (take control of) the nearest pedestrian or animal with CHANGE_PLAYER_PED,
//    and switch back to the original character later.
//  * Keep the Vanilla Unicorn front, back, office and dressing-room doors unlocked.
//  * A small on-screen menu (drawn with DRAW_RECT/text so the iOS port shows its touch
//    D-pad) plus direct hotkeys that can be bound to on-screen buttons in the port.
//
// Built without the C runtime: the only imports are six Script Hook V functions and
// GetModuleFileNameA / GetPrivateProfileIntA / GetAsyncKeyState.
#include "shv.h"

// ---------------------------------------------------------------------------------------
// Freestanding support (the compiler may emit these; we link no CRT).
extern "C" {
int _fltused = 0;
void *memset(void *d, int c, unsigned long long n) {
  volatile unsigned char *p = (volatile unsigned char *)d;
  while (n--) *p++ = (unsigned char)c;
  return d;
}
void *memcpy(void *d, const void *s, unsigned long long n) {
  volatile unsigned char *p = (volatile unsigned char *)d;
  const unsigned char *q = (const unsigned char *)s;
  while (n--) *p++ = *q++;
  return d;
}
}

// ---------------------------------------------------------------------------------------
// Native calls. Every hash below is listed in the port's Scripts/asi_native_hashes.txt.
namespace N {
enum : UINT64 {
  PLAYER_ID = 0x4F8644AF03D0E0D6ull,
  PLAYER_PED_ID = 0xD80958FC74E988A6ull,
  CHANGE_PLAYER_PED = 0x048189FAC643DEEEull,
  GET_PED_NEARBY_PEDS = 0x23F8F5FC7E8C4A6Bull,
  GET_PED_TYPE = 0xFF059E1E4C01E63Cull,
  IS_PED_HUMAN = 0xB980061DA992779Dull,
  IS_PED_A_PLAYER = 0x12534C348C6CB68Bull,
  DOES_ENTITY_EXIST = 0x7239B21A38F536BAull,
  IS_ENTITY_DEAD = 0x5F9532F3B5CC2551ull,
  GET_ENTITY_COORDS = 0x3FEF770D40960D5Aull,
  SET_ENTITY_AS_MISSION_ENTITY = 0xAD738C3085FE7E11ull,
  SET_PED_AS_NO_LONGER_NEEDED = 0x2595DD4236549CE3ull,
  SET_ENTITY_INVINCIBLE = 0x3882114BDE571AD4ull,
  TASK_STAND_STILL = 0x919BE13EED931959ull,
  CLEAR_PED_TASKS = 0xE1EF3C1216AFF2CDull,
  SET_BLOCKING_OF_NON_TEMPORARY_EVENTS = 0x9F8AA94D6D97DBF4ull,
  ADD_BLIP_FOR_ENTITY = 0x5CDE92C702A8FCE7ull,
  REMOVE_BLIP = 0x86A652570E5F25DDull,
  DOES_BLIP_EXIST = 0xA6DB27D19ECBB7DAull,
  SET_BLIP_COLOUR = 0x03D7FB09E75D6B7Eull,
  GET_CLOSEST_OBJECT_OF_TYPE = 0xE143FA2249364369ull,
  SET_STATE_OF_CLOSEST_DOOR_OF_TYPE = 0xF82D8F1926A02C3Dull,
  DOOR_SYSTEM_FIND_EXISTING_DOOR = 0x589F80B325CC82C5ull,
  DOOR_SYSTEM_SET_DOOR_STATE = 0x6BAB9442830C7F53ull,
  GET_INTERIOR_AT_COORDS = 0xB0F7F8663821D9C3ull,
  IS_INTERIOR_DISABLED = 0xBC5115A5A939DD15ull,
  DISABLE_INTERIOR = 0x6170941419D7D8ECull,
  IS_INTERIOR_CAPPED = 0x92BAC8ACF88CEC26ull,
  CAP_INTERIOR = 0xD9175F941610DB54ull,
  DRAW_RECT = 0x3A618A217E5154F0ull,
  SET_TEXT_FONT = 0x66E0276CC5F6B9DAull,
  SET_TEXT_SCALE = 0x07C837F9A01C34C9ull,
  SET_TEXT_COLOUR = 0xBE6B23FFA53FB442ull,
  SET_TEXT_OUTLINE = 0x2513DFB0FB8400FEull,
  BEGIN_TEXT_COMMAND_DISPLAY_TEXT = 0x25FBB336DF1804CBull,
  ADD_TEXT_COMPONENT_SUBSTRING_PLAYER_NAME = 0x6C188BE134E074AAull,
  END_TEXT_COMMAND_DISPLAY_TEXT = 0xCD015E5BB0D96A57ull,
  BEGIN_TEXT_COMMAND_THEFEED_POST = 0x202709F4C58A0424ull,
  END_TEXT_COMMAND_THEFEED_POST_TICKER = 0x2ED7843F8F801023ull,
  GET_GAME_TIMER = 0x9CD27B0045628463ull,
  GIVE_WEAPON_TO_PED = 0xBF0FD6E56C964FCBull,
  IS_WEAPON_VALID = 0x937C71165CF334B3ull,
  STAT_GET_INT = 0x767FBC2AC802EF3Dull,
  STAT_SET_INT = 0xB3271D7AB655B441ull,
  GET_NAME_OF_ZONE = 0xCD90657D4C30E1CAull,
  GET_PLAYER_WANTED_LEVEL = 0xE28E54788CE8F12Dull,
  CLEAR_PLAYER_WANTED_LEVEL = 0xB302540597885499ull,
  SET_MAX_WANTED_LEVEL = 0xAA5F02DB48D704B9ull,
  SET_CLOCK_TIME = 0x47C3B5848C3E45D8ull,
  GET_ENTITY_MODEL = 0x9F47B058362C84B5ull,
  DRAW_MARKER = 0x28477EC23D892089ull,
  IS_ENTITY_A_MISSION_ENTITY = 0x0A7B270912999B3Cull,
  IS_ENTITY_A_PED = 0x524AC5ECEA15343Eull,
  IS_PED_FATALLY_INJURED = 0xD839450756ED5A80ull,
  GET_STATE_OF_CLOSEST_DOOR_OF_TYPE = 0xEDC1A5B84AEF33FFull,
  DOOR_SYSTEM_GET_DOOR_STATE = 0x160AA1B32F6139B8ull,
};
}

static inline UINT64 A(int v) { return (UINT64)(int64_t)v; }
static inline UINT64 A(unsigned v) { return (UINT64)v; }
static inline UINT64 A(bool v) { return v ? 1 : 0; }
static inline UINT64 A(float v) { uint32_t b; memcpy(&b, &v, 4); return (UINT64)b; }
static inline UINT64 A(const void *p) { return (UINT64)p; }

static UINT64 *Call(UINT64 hash) { nativeInit(hash); return nativeCall(); }
template <typename... T> static UINT64 *Call(UINT64 hash, T... a) {
  nativeInit(hash);
  (nativePush64(A(a)), ...);
  return nativeCall();
}
template <typename... T> static int CallI(UINT64 hash, T... a) { return (int)(uint32_t)*Call(hash, a...); }
template <typename... T> static bool CallB(UINT64 hash, T... a) { return ((uint32_t)*Call(hash, a...)) != 0; }

struct Vec3 { float x, y, z; };
static Vec3 Coords(int entity) {
  UINT64 *r = Call(N::GET_ENTITY_COORDS, entity, true);  // Vector3: 3 floats, 8-byte slots
  Vec3 v; memcpy(&v.x, &r[0], 4); memcpy(&v.y, &r[1], 4); memcpy(&v.z, &r[2], 4);
  return v;
}
static float Dist2(Vec3 a, Vec3 b) { float x = a.x - b.x, y = a.y - b.y, z = a.z - b.z; return x * x + y * y + z * z; }
// Distances are compared as the bit patterns of non-negative floats (same order as the
// values; NaN sorts above everything). The GTA V iOS port's x86 interpreter (Blink,
// OpCmppsd) stores -1.0f instead of an all-ones mask for CMPSS/CMPPS, so compiler-generated
// branchless float compares give wrong answers there. Integer compares avoid them entirely.
static inline uint32_t Key(float nonNegative) { uint32_t b; memcpy(&b, &nonNegative, 4); return b & 0x7FFFFFFFu; }

// Jenkins one-at-a-time hash (GET_HASH_KEY), lower-cased, computed at compile time.
static constexpr uint32_t Joaat(const char *s) {
  uint32_t h = 0;
  for (; *s; ++s) {
    char c = *s; if (c >= 'A' && c <= 'Z') c = (char)(c + 32);
    h += (unsigned char)c; h += h << 10; h ^= h >> 6;
  }
  h += h << 3; h ^= h >> 11; h += h << 15;
  return h;
}

// ---------------------------------------------------------------------------------------
// Settings (PossessionV.ini next to the .asi).
static HMODULE g_module;
static char g_ini[300];
struct Settings {
  int menuKey = 118;          // F7
  int possessPedKey = 120;    // F9
  int possessAnimalKey = 121; // F10
  int returnKey = 122;        // F11
  int nextTargetKey = 116;    // F5: mark the next nearby ped as the target
  int doorsKey = 119;         // F8: Vanilla Unicorn doors UNLOCK / LOCK (restore)
  int modeKey = 123;          // F12: possession mode ON / OFF
  bool possessionEnabled = true;
  bool allowMissionPeds = false;  // peds other scripts own (story/mission characters) are skipped
  bool showMarker = true;
  bool clearWantedOnReturn = false;
  int maxDistance = 60;       // metres
  bool protectOriginal = true;
  bool autoReturnOnDeath = true;
  bool unicornDoors = true;
  bool unicornInterior = true;
  bool restrictedNoWanted = true;  // no wanted level inside Fort Zancudo, Humane Labs, the prison, LSIA
  bool neverWanted = false;
  bool autoWeapons = false;        // give all weapons to every body you possess
  int moneyAmount = 1000000;
} S;

static void LoadSettings() {
  DWORD n = GetModuleFileNameA(g_module, g_ini, sizeof g_ini - 5);
  if (n == 0 || n >= sizeof g_ini - 5) { const char *d = "PossessionV.ini"; int i = 0; for (; d[i]; i++) g_ini[i] = d[i]; g_ini[i] = 0; }
  else {
    int dot = -1; for (int i = (int)n - 1; i >= 0 && g_ini[i] != '\\' && g_ini[i] != '/'; i--) if (g_ini[i] == '.') { dot = i; break; }
    int at = dot >= 0 ? dot : (int)n; g_ini[at] = '.'; g_ini[at + 1] = 'i'; g_ini[at + 2] = 'n'; g_ini[at + 3] = 'i'; g_ini[at + 4] = 0;
  }
  S.menuKey = (int)GetPrivateProfileIntA("Keys", "MenuKey", S.menuKey, g_ini);
  S.possessPedKey = (int)GetPrivateProfileIntA("Keys", "PossessPedKey", S.possessPedKey, g_ini);
  S.possessAnimalKey = (int)GetPrivateProfileIntA("Keys", "PossessAnimalKey", S.possessAnimalKey, g_ini);
  S.returnKey = (int)GetPrivateProfileIntA("Keys", "ReturnKey", S.returnKey, g_ini);
  S.nextTargetKey = (int)GetPrivateProfileIntA("Keys", "NextTargetKey", S.nextTargetKey, g_ini);
  S.doorsKey = (int)GetPrivateProfileIntA("Keys", "DoorsKey", S.doorsKey, g_ini);
  S.modeKey = (int)GetPrivateProfileIntA("Keys", "PossessionModeKey", S.modeKey, g_ini);
  S.possessionEnabled = GetPrivateProfileIntA("Possession", "Enabled", 1, g_ini) != 0;
  S.allowMissionPeds = GetPrivateProfileIntA("Possession", "AllowMissionPeds", 0, g_ini) != 0;
  S.showMarker = GetPrivateProfileIntA("Possession", "ShowTargetMarker", 1, g_ini) != 0;
  S.clearWantedOnReturn = GetPrivateProfileIntA("Possession", "ClearWantedOnReturn", 0, g_ini) != 0;
  S.maxDistance = (int)GetPrivateProfileIntA("Possession", "MaxDistance", S.maxDistance, g_ini);
  S.protectOriginal = GetPrivateProfileIntA("Possession", "ProtectOriginal", 1, g_ini) != 0;
  S.autoReturnOnDeath = GetPrivateProfileIntA("Possession", "AutoReturnOnDeath", 1, g_ini) != 0;
  S.unicornDoors = GetPrivateProfileIntA("VanillaUnicorn", "UnlockDoors", 1, g_ini) != 0;
  S.unicornInterior = GetPrivateProfileIntA("VanillaUnicorn", "EnableInterior", 1, g_ini) != 0;
  S.restrictedNoWanted = GetPrivateProfileIntA("Unlocks", "RestrictedAreasNoWanted", 1, g_ini) != 0;
  S.neverWanted = GetPrivateProfileIntA("Unlocks", "NeverWanted", 0, g_ini) != 0;
  S.autoWeapons = GetPrivateProfileIntA("Unlocks", "AutoGiveWeapons", 0, g_ini) != 0;
  S.moneyAmount = (int)GetPrivateProfileIntA("Unlocks", "MoneyAmount", S.moneyAmount, g_ini);
  if (S.moneyAmount < 1) S.moneyAmount = 1;
  if (S.moneyAmount > 100000000) S.moneyAmount = 100000000;
  if (S.maxDistance < 5) S.maxDistance = 5;
  if (S.maxDistance > 300) S.maxDistance = 300;
}

// ---------------------------------------------------------------------------------------
// Feedback.
static void Notify(const char *text) {
  Call(N::BEGIN_TEXT_COMMAND_THEFEED_POST, "STRING");
  Call(N::ADD_TEXT_COMPONENT_SUBSTRING_PLAYER_NAME, text);
  Call(N::END_TEXT_COMMAND_THEFEED_POST_TICKER, false, false);
}
static void Text(const char *text, float x, float y, float scale, int r, int g, int b) {
  Call(N::SET_TEXT_FONT, 0);
  Call(N::SET_TEXT_SCALE, 0.0f, scale);
  Call(N::SET_TEXT_COLOUR, r, g, b, 255);
  Call(N::SET_TEXT_OUTLINE);
  Call(N::BEGIN_TEXT_COMMAND_DISPLAY_TEXT, "STRING");
  Call(N::ADD_TEXT_COMPONENT_SUBSTRING_PLAYER_NAME, text);
  Call(N::END_TEXT_COMMAND_DISPLAY_TEXT, x, y, 0);
}
static void Rect(float x, float y, float w, float h, int r, int g, int b, int a) {
  Call(N::DRAW_RECT, x, y, w, h, r, g, b, a, false);
}

// ---------------------------------------------------------------------------------------
// Possession: you take over a real pedestrian with CHANGE_PLAYER_PED. The ped keeps its own
// model, clothes and appearance; the game hands it the player's controls and camera.
enum Kind { KIND_HUMAN, KIND_ANIMAL, KIND_ANY };
static const int PED_TYPE_ANIMAL = 28;

static int g_original;      // the character we started as (0: not possessing)
static int g_originalBlip;  // blip on the original while away
static bool g_originalLostTold;

static bool Alive(int ped) { return ped && CallB(N::DOES_ENTITY_EXIST, ped) && !CallB(N::IS_ENTITY_DEAD, ped, false); }
static bool IsAnimal(int ped) { return CallI(N::GET_PED_TYPE, ped) == PED_TYPE_ANIMAL || !CallB(N::IS_PED_HUMAN, ped); }

// Everything that must be true before the game is asked to switch: a living, non-player ped
// that isn't your original character and (unless allowed) isn't owned by a mission script.
static bool CanPossess(int ped, int me, Kind kind) {
  if (!ped || ped == me || ped == g_original) return false;
  if (!CallB(N::DOES_ENTITY_EXIST, ped) || !CallB(N::IS_ENTITY_A_PED, ped)) return false;
  if (CallB(N::IS_ENTITY_DEAD, ped, false) || CallB(N::IS_PED_FATALLY_INJURED, ped)) return false;
  if (CallB(N::IS_PED_A_PLAYER, ped)) return false;
  if (!S.allowMissionPeds && CallB(N::IS_ENTITY_A_MISSION_ENTITY, ped)) return false;
  if (kind != KIND_ANY) { bool animal = IsAnimal(ped); if ((kind == KIND_HUMAN) == animal) return false; }
  return true;
}

// Nearby candidates, nearest first (distances compared as integers, see Key()).
static int g_targets[16], g_targetCount, g_selected, g_nextTargetRefresh;
static int Candidates(Kind kind, int *out, int cap) {
  int me = CallI(N::PLAYER_PED_ID);
  Vec3 at = Coords(me);
  const int CAP = 64;
  UINT64 list[CAP + 1];          // script array: [0] = capacity, then 8-byte slots
  memset(list, 0, sizeof list);
  list[0] = CAP;
  int count = CallI(N::GET_PED_NEARBY_PEDS, me, (const void *)list, -1);
  if (count < 0) count = 0;
  if (count > CAP) count = CAP;
  const uint32_t maxKey = Key((float)(S.maxDistance * S.maxDistance));
  uint32_t keys[16]; int n = 0;
  for (int i = 0; i < count; i++) {
    int ped = (int)(uint32_t)list[i + 1];
    if (!CanPossess(ped, me, kind)) continue;
    uint32_t d = Key(Dist2(at, Coords(ped)));
    if (d > maxKey) continue;
    int pos = n < cap ? n : cap;                 // insertion sort, keep the nearest `cap`
    while (pos > 0 && keys[pos - 1] > d) { if (pos < cap) { keys[pos] = keys[pos - 1]; out[pos] = out[pos - 1]; } pos--; }
    if (pos < cap) { keys[pos] = d; out[pos] = ped; if (n < cap) n++; }
  }
  return n;
}
static int FindNearest(Kind kind) { int one[1]; return Candidates(kind, one, 1) ? one[0] : 0; }

static void RefreshTargets() {
  g_targetCount = Candidates(KIND_ANY, g_targets, 16);
  bool kept = false;
  for (int i = 0; i < g_targetCount; i++) if (g_targets[i] == g_selected) kept = true;
  if (!kept) g_selected = g_targetCount ? g_targets[0] : 0;  // default: the nearest
}
static void NextTarget() {
  if (!S.possessionEnabled) { Notify("PossessionV: possession mode is OFF."); return; }
  RefreshTargets();
  if (!g_targetCount) { g_selected = 0; Notify("PossessionV: nobody nearby to target."); return; }
  int at = 0; for (int i = 0; i < g_targetCount; i++) if (g_targets[i] == g_selected) at = i;
  g_selected = g_targets[(at + 1) % g_targetCount];
}
static void TargetTick(int now) {
  if (!S.possessionEnabled) { g_selected = 0; return; }
  if (now >= g_nextTargetRefresh) { g_nextTargetRefresh = now + 500; RefreshTargets(); }
  if (!S.showMarker || !g_selected || !CallB(N::DOES_ENTITY_EXIST, g_selected)) return;
  Vec3 p = Coords(g_selected);
  // Marker type 0 (upside-down cone) floating over the target's head, facing the camera.
  Call(N::DRAW_MARKER, 0, p.x, p.y, p.z + 1.25f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.3f, 0.3f, 0.3f,
       255, 60, 200, 200, true, true, 2, false, (const void *)0, (const void *)0, false);
}

static void ReleasePed(int ped) {
  if (!ped || !CallB(N::DOES_ENTITY_EXIST, ped)) return;
  Call(N::SET_BLOCKING_OF_NON_TEMPORARY_EVENTS, ped, false);
  Call(N::CLEAR_PED_TASKS, ped);
  int handle = ped;
  Call(N::SET_PED_AS_NO_LONGER_NEEDED, (const void *)&handle);
}

static void ParkOriginal(int ped) {
  if (S.protectOriginal) Call(N::SET_ENTITY_INVINCIBLE, ped, true);
  Call(N::SET_BLOCKING_OF_NON_TEMPORARY_EVENTS, ped, true);
  Call(N::TASK_STAND_STILL, ped, -1);
  if (!g_originalBlip || !CallB(N::DOES_BLIP_EXIST, g_originalBlip)) {
    g_originalBlip = CallI(N::ADD_BLIP_FOR_ENTITY, ped);
    if (g_originalBlip) Call(N::SET_BLIP_COLOUR, g_originalBlip, 2);  // green
  }
}
static void RemoveOriginalBlip() {
  if (g_originalBlip && CallB(N::DOES_BLIP_EXIST, g_originalBlip)) { int b = g_originalBlip; Call(N::REMOVE_BLIP, (const void *)&b); }
  g_originalBlip = 0;
}

static void GiveAllWeapons(int ped, bool quiet);
static void PossessPed(int target) {
  int me = CallI(N::PLAYER_PED_ID);
  if (!Alive(me)) { Notify("PossessionV: you can't possess anyone while dead."); return; }
  if (!CanPossess(target, me, KIND_ANY)) { Notify("PossessionV: that ped can't be possessed (gone, dead, a player or a mission character)."); return; }
  int player = CallI(N::PLAYER_ID);
  bool first = g_original == 0;
  Call(N::SET_ENTITY_AS_MISSION_ENTITY, target, true, true);
  if (first) Call(N::SET_ENTITY_AS_MISSION_ENTITY, me, true, true);
  Call(N::CLEAR_PED_TASKS, target);
  Call(N::CHANGE_PLAYER_PED, player, target, false, false);
  if (CallI(N::PLAYER_PED_ID) != target) {
    int h = target; Call(N::SET_PED_AS_NO_LONGER_NEEDED, (const void *)&h);
    Notify("PossessionV: the game refused to switch to that character.");
    return;
  }
  if (first) { g_original = me; g_originalLostTold = false; ParkOriginal(me); }
  else ReleasePed(me);  // hopping from one possessed body to the next: let the old one go
  g_selected = 0; g_nextTargetRefresh = 0;
  Notify(IsAnimal(target) ? "PossessionV: animal possessed." : "PossessionV: pedestrian possessed.");
  if (S.autoWeapons && !IsAnimal(target)) GiveAllWeapons(target, true);
}
static void Possess(Kind kind) {
  if (!S.possessionEnabled) { Notify("PossessionV: possession mode is OFF (turn it on in the menu)."); return; }
  int target = FindNearest(kind);
  if (!target) {
    Notify(kind == KIND_ANIMAL ? "PossessionV: no animal nearby." : kind == KIND_HUMAN ? "PossessionV: no pedestrian nearby." : "PossessionV: nobody nearby.");
    return;
  }
  PossessPed(target);
}
static void PossessTarget() {  // the marked ped, or the nearest anyone
  if (!S.possessionEnabled) { Notify("PossessionV: possession mode is OFF (turn it on in the menu)."); return; }
  int me = CallI(N::PLAYER_PED_ID);
  if (g_selected && CanPossess(g_selected, me, KIND_ANY)) PossessPed(g_selected);
  else Possess(KIND_ANY);
}

static void ReturnToOriginal(bool quiet) {
  if (!g_original) { if (!quiet) Notify("PossessionV: you are already your original character."); return; }
  if (!Alive(g_original)) {
    if (!quiet) Notify("PossessionV: your original character is gone; you stay in this body.");
    RemoveOriginalBlip(); g_original = 0; return;
  }
  int body = CallI(N::PLAYER_PED_ID);
  int player = CallI(N::PLAYER_ID);
  Call(N::CHANGE_PLAYER_PED, player, g_original, false, false);
  if (CallI(N::PLAYER_PED_ID) != g_original) { Notify("PossessionV: the game refused to switch back; try again in a moment."); return; }
  Call(N::SET_ENTITY_INVINCIBLE, g_original, false);
  Call(N::SET_BLOCKING_OF_NON_TEMPORARY_EVENTS, g_original, false);
  Call(N::CLEAR_PED_TASKS, g_original);
  RemoveOriginalBlip();
  if (body != g_original) ReleasePed(body);
  g_original = 0; g_selected = 0; g_nextTargetRefresh = 0;
  if (S.clearWantedOnReturn) Call(N::CLEAR_PLAYER_WANTED_LEVEL, player);
  if (!quiet) Notify("PossessionV: back to your original character.");
}

static void PossessionTick() {
  if (!g_original) return;
  if (!CallB(N::DOES_ENTITY_EXIST, g_original) || CallB(N::IS_ENTITY_DEAD, g_original, false)) {
    if (!g_originalLostTold) { Notify("PossessionV: your original character died or despawned."); g_originalLostTold = true; }
    RemoveOriginalBlip(); g_original = 0; return;
  }
  if (S.autoReturnOnDeath && CallB(N::IS_ENTITY_DEAD, CallI(N::PLAYER_PED_ID), false)) ReturnToOriginal(true);
}

// ---------------------------------------------------------------------------------------
// Vanilla Unicorn doors. Model names and positions: the four strip-club doors used by
// community door-lock configs (front, back, office, dressing room), plus other doors of the
// office/dressing-room models found by probing inside the club.
//
// UNLOCK records each door's original state the first time it is changed: its door-system
// state (DOOR_SYSTEM_GET_DOOR_STATE, when the game has registered the door) and its
// closest-door lock flag (GET_STATE_OF_CLOSEST_DOOR_OF_TYPE, when the door object is loaded).
// LOCK writes those recorded states back, so the game's original door behaviour returns.
struct Door { uint32_t model; float x, y, z; const char *name; };
static const Door kDoors[] = {
  {Joaat("prop_strip_door_01"), 127.9552f, -1298.503f, 29.41962f, "front entrance"},
  {Joaat("prop_magenta_door"), 96.09197f, -1284.854f, 29.43878f, "back entrance"},
  {Joaat("v_ilev_roc_door2"), 99.08321f, -1293.701f, 29.41868f, "office"},
  {Joaat("v_ilev_door_orangesolid"), 113.9822f, -1297.43f, 29.41868f, "dressing room"},
};
static const uint32_t kInteriorModels[] = {Joaat("v_ilev_roc_door2"), Joaat("v_ilev_door_orangesolid")};
static const Vec3 kClub = {112.0f, -1290.0f, 29.3f};

struct DoorRec {
  uint32_t model; Vec3 at;
  bool sysRecorded; uint32_t sysHash; int sysState;   // door system
  bool lockRecorded; bool wasLocked;                   // closest door of type
};
static DoorRec g_doors[20]; static int g_doorCount; static bool g_probed;
static void InitDoors() {
  g_doorCount = 0;
  for (const Door &d : kDoors) g_doors[g_doorCount++] = DoorRec{d.model, Vec3{d.x, d.y, d.z}, false, 0, 0, false, false};
}

static void UnlockDoor(DoorRec &d) {
  UINT64 doorHash = 0;
  bool registered = CallB(N::DOOR_SYSTEM_FIND_EXISTING_DOOR, d.at.x, d.at.y, d.at.z, d.model, (const void *)&doorHash) && (uint32_t)doorHash;
  if (registered && !d.sysRecorded) { d.sysHash = (uint32_t)doorHash; d.sysState = CallI(N::DOOR_SYSTEM_GET_DOOR_STATE, d.sysHash); d.sysRecorded = true; }
  if (!d.lockRecorded && CallI(N::GET_CLOSEST_OBJECT_OF_TYPE, d.at.x, d.at.y, d.at.z, 2.0f, d.model, false, false, false)) {
    UINT64 locked = 0, heading = 0;
    Call(N::GET_STATE_OF_CLOSEST_DOOR_OF_TYPE, d.model, d.at.x, d.at.y, d.at.z, (const void *)&locked, (const void *)&heading);
    d.wasLocked = (uint32_t)locked != 0; d.lockRecorded = true;
  }
  if (registered) Call(N::DOOR_SYSTEM_SET_DOOR_STATE, (uint32_t)doorHash, 0, false, true);  // 0 = unlocked
  Call(N::SET_STATE_OF_CLOSEST_DOOR_OF_TYPE, d.model, d.at.x, d.at.y, d.at.z, false, 0.0f, false);
}
static int RestoreDoors() {
  int restored = 0;
  for (int i = 0; i < g_doorCount; i++) {
    DoorRec &d = g_doors[i];
    if (d.sysRecorded) { Call(N::DOOR_SYSTEM_SET_DOOR_STATE, d.sysHash, d.sysState, false, true); d.sysRecorded = false; restored++; }
    if (d.lockRecorded) { Call(N::SET_STATE_OF_CLOSEST_DOOR_OF_TYPE, d.model, d.at.x, d.at.y, d.at.z, d.wasLocked, 0.0f, false); d.lockRecorded = false; restored++; }
  }
  return restored;
}

static void ProbeExtraDoors() {
  g_probed = true;
  for (float x = 95.0f; x <= 130.0f; x += 5.0f)
    for (float y = -1305.0f; y <= -1280.0f; y += 5.0f)
      for (uint32_t model : kInteriorModels) {
        int obj = CallI(N::GET_CLOSEST_OBJECT_OF_TYPE, x, y, 29.4f, 4.0f, model, false, false, false);
        if (!obj || !CallB(N::DOES_ENTITY_EXIST, obj)) continue;
        Vec3 at = Coords(obj);
        bool known = false;
        for (int i = 0; i < g_doorCount; i++) if (Key(Dist2(at, g_doors[i].at)) < Key(1.0f)) known = true;
        if (!known && g_doorCount < 20) g_doors[g_doorCount++] = DoorRec{model, at, false, 0, 0, false, false};
      }
}

// Interior flags we changed, restored when you're outside the club.
static int g_interior; static bool g_interiorWasDisabled, g_interiorWasCapped, g_interiorChanged, g_interiorRestorePending;
static int g_nextDoorTick;
static void UnicornTick(int now) {
  if (now < g_nextDoorTick) return;
  g_nextDoorTick = now + 1000;
  Vec3 me = Coords(CallI(N::PLAYER_PED_ID));
  uint32_t d2 = Key(Dist2(me, kClub));
  if (!S.unicornDoors) {
    if (g_interiorRestorePending && d2 > Key(40.0f * 40.0f)) {
      if (g_interiorWasDisabled) Call(N::DISABLE_INTERIOR, g_interior, true);
      if (g_interiorWasCapped) Call(N::CAP_INTERIOR, g_interior, true);
      g_interiorRestorePending = false; g_interiorChanged = false;
    }
    return;
  }
  if (d2 > Key(120.0f * 120.0f)) { g_probed = false; return; }  // only near the club
  if (S.unicornInterior) {
    int interior = CallI(N::GET_INTERIOR_AT_COORDS, kClub.x, kClub.y, kClub.z);
    if (interior) {
      bool disabled = CallB(N::IS_INTERIOR_DISABLED, interior), capped = CallB(N::IS_INTERIOR_CAPPED, interior);
      if ((disabled || capped) && !g_interiorChanged) { g_interior = interior; g_interiorWasDisabled = disabled; g_interiorWasCapped = capped; g_interiorChanged = true; }
      if (disabled) Call(N::DISABLE_INTERIOR, interior, false);
      if (capped) Call(N::CAP_INTERIOR, interior, false);
    }
  }
  if (!g_probed && d2 < Key(40.0f * 40.0f)) ProbeExtraDoors();  // interior streamed in
  for (int i = 0; i < g_doorCount; i++) UnlockDoor(g_doors[i]);
}

static void SetDoors(bool unlock) {
  S.unicornDoors = unlock; g_nextDoorTick = 0;
  if (unlock) { g_interiorRestorePending = false; Notify("PossessionV: Vanilla Unicorn doors UNLOCKED (kept open near the club)."); return; }
  int n = RestoreDoors();
  if (g_interiorChanged) g_interiorRestorePending = true;
  Notify(n ? "PossessionV: Vanilla Unicorn doors LOCKED: original door states restored."
           : "PossessionV: Vanilla Unicorn doors left to the game (nothing had been changed yet).");
}

// ---------------------------------------------------------------------------------------
// Unlocks: weapons, money, wanted level in restricted areas, time of day.
static const char *const kWeapons[] = {
  "WEAPON_KNIFE", "WEAPON_NIGHTSTICK", "WEAPON_HAMMER", "WEAPON_BAT", "WEAPON_GOLFCLUB", "WEAPON_CROWBAR",
  "WEAPON_BOTTLE", "WEAPON_DAGGER", "WEAPON_HATCHET", "WEAPON_KNUCKLE", "WEAPON_MACHETE", "WEAPON_FLASHLIGHT",
  "WEAPON_SWITCHBLADE", "WEAPON_POOLCUE", "WEAPON_WRENCH", "WEAPON_BATTLEAXE", "WEAPON_STONE_HATCHET",
  "WEAPON_PISTOL", "WEAPON_PISTOL_MK2", "WEAPON_COMBATPISTOL", "WEAPON_APPISTOL", "WEAPON_PISTOL50",
  "WEAPON_SNSPISTOL", "WEAPON_SNSPISTOL_MK2", "WEAPON_HEAVYPISTOL", "WEAPON_VINTAGEPISTOL", "WEAPON_MARKSMANPISTOL",
  "WEAPON_REVOLVER", "WEAPON_REVOLVER_MK2", "WEAPON_DOUBLEACTION", "WEAPON_STUNGUN", "WEAPON_FLAREGUN",
  "WEAPON_CERAMICPISTOL", "WEAPON_NAVYREVOLVER", "WEAPON_GADGETPISTOL", "WEAPON_RAYPISTOL",
  "WEAPON_MICROSMG", "WEAPON_SMG", "WEAPON_SMG_MK2", "WEAPON_ASSAULTSMG", "WEAPON_COMBATPDW", "WEAPON_MACHINEPISTOL",
  "WEAPON_MINISMG", "WEAPON_RAYCARBINE", "WEAPON_MG", "WEAPON_COMBATMG", "WEAPON_COMBATMG_MK2", "WEAPON_GUSENBERG",
  "WEAPON_PUMPSHOTGUN", "WEAPON_PUMPSHOTGUN_MK2", "WEAPON_SAWNOFFSHOTGUN", "WEAPON_ASSAULTSHOTGUN",
  "WEAPON_BULLPUPSHOTGUN", "WEAPON_HEAVYSHOTGUN", "WEAPON_DBSHOTGUN", "WEAPON_AUTOSHOTGUN", "WEAPON_MUSKET",
  "WEAPON_COMBATSHOTGUN", "WEAPON_ASSAULTRIFLE", "WEAPON_ASSAULTRIFLE_MK2", "WEAPON_CARBINERIFLE",
  "WEAPON_CARBINERIFLE_MK2", "WEAPON_ADVANCEDRIFLE", "WEAPON_SPECIALCARBINE", "WEAPON_SPECIALCARBINE_MK2",
  "WEAPON_BULLPUPRIFLE", "WEAPON_BULLPUPRIFLE_MK2", "WEAPON_COMPACTRIFLE", "WEAPON_MILITARYRIFLE",
  "WEAPON_SNIPERRIFLE", "WEAPON_HEAVYSNIPER", "WEAPON_HEAVYSNIPER_MK2", "WEAPON_MARKSMANRIFLE",
  "WEAPON_MARKSMANRIFLE_MK2", "WEAPON_RPG", "WEAPON_GRENADELAUNCHER", "WEAPON_MINIGUN", "WEAPON_FIREWORK",
  "WEAPON_RAILGUN", "WEAPON_HOMINGLAUNCHER", "WEAPON_COMPACTLAUNCHER", "WEAPON_RAYMINIGUN",
  "WEAPON_GRENADE", "WEAPON_STICKYBOMB", "WEAPON_PROXMINE", "WEAPON_SMOKEGRENADE", "WEAPON_BZGAS",
  "WEAPON_MOLOTOV", "WEAPON_PIPEBOMB", "WEAPON_SNOWBALL", "WEAPON_FLARE", "WEAPON_BALL",
  "WEAPON_PETROLCAN", "WEAPON_FIREEXTINGUISHER", "GADGET_PARACHUTE",
};
static uint32_t JoaatRT(const char *s) { uint32_t h = 0; for (; *s; ++s) { char c = *s; if (c >= 'A' && c <= 'Z') c = (char)(c + 32); h += (unsigned char)c; h += h << 10; h ^= h >> 6; } h += h << 3; h ^= h >> 11; h += h << 15; return h; }

static void GiveAllWeapons(int ped, bool quiet) {
  int given = 0;
  for (const char *name : kWeapons) {
    uint32_t hash = JoaatRT(name);
    if (!CallB(N::IS_WEAPON_VALID, hash)) continue;  // not in this game version
    Call(N::GIVE_WEAPON_TO_PED, ped, hash, 9999, false, false);  // the game caps ammo at each weapon's maximum
    given++;
  }
  if (!quiet) Notify(given ? "PossessionV: all weapons given with full ammo." : "PossessionV: no weapons available in this game version.");
}

// Story cash lives in SP0/SP1/SP2_TOTAL_CASH (Michael, Franklin, Trevor). While possessing someone,
// the money goes to your original story character.
static void AddMoney() {
  int ped = g_original ? g_original : CallI(N::PLAYER_PED_ID);
  uint32_t model = (uint32_t)CallI(N::GET_ENTITY_MODEL, ped);
  uint32_t stat = model == Joaat("player_zero") ? Joaat("SP0_TOTAL_CASH") : model == Joaat("player_one") ? Joaat("SP1_TOTAL_CASH")
                : model == Joaat("player_two") ? Joaat("SP2_TOTAL_CASH") : 0;
  if (!stat) { Notify("PossessionV: money only works for Michael, Franklin or Trevor."); return; }
  UINT64 cash = 0;
  if (!CallB(N::STAT_GET_INT, stat, (const void *)&cash, -1)) { Notify("PossessionV: couldn't read this character's money."); return; }
  int64_t total = (int64_t)(int)(uint32_t)cash + S.moneyAmount;
  if (total > 2000000000) total = 2000000000;
  Call(N::STAT_SET_INT, stat, (int)total, true);
  Notify("PossessionV: money added.");
}

static bool SameText(const char *a, const char *b) { while (*a && *a == *b) { a++; b++; } return *a == *b; }
static bool InRestrictedArea() {
  Vec3 p = Coords(CallI(N::PLAYER_PED_ID));
  const char *zone = (const char *)*Call(N::GET_NAME_OF_ZONE, p.x, p.y, p.z);  // engine string (guest address)
  if (!zone) return false;
  return SameText(zone, "ARMYB") || SameText(zone, "HUMLAB") || SameText(zone, "JAIL") || SameText(zone, "AIRP");
}

static bool g_maxWantedSet;
static void WantedTick() {
  int player = CallI(N::PLAYER_ID);
  if (S.neverWanted) {
    if (!g_maxWantedSet) { Call(N::SET_MAX_WANTED_LEVEL, 0); g_maxWantedSet = true; }
    if (CallI(N::GET_PLAYER_WANTED_LEVEL, player) > 0) Call(N::CLEAR_PLAYER_WANTED_LEVEL, player);
    return;
  }
  if (g_maxWantedSet) { Call(N::SET_MAX_WANTED_LEVEL, 5); g_maxWantedSet = false; }
  if (S.restrictedNoWanted && CallI(N::GET_PLAYER_WANTED_LEVEL, player) > 0 && InRestrictedArea()) Call(N::CLEAR_PLAYER_WANTED_LEVEL, player);
}

// ---------------------------------------------------------------------------------------
// Keys and menu.
static bool g_prev[256];
static bool Down(int vk) { return vk > 0 && vk < 256 && (GetAsyncKeyState(vk) & 0x8000) != 0; }
static bool Pressed(int vk) {  // edge: up -> down
  if (vk <= 0 || vk >= 256) return false;
  bool d = Down(vk), was = g_prev[vk]; g_prev[vk] = d; return d && !was;
}

enum { M_MODE, M_TARGET, M_NEXT, M_ANIMAL, M_RETURN, M_DOORS, M_WEAPONS, M_MONEY, M_RESTRICTED, M_NEVERWANTED, M_NIGHT, M_CLOSE, M_COUNT };
static bool g_menu; static int g_sel;

static void ToggleMode() {
  S.possessionEnabled = !S.possessionEnabled; g_selected = 0; g_nextTargetRefresh = 0;
  Notify(S.possessionEnabled ? "PossessionV: pedestrian possession ON." : "PossessionV: pedestrian possession OFF (you can still return to your original).");
}

static void MenuAction(int item) {
  switch (item) {
    case M_MODE: ToggleMode(); break;
    case M_TARGET: PossessTarget(); break;
    case M_NEXT: NextTarget(); break;
    case M_ANIMAL: Possess(KIND_ANIMAL); break;
    case M_RETURN: ReturnToOriginal(false); break;
    case M_DOORS: SetDoors(!S.unicornDoors); break;
    case M_WEAPONS: GiveAllWeapons(CallI(N::PLAYER_PED_ID), false); break;
    case M_MONEY: AddMoney(); break;
    case M_RESTRICTED:
      S.restrictedNoWanted = !S.restrictedNoWanted;
      Notify(S.restrictedNoWanted ? "PossessionV: no wanted level in restricted areas." : "PossessionV: restricted areas back to normal.");
      break;
    case M_NEVERWANTED:
      S.neverWanted = !S.neverWanted;
      Notify(S.neverWanted ? "PossessionV: never wanted ON." : "PossessionV: never wanted OFF.");
      break;
    case M_NIGHT: Call(N::SET_CLOCK_TIME, 22, 0, 0); Notify("PossessionV: time set to 22:00."); break;
    case M_CLOSE: g_menu = false; break;
  }
}

static void DrawMenu() {
  // Left side, large rows for touch play; the port shows its D-pad while a menu is drawn.
  const float x = 0.17f, w = 0.30f, rowH = 0.045f, top = 0.12f;
  Rect(x, top - rowH, w, rowH, 120, 20, 120, 230);
  Text("PossessionV", x - w / 2 + 0.01f, top - rowH - 0.022f, 0.55f, 255, 255, 255);
  const char *labels[M_COUNT] = {
    S.possessionEnabled ? "Pedestrian Possession: ON" : "Pedestrian Possession: OFF",
    "Possess target (marked ped)", "Next target", "Possess nearest animal",
    g_original ? "Return to Original Player" : "Return to Original Player (not possessing)",
    S.unicornDoors ? "Vanilla Unicorn Doors: UNLOCKED" : "Vanilla Unicorn Doors: LOCKED (game)",
    "Give all weapons + ammo", "Add money (story character)",
    S.restrictedNoWanted ? "Restricted areas: no wanted ON" : "Restricted areas: no wanted OFF",
    S.neverWanted ? "Never wanted: ON" : "Never wanted: OFF",
    "Set time to night (22:00)", "Close Menu"};
  for (int i = 0; i < M_COUNT; i++) {
    float y = top + rowH * (float)i;
    bool sel = i == g_sel;
    Rect(x, y, w, rowH - 0.004f, sel ? 255 : 0, sel ? 255 : 0, sel ? 255 : 0, sel ? 220 : 170);
    int c = sel ? 0 : 255;
    Text(labels[i], x - w / 2 + 0.01f, y - 0.017f, 0.40f, c, c, c);
  }
  Text("Up/Down move  -  Enter select  -  Back close", x - w / 2 + 0.01f, top + rowH * M_COUNT - 0.015f, 0.30f, 230, 230, 230);
}

static void MenuTick() {
  // Navigation keys are only polled while the menu is open, so the port's touch D-pad maps
  // to them and they don't clutter the Mods key tray otherwise.
  // Every key is polled each frame (no short-circuit), so edge state stays current.
  int up = (int)Pressed(0x26) + (int)Pressed(0x68), down = (int)Pressed(0x28) + (int)Pressed(0x62);
  int select = (int)Pressed(0x0D) + (int)Pressed(0x65), back = (int)Pressed(0x08) + (int)Pressed(0x60) + (int)Pressed(0x1B);
  if (up) g_sel = (g_sel + M_COUNT - 1) % M_COUNT;
  if (down) g_sel = (g_sel + 1) % M_COUNT;
  if (back) { g_menu = false; return; }
  if (select) MenuAction(g_sel);
  if (g_menu) DrawMenu();
}

// ---------------------------------------------------------------------------------------
static void ScriptMain() {
  LoadSettings();
  InitDoors();
  Notify("PossessionV loaded. Open the menu from the Mods keys (F7).");
  for (;;) {
    int now = CallI(N::GET_GAME_TIMER);
    if (Pressed(S.menuKey)) { g_menu = !g_menu; g_sel = 0; }
    if (Pressed(S.possessPedKey)) PossessTarget();
    if (Pressed(S.possessAnimalKey)) Possess(KIND_ANIMAL);
    if (Pressed(S.returnKey)) ReturnToOriginal(false);
    if (Pressed(S.nextTargetKey)) NextTarget();
    if (Pressed(S.doorsKey)) SetDoors(!S.unicornDoors);
    if (Pressed(S.modeKey)) ToggleMode();
    if (g_menu) MenuTick();
    PossessionTick();
    TargetTick(now);
    WantedTick();
    UnicornTick(now);
    scriptWait(0);
  }
}

extern "C" BOOL __stdcall DllMain(HINSTANCE instance, DWORD reason, void *) {
  if (reason == 1) {  // DLL_PROCESS_ATTACH
    g_module = instance;
    scriptRegister(instance, ScriptMain);
  } else if (reason == 0) {  // DLL_PROCESS_DETACH
    scriptUnregister(instance);
  }
  return 1;
}
