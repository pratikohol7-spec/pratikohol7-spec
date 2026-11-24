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
  int maxDistance = 60;       // metres
  bool protectOriginal = true;
  bool autoReturnOnDeath = true;
  bool unicornDoors = true;
  bool unicornInterior = true;
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
  S.maxDistance = (int)GetPrivateProfileIntA("Possession", "MaxDistance", S.maxDistance, g_ini);
  S.protectOriginal = GetPrivateProfileIntA("Possession", "ProtectOriginal", 1, g_ini) != 0;
  S.autoReturnOnDeath = GetPrivateProfileIntA("Possession", "AutoReturnOnDeath", 1, g_ini) != 0;
  S.unicornDoors = GetPrivateProfileIntA("VanillaUnicorn", "UnlockDoors", 1, g_ini) != 0;
  S.unicornInterior = GetPrivateProfileIntA("VanillaUnicorn", "EnableInterior", 1, g_ini) != 0;
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
// Possession.
enum Kind { KIND_HUMAN, KIND_ANIMAL, KIND_ANY };
static const int PED_TYPE_ANIMAL = 28;

static int g_original;      // the character we started as (0: not possessing)
static int g_originalBlip;  // blip on the original while away
static bool g_originalLostTold;

static bool Alive(int ped) { return ped && CallB(N::DOES_ENTITY_EXIST, ped) && !CallB(N::IS_ENTITY_DEAD, ped, false); }
static bool IsAnimal(int ped) { return CallI(N::GET_PED_TYPE, ped) == PED_TYPE_ANIMAL || !CallB(N::IS_PED_HUMAN, ped); }

static int FindNearest(Kind kind) {
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
  uint32_t best = 0xFFFFFFFFu;
  int found = 0;
  for (int i = 0; i < count; i++) {
    int ped = (int)(uint32_t)list[i + 1];
    if (!ped || ped == me || ped == g_original) continue;
    if (!Alive(ped) || CallB(N::IS_PED_A_PLAYER, ped)) continue;
    bool animal = IsAnimal(ped);
    if ((kind == KIND_HUMAN && animal) || (kind == KIND_ANIMAL && !animal)) continue;
    uint32_t d = Key(Dist2(at, Coords(ped)));
    if (d <= maxKey && d < best) { best = d; found = ped; }
  }
  return found;
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

static void Possess(Kind kind) {
  int me = CallI(N::PLAYER_PED_ID);
  if (!Alive(me)) { Notify("PossessionV: you can't possess anyone while dead."); return; }
  int target = FindNearest(kind);
  if (!target) {
    Notify(kind == KIND_ANIMAL ? "PossessionV: no animal nearby." : kind == KIND_HUMAN ? "PossessionV: no pedestrian nearby." : "PossessionV: nobody nearby.");
    return;
  }
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
  Notify(IsAnimal(target) ? "PossessionV: animal possessed." : "PossessionV: pedestrian possessed.");
}

static void ReturnToOriginal(bool quiet) {
  if (!g_original) { if (!quiet) Notify("PossessionV: you are already your original character."); return; }
  if (!Alive(g_original)) {
    if (!quiet) Notify("PossessionV: your original character is gone; you stay in this body.");
    RemoveOriginalBlip(); g_original = 0; return;
  }
  int body = CallI(N::PLAYER_PED_ID);
  Call(N::CHANGE_PLAYER_PED, CallI(N::PLAYER_ID), g_original, false, false);
  if (CallI(N::PLAYER_PED_ID) != g_original) { Notify("PossessionV: the game refused to switch back."); return; }
  Call(N::SET_ENTITY_INVINCIBLE, g_original, false);
  Call(N::SET_BLOCKING_OF_NON_TEMPORARY_EVENTS, g_original, false);
  Call(N::CLEAR_PED_TASKS, g_original);
  RemoveOriginalBlip();
  if (body != g_original) ReleasePed(body);
  g_original = 0;
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
// community door-lock configs (front, back, office, dressing room).
struct Door { uint32_t model; float x, y, z; const char *name; };
static const Door kDoors[] = {
  {Joaat("prop_strip_door_01"), 127.9552f, -1298.503f, 29.41962f, "front entrance"},
  {Joaat("prop_magenta_door"), 96.09197f, -1284.854f, 29.43878f, "back entrance"},
  {Joaat("v_ilev_roc_door2"), 99.08321f, -1293.701f, 29.41868f, "office"},
  {Joaat("v_ilev_door_orangesolid"), 113.9822f, -1297.43f, 29.41868f, "dressing room"},
};
static const uint32_t kInteriorModels[] = {Joaat("v_ilev_roc_door2"), Joaat("v_ilev_door_orangesolid")};
static const Vec3 kClub = {112.0f, -1290.0f, 29.3f};

// Extra internal doors of the same models found by probing the club (other dressing rooms).
struct Found { uint32_t model; Vec3 at; };
static Found g_extra[16]; static int g_extraCount; static bool g_probed;

static void UnlockAt(uint32_t model, float x, float y, float z) {
  UINT64 doorHash = 0;
  if (CallB(N::DOOR_SYSTEM_FIND_EXISTING_DOOR, x, y, z, model, (const void *)&doorHash) && (uint32_t)doorHash)
    Call(N::DOOR_SYSTEM_SET_DOOR_STATE, (uint32_t)doorHash, 0, false, true);  // 0 = unlocked
  Call(N::SET_STATE_OF_CLOSEST_DOOR_OF_TYPE, model, x, y, z, false, 0.0f, false);
}

static void ProbeExtraDoors() {
  g_probed = true; g_extraCount = 0;
  for (float x = 95.0f; x <= 130.0f; x += 5.0f)
    for (float y = -1305.0f; y <= -1280.0f; y += 5.0f)
      for (uint32_t model : kInteriorModels) {
        int obj = CallI(N::GET_CLOSEST_OBJECT_OF_TYPE, x, y, 29.4f, 4.0f, model, false, false, false);
        if (!obj || !CallB(N::DOES_ENTITY_EXIST, obj)) continue;
        Vec3 at = Coords(obj);
        bool known = false;
        for (const Door &d : kDoors) if (Key(Dist2(at, Vec3{d.x, d.y, d.z})) < Key(1.0f)) known = true;
        for (int i = 0; i < g_extraCount; i++) if (Key(Dist2(at, g_extra[i].at)) < Key(1.0f)) known = true;
        if (!known && g_extraCount < 16) g_extra[g_extraCount++] = Found{model, at};
      }
}

static int g_nextDoorTick;
static void UnicornTick(int now) {
  if (!S.unicornDoors || now < g_nextDoorTick) return;
  g_nextDoorTick = now + 1000;
  Vec3 me = Coords(CallI(N::PLAYER_PED_ID));
  if (Key(Dist2(me, kClub)) > Key(120.0f * 120.0f)) { g_probed = false; return; }  // only near the club
  if (S.unicornInterior) {
    int interior = CallI(N::GET_INTERIOR_AT_COORDS, kClub.x, kClub.y, kClub.z);
    if (interior) {
      if (CallB(N::IS_INTERIOR_DISABLED, interior)) Call(N::DISABLE_INTERIOR, interior, false);
      if (CallB(N::IS_INTERIOR_CAPPED, interior)) Call(N::CAP_INTERIOR, interior, false);
    }
  }
  for (const Door &d : kDoors) UnlockAt(d.model, d.x, d.y, d.z);
  if (!g_probed && Key(Dist2(me, kClub)) < Key(40.0f * 40.0f)) ProbeExtraDoors();  // interior streamed in
  for (int i = 0; i < g_extraCount; i++) UnlockAt(g_extra[i].model, g_extra[i].at.x, g_extra[i].at.y, g_extra[i].at.z);
}

// ---------------------------------------------------------------------------------------
// Keys and menu.
static bool g_prev[256];
static bool Down(int vk) { return vk > 0 && vk < 256 && (GetAsyncKeyState(vk) & 0x8000) != 0; }
static bool Pressed(int vk) {  // edge: up -> down
  if (vk <= 0 || vk >= 256) return false;
  bool d = Down(vk), was = g_prev[vk]; g_prev[vk] = d; return d && !was;
}

enum { M_PED, M_ANIMAL, M_ANY, M_RETURN, M_DOORS, M_CLOSE, M_COUNT };
static bool g_menu; static int g_sel;

static void MenuAction(int item) {
  switch (item) {
    case M_PED: Possess(KIND_HUMAN); break;
    case M_ANIMAL: Possess(KIND_ANIMAL); break;
    case M_ANY: Possess(KIND_ANY); break;
    case M_RETURN: ReturnToOriginal(false); break;
    case M_DOORS:
      S.unicornDoors = !S.unicornDoors; g_nextDoorTick = 0; g_probed = false;
      Notify(S.unicornDoors ? "PossessionV: Vanilla Unicorn doors kept unlocked." : "PossessionV: Vanilla Unicorn doors left to the game.");
      break;
    case M_CLOSE: g_menu = false; break;
  }
}

static void DrawMenu() {
  // Large rows for touch play; the port shows its D-pad while a menu is drawn.
  const float x = 0.17f, w = 0.30f, rowH = 0.055f, top = 0.20f;
  Rect(x, top - rowH, w, rowH, 120, 20, 120, 230);
  Text("PossessionV", x - w / 2 + 0.01f, top - rowH - 0.022f, 0.55f, 255, 255, 255);
  const char *labels[M_COUNT] = {
    "Possess nearest pedestrian", "Possess nearest animal", "Possess nearest (anyone)",
    g_original ? "Return to original character" : "Return (not possessing)",
    S.unicornDoors ? "Vanilla Unicorn doors: UNLOCKED" : "Vanilla Unicorn doors: game default",
    "Close menu"};
  for (int i = 0; i < M_COUNT; i++) {
    float y = top + rowH * (float)i;
    bool sel = i == g_sel;
    Rect(x, y, w, rowH - 0.004f, sel ? 255 : 0, sel ? 255 : 0, sel ? 255 : 0, sel ? 220 : 170);
    int c = sel ? 0 : 255;
    Text(labels[i], x - w / 2 + 0.01f, y - 0.018f, 0.42f, c, c, c);
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
  Notify("PossessionV loaded. Open the menu from the Mods keys (F7).");
  for (;;) {
    int now = CallI(N::GET_GAME_TIMER);
    if (Pressed(S.menuKey)) { g_menu = !g_menu; g_sel = 0; }
    if (Pressed(S.possessPedKey)) Possess(KIND_HUMAN);
    if (Pressed(S.possessAnimalKey)) Possess(KIND_ANIMAL);
    if (Pressed(S.returnKey)) ReturnToOriginal(false);
    if (g_menu) MenuTick();
    PossessionTick();
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
