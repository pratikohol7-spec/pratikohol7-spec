// SuperGarageV - Script Hook V .asi for the GTA V iOS port (and PC Script Hook V).
//
//  * Lists every super car (vehicle class 7) and super/sport bike (class 8) that this game
//    build actually contains: candidates are checked with IS_MODEL_IN_CDIMAGE and sorted by
//    the class the game itself reports (GET_VEHICLE_CLASS_FROM_NAME).
//  * Spawns the chosen vehicle in front of you and puts you in it.
//  * Engine sound: each vehicle keeps its own game sound by default, or you can pick another
//    game vehicle's engine sound (FORCE_USE_AUDIO_GAME_OBJECT), also for the car you're in.
//
// No C runtime; imports: six Script Hook V functions, GetModuleFileNameA,
// GetPrivateProfileIntA, GetAsyncKeyState. No float compares (see Key note in PossessionV).
#include "shv.h"

extern "C" {
int _fltused = 0;
void *memset(void *d, int c, unsigned long long n) { volatile unsigned char *p = (volatile unsigned char *)d; while (n--) *p++ = (unsigned char)c; return d; }
void *memcpy(void *d, const void *s, unsigned long long n) { volatile unsigned char *p = (volatile unsigned char *)d; const unsigned char *q = (const unsigned char *)s; while (n--) *p++ = *q++; return d; }
}

namespace N {
enum : UINT64 {
  PLAYER_PED_ID = 0xD80958FC74E988A6ull,
  DOES_ENTITY_EXIST = 0x7239B21A38F536BAull,
  IS_ENTITY_DEAD = 0x5F9532F3B5CC2551ull,
  REQUEST_MODEL = 0x963D27A58DF860ACull,
  HAS_MODEL_LOADED = 0x98A4EB5D89A0C952ull,
  IS_MODEL_IN_CDIMAGE = 0x35B9E0803292B641ull,
  IS_MODEL_A_VEHICLE = 0x19AAC8F07BFEC53Eull,
  GET_VEHICLE_CLASS_FROM_NAME = 0xDEDF1C8BD47C2200ull,
  CREATE_VEHICLE = 0xAF35D0D2583051B0ull,
  SET_PED_INTO_VEHICLE = 0xF75B0D629E1C063Dull,
  SET_MODEL_AS_NO_LONGER_NEEDED = 0xE532F5D78798DAABull,
  GET_ENTITY_HEADING = 0xE83D4F9BA2A38914ull,
  SET_VEHICLE_ON_GROUND_PROPERLY = 0x49733E92263139D1ull,
  SET_ENTITY_AS_NO_LONGER_NEEDED = 0xB736A491E64A32CFull,
  DELETE_VEHICLE = 0xEA386986E786A54Full,
  GET_DISPLAY_NAME_FROM_VEHICLE_MODEL = 0xB215AAC32D25D019ull,
  GET_FILENAME_FOR_AUDIO_CONVERSATION = 0x7B5280EBA9840C72ull,  // returns a text label's localized text
  DOES_TEXT_LABEL_EXIST = 0xAC09CA973C564252ull,
  GET_OFFSET_FROM_ENTITY_IN_WORLD_COORDS = 0x1899F328B0E12848ull,
  FORCE_USE_AUDIO_GAME_OBJECT = 0x4F0C413926060B38ull,
  GET_VEHICLE_PED_IS_IN = 0x9A9112A0FE9A4713ull,
  IS_PED_IN_ANY_VEHICLE = 0x997ABD671D25CA0Bull,
  SET_VEHICLE_ENGINE_ON = 0x2497C4717C8B881Eull,
  SET_VEHICLE_DIRT_LEVEL = 0x79D3B596FE44EE8Bull,
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
  DISPLAY_ONSCREEN_KEYBOARD = 0x00DC833F2568DBF6ull,
  UPDATE_ONSCREEN_KEYBOARD = 0x0CF2B696BBF945AEull,
  GET_ONSCREEN_KEYBOARD_RESULT = 0x8362B09B91893647ull,
};
}

static inline UINT64 A(int v) { return (UINT64)(int64_t)v; }
static inline UINT64 A(unsigned v) { return (UINT64)v; }
static inline UINT64 A(bool v) { return v ? 1 : 0; }
static inline UINT64 A(float v) { uint32_t b; memcpy(&b, &v, 4); return (UINT64)b; }
static inline UINT64 A(const void *p) { return (UINT64)p; }
static UINT64 *Call(UINT64 hash) { nativeInit(hash); return nativeCall(); }
template <typename... T> static UINT64 *Call(UINT64 hash, T... a) { nativeInit(hash); (nativePush64(A(a)), ...); return nativeCall(); }
template <typename... T> static int CallI(UINT64 hash, T... a) { return (int)(uint32_t)*Call(hash, a...); }
template <typename... T> static bool CallB(UINT64 hash, T... a) { return ((uint32_t)*Call(hash, a...)) != 0; }
template <typename... T> static float CallF(UINT64 hash, T... a) { float f; memcpy(&f, Call(hash, a...), 4); return f; }
template <typename... T> static const char *CallS(UINT64 hash, T... a) { return (const char *)*Call(hash, a...); }

static uint32_t Joaat(const char *s) {
  uint32_t h = 0;
  for (; *s; ++s) { char c = *s; if (c >= 'A' && c <= 'Z') c = (char)(c + 32); h += (unsigned char)c; h += h << 10; h ^= h >> 6; }
  h += h << 3; h ^= h >> 11; h += h << 15; return h;
}
static bool SameText(const char *a, const char *b) { while (*a && *a == *b) { a++; b++; } return *a == *b; }

// ---------------------------------------------------------------------------------------
// Candidate models. The game decides: models it doesn't have are dropped, and each one is
// filed under the class the game reports (7 = Super, 8 = Motorcycles), so a wrong guess here
// can't put a vehicle in the wrong list.
static const char *const kCandidates[] = {
  // super cars
  "adder", "autarch", "banshee2", "bullet", "cheetah", "cyclone", "deveste", "emerus", "entityxf", "entity2",
  "fmj", "furia", "gp1", "infernus", "italigtb", "italigtb2", "krieger", "le7b", "nero", "nero2", "osiris",
  "penetrator", "pfister811", "prototipo", "reaper", "s80", "sc1", "sheava", "sultanrs", "t20", "taipan",
  "tempesta", "tezeract", "thrax", "tigon", "turismor", "tyrant", "tyrus", "vacca", "vagner", "visione",
  "voltic", "xa21", "zentorno", "zorrusso", "champion", "ignus", "zeno", "torero2", "entity3", "lm87",
  "virtue", "pipistrello", "furia2",
  // super / sport bikes
  "akuma", "bati", "bati2", "carbonrs", "double", "hakuchou", "hakuchou2", "lectro", "nemesis", "ruffian",
  "vader", "vortex", "defiler", "shotaro", "diablous2", "fcr", "fcr2", "pcj", "bf400", "thrust", "reever",
  "shinobi", "powersurge", "sanchez", "sanchez2", "manchez", "esskey", "vindicator",
};
static const int kCount = (int)(sizeof kCandidates / sizeof *kCandidates);

// Real-world picks: the game's own vehicle whose design is widely known to be based on the
// real one. Only well-known pairings; real vehicles with no clear in-game counterpart
// (Kawasaki Ninja H2/H2R, ZX-10R, Z900, BMW M4 and BMW bikes, Royal Enfield Bullet,
// Benelli) need an add-on model, which the Add-on page and Spawn by name handle.
struct Pick { const char *label; const char *model; };
static const Pick kPicks[] = {
  {"Bugatti Chiron style", "nero"}, {"Bugatti Chiron style (custom)", "nero2"}, {"Bugatti Veyron style", "adder"},
  {"Pagani Huayra style", "osiris"},
  {"Koenigsegg CCX style", "entityxf"}, {"Koenigsegg Agera style", "entity2"},
  {"McLaren P1 style", "t20"}, {"McLaren Senna style", "emerus"}, {"McLaren F1 style", "gp1"},
  {"Aston Martin Vanquish style", "massacro"}, {"Aston Martin One-77 style", "seven70"},
  {"Bentley Continental GT style", "paragon"},
  {"Dodge Challenger style", "gauntlet"}, {"Dodge Challenger Hellcat style", "gauntlet4"},
  {"Suzuki Hayabusa style", "hakuchou"}, {"Suzuki Hayabusa drag style", "hakuchou2"},
  {"Ducati superbike style", "bati"},
};
static const int kPickCount = (int)(sizeof kPicks / sizeof *kPicks);

static const char *g_cars[128], *g_bikes[128]; static int g_carCount, g_bikeCount;
static short g_picks[64]; static int g_pickCount;
static char g_addonNames[32][32]; static const char *g_addons[32]; static int g_addonCount;
static bool g_scanned;
static char g_ini[300];

static bool ModelAvailable(const char *model) {
  uint32_t h = Joaat(model);
  return CallB(N::IS_MODEL_IN_CDIMAGE, h) && CallB(N::IS_MODEL_A_VEHICLE, h);
}
static void Scan() {
  g_scanned = true; g_carCount = g_bikeCount = g_pickCount = g_addonCount = 0;
  for (int i = 0; i < kCount; i++) {
    if (!ModelAvailable(kCandidates[i])) continue;
    int cls = CallI(N::GET_VEHICLE_CLASS_FROM_NAME, Joaat(kCandidates[i]));
    if (cls == 7 && g_carCount < 128) g_cars[g_carCount++] = kCandidates[i];
    else if (cls == 8 && g_bikeCount < 128) g_bikes[g_bikeCount++] = kCandidates[i];
  }
  for (int i = 0; i < kPickCount; i++) if (ModelAvailable(kPicks[i].model) && g_pickCount < 64) g_picks[g_pickCount++] = (short)i;
  // [AddOns] Vehicle1=...Vehicle32=: spawn names of add-on vehicles you installed yourself.
  char key[16] = "Vehicle";
  for (int n = 1; n <= 32; n++) {
    int k = 7; if (n >= 10) key[k++] = (char)('0' + n / 10); key[k++] = (char)('0' + n % 10); key[k] = 0;
    char *out = g_addonNames[g_addonCount < 32 ? g_addonCount : 31];
    DWORD len = GetPrivateProfileStringA("AddOns", key, "", out, 32, g_ini);
    if (len == 0 || !out[0] || g_addonCount >= 32) continue;
    if (ModelAvailable(out)) { g_addons[g_addonCount] = out; g_addonCount++; }
  }
}

// The in-game name ("Truffade Nero" shows as "Nero"), falling back to the model name.
static const char *DisplayName(const char *model) {
  const char *label = CallS(N::GET_DISPLAY_NAME_FROM_VEHICLE_MODEL, Joaat(model));
  if (label && *label && !SameText(label, "CARNOTFOUND") && CallB(N::DOES_TEXT_LABEL_EXIST, label)) {
    const char *text = CallS(N::GET_FILENAME_FOR_AUDIO_CONVERSATION, label);
    if (text && *text && !SameText(text, "NULL")) return text;
  }
  return model;
}

// ---------------------------------------------------------------------------------------
// Settings.
static HMODULE g_module;
static int g_menuKey = 117;  // F6
static bool g_warp = true, g_deletePrevious = true;
static void LoadSettings() {
  DWORD n = GetModuleFileNameA(g_module, g_ini, sizeof g_ini - 5);
  if (n == 0 || n >= sizeof g_ini - 5) { const char *d = "SuperGarageV.ini"; int i = 0; for (; d[i]; i++) g_ini[i] = d[i]; g_ini[i] = 0; }
  else {
    int dot = -1; for (int i = (int)n - 1; i >= 0 && g_ini[i] != '\\' && g_ini[i] != '/'; i--) if (g_ini[i] == '.') { dot = i; break; }
    int at = dot >= 0 ? dot : (int)n; g_ini[at] = '.'; g_ini[at + 1] = 'i'; g_ini[at + 2] = 'n'; g_ini[at + 3] = 'i'; g_ini[at + 4] = 0;
  }
  g_menuKey = (int)GetPrivateProfileIntA("Keys", "MenuKey", g_menuKey, g_ini);
  g_warp = GetPrivateProfileIntA("Spawn", "PutMeInVehicle", 1, g_ini) != 0;
  g_deletePrevious = GetPrivateProfileIntA("Spawn", "DeletePreviousSpawn", 1, g_ini) != 0;
}

static void Notify(const char *text) {
  Call(N::BEGIN_TEXT_COMMAND_THEFEED_POST, "STRING");
  Call(N::ADD_TEXT_COMPONENT_SUBSTRING_PLAYER_NAME, text);
  Call(N::END_TEXT_COMMAND_THEFEED_POST_TICKER, false, false);
}
static void Text(const char *text, float x, float y, float scale, int c) {
  Call(N::SET_TEXT_FONT, 0); Call(N::SET_TEXT_SCALE, 0.0f, scale); Call(N::SET_TEXT_COLOUR, c, c, c, 255); Call(N::SET_TEXT_OUTLINE);
  Call(N::BEGIN_TEXT_COMMAND_DISPLAY_TEXT, "STRING"); Call(N::ADD_TEXT_COMPONENT_SUBSTRING_PLAYER_NAME, text);
  Call(N::END_TEXT_COMMAND_DISPLAY_TEXT, x, y, 0);
}
static void Rect(float x, float y, float w, float h, int r, int g, int b, int a) { Call(N::DRAW_RECT, x, y, w, h, r, g, b, a, false); }

// ---------------------------------------------------------------------------------------
// Spawning and sound.
static int g_lastSpawn;
static const char *g_sound;  // 0: each vehicle's own sound; else a game vehicle's audio name

static void ApplySound(int vehicle) {
  if (g_sound && vehicle) Call(N::FORCE_USE_AUDIO_GAME_OBJECT, vehicle, g_sound);
}

static void Spawn(const char *name) {
  uint32_t model = Joaat(name);
  int me = CallI(N::PLAYER_PED_ID);
  if (!CallB(N::DOES_ENTITY_EXIST, me) || CallB(N::IS_ENTITY_DEAD, me, false)) return;
  if (!ModelAvailable(name)) { Notify("SuperGarageV: that vehicle isn't in this game's files."); return; }
  Call(N::REQUEST_MODEL, model);
  for (int tries = 0; !CallB(N::HAS_MODEL_LOADED, model); tries++) {
    if (tries >= 250) { Notify("SuperGarageV: the vehicle didn't load in time (game files streaming?)."); return; }
    scriptWait(20);
  }
  if (g_deletePrevious && g_lastSpawn && CallB(N::DOES_ENTITY_EXIST, g_lastSpawn)) {
    bool inIt = CallB(N::IS_PED_IN_ANY_VEHICLE, me, false) && CallI(N::GET_VEHICLE_PED_IS_IN, me, false) == g_lastSpawn;
    int old = g_lastSpawn;
    if (!inIt) Call(N::DELETE_VEHICLE, (const void *)&old);
    else Call(N::SET_ENTITY_AS_NO_LONGER_NEEDED, (const void *)&old);
  }
  UINT64 *r = Call(N::GET_OFFSET_FROM_ENTITY_IN_WORLD_COORDS, me, 0.0f, 5.0f, 0.0f);
  float x, y, z; memcpy(&x, &r[0], 4); memcpy(&y, &r[1], 4); memcpy(&z, &r[2], 4);
  float heading = CallF(N::GET_ENTITY_HEADING, me);
  int vehicle = CallI(N::CREATE_VEHICLE, model, x, y, z, heading, false, false, false);
  Call(N::SET_MODEL_AS_NO_LONGER_NEEDED, model);
  if (!vehicle) { Notify("SuperGarageV: the game couldn't create that vehicle."); return; }
  Call(N::SET_VEHICLE_ON_GROUND_PROPERLY, vehicle, 5.0f);
  Call(N::SET_VEHICLE_DIRT_LEVEL, vehicle, 0.0f);
  ApplySound(vehicle);
  if (g_warp) { Call(N::SET_PED_INTO_VEHICLE, me, vehicle, -1); Call(N::SET_VEHICLE_ENGINE_ON, vehicle, true, true, false); }
  g_lastSpawn = vehicle;
  Notify("SuperGarageV: vehicle spawned.");
}

static void SoundForCurrent() {
  int me = CallI(N::PLAYER_PED_ID);
  if (!CallB(N::IS_PED_IN_ANY_VEHICLE, me, false)) { Notify("SuperGarageV: get in a vehicle first."); return; }
  if (!g_sound) { Notify("SuperGarageV: pick an engine sound first (Own sound = the car's default)."); return; }
  ApplySound(CallI(N::GET_VEHICLE_PED_IS_IN, me, false));
  Notify("SuperGarageV: engine sound applied to your vehicle.");
}

// Spawn by name: the game's text box, which the iOS port answers with the iPhone keyboard.
static char g_typed[40];
static void SpawnByName() {
  Call(N::DISPLAY_ONSCREEN_KEYBOARD, 0, "FMMC_KEY_TIP8", "", "", "", "", "", 32);
  int state;
  while ((state = CallI(N::UPDATE_ONSCREEN_KEYBOARD)) == 0) scriptWait(0);
  if (state != 1) return;  // cancelled
  const char *text = CallS(N::GET_ONSCREEN_KEYBOARD_RESULT);
  int n = 0;
  for (; text && text[n] && n < (int)sizeof g_typed - 1; n++) g_typed[n] = text[n] == ' ' ? '_' : text[n];
  g_typed[n] = 0;
  if (!n) return;
  Spawn(g_typed);
}

// ---------------------------------------------------------------------------------------
// Menu pages.
enum Page { P_MAIN, P_PICKS, P_CARS, P_BIKES, P_ADDONS, P_SOUND };
enum { M_PICKS, M_CARS, M_BIKES, M_ADDONS, M_TYPE, M_SOUND, M_APPLY, M_WARP, M_DELETE, M_CLOSE, M_COUNT };
static bool g_open; static Page g_page; static int g_sel, g_mainSel;
static bool g_prev[256];
static bool Pressed(int vk) {
  if (vk <= 0 || vk >= 256) return false;
  bool d = (GetAsyncKeyState(vk) & 0x8000) != 0, was = g_prev[vk]; g_prev[vk] = d; return d && !was;
}
static int SoundRows() { return 1 + g_pickCount + g_carCount + g_bikeCount; }
static const char *SoundAt(int row) {  // 0: own sound
  if (row == 0) return 0; row--;
  if (row < g_pickCount) return kPicks[g_picks[row]].model; row -= g_pickCount;
  if (row < g_carCount) return g_cars[row]; row -= g_carCount;
  return g_bikes[row];
}
static int PageCount() {
  switch (g_page) {
    case P_PICKS: return g_pickCount; case P_CARS: return g_carCount; case P_BIKES: return g_bikeCount;
    case P_ADDONS: return g_addonCount; case P_SOUND: return SoundRows(); default: return M_COUNT;
  }
}

static char g_line[160];
static const char *Join(const char *a, const char *b) {
  int o = 0; for (; *a && o < 158; a++) g_line[o++] = *a; for (; *b && o < 158; b++) g_line[o++] = *b; g_line[o] = 0; return g_line;
}
static const char *Num(const char *label, int n) {
  char digits[12]; int k = 0; if (n == 0) digits[k++] = '0'; while (n > 0 && k < 11) { digits[k++] = (char)('0' + n % 10); n /= 10; }
  char tail[16]; int t = 0; tail[t++] = ' '; tail[t++] = '('; while (k) tail[t++] = digits[--k]; tail[t++] = ')'; tail[t] = 0;
  return Join(label, tail);
}
static const char *PickLabel(int i) {
  static char buf[160]; const char *a = kPicks[i].label, *b = DisplayName(kPicks[i].model); int o = 0;
  for (; *a && o < 120; a++) buf[o++] = *a; buf[o++] = ':'; buf[o++] = ' ';
  for (; *b && o < 158; b++) buf[o++] = *b; buf[o] = 0; return buf;
}

static void Draw() {
  const float x = 0.19f, w = 0.34f, rowH = 0.045f, top = 0.15f; const int rows = 12;
  const char *title = g_page == P_PICKS ? "SuperGarageV - Real-world picks" : g_page == P_CARS ? "SuperGarageV - Super cars"
                    : g_page == P_BIKES ? "SuperGarageV - Super bikes" : g_page == P_ADDONS ? "SuperGarageV - Add-on vehicles"
                    : g_page == P_SOUND ? "SuperGarageV - Engine sound" : "SuperGarageV";
  Rect(x, top - rowH, w, rowH, 20, 60, 160, 230);
  Text(title, x - w / 2 + 0.01f, top - rowH - 0.02f, 0.5f, 255);
  int count = PageCount(), first = g_sel - rows / 2; if (first > count - rows) first = count - rows; if (first < 0) first = 0;
  for (int i = first, row = 0; i < count && row < rows; i++, row++) {
    const char *label;
    switch (g_page) {
      case P_MAIN:
        switch (i) {
          case M_PICKS: label = Num("Real-world picks", g_pickCount); break;
          case M_CARS: label = Num("Super cars", g_carCount); break;
          case M_BIKES: label = Num("Super bikes", g_bikeCount); break;
          case M_ADDONS: label = Num("Add-on vehicles", g_addonCount); break;
          case M_TYPE: label = "Spawn by name (keyboard)"; break;
          case M_SOUND: label = Join("Engine sound: ", g_sound ? DisplayName(g_sound) : "each vehicle's own"); break;
          case M_APPLY: label = "Apply engine sound to my vehicle"; break;
          case M_WARP: label = g_warp ? "Put me in spawned vehicle: ON" : "Put me in spawned vehicle: OFF"; break;
          case M_DELETE: label = g_deletePrevious ? "Delete previous spawn: ON" : "Delete previous spawn: OFF"; break;
          default: label = "Close menu"; break;
        }
        break;
      case P_PICKS: label = PickLabel(g_picks[i]); break;
      case P_CARS: label = DisplayName(g_cars[i]); break;
      case P_BIKES: label = DisplayName(g_bikes[i]); break;
      case P_ADDONS: label = Join(g_addons[i], ""); break;
      default: { const char *s = SoundAt(i); label = s ? DisplayName(s) : "Own sound (default)"; } break;
    }
    float y = top + rowH * (float)row; bool sel = i == g_sel;
    Rect(x, y, w, rowH - 0.004f, sel ? 255 : 0, sel ? 255 : 0, sel ? 255 : 0, sel ? 220 : 170);
    Text(label, x - w / 2 + 0.01f, y - 0.016f, 0.36f, sel ? 0 : 255);
  }
  int shown = count < rows ? count : rows;
  Text(g_page == P_MAIN ? "Up/Down move - Enter select - Back close" : "Up/Down move - Left/Right page - Enter select - Back",
       x - w / 2 + 0.01f, top + rowH * (float)shown - 0.014f, 0.28f, 230);
}

static void OpenPage(Page page, int count, const char *empty) {
  if (count) { g_page = page; g_sel = 0; } else Notify(empty);
}
static void Select() {
  switch (g_page) {
    case P_MAIN:
      g_mainSel = g_sel;
      switch (g_sel) {
        case M_PICKS: OpenPage(P_PICKS, g_pickCount, "SuperGarageV: none of the picks are in this game's files."); break;
        case M_CARS: OpenPage(P_CARS, g_carCount, "SuperGarageV: no super cars in this game's files."); break;
        case M_BIKES: OpenPage(P_BIKES, g_bikeCount, "SuperGarageV: no super bikes in this game's files."); break;
        case M_ADDONS: OpenPage(P_ADDONS, g_addonCount, "SuperGarageV: no add-on vehicles listed in SuperGarageV.ini (or not installed)."); break;
        case M_TYPE: SpawnByName(); break;
        case M_SOUND: g_page = P_SOUND; g_sel = 0; break;
        case M_APPLY: SoundForCurrent(); break;
        case M_WARP: g_warp = !g_warp; break;
        case M_DELETE: g_deletePrevious = !g_deletePrevious; break;
        default: g_open = false; break;
      }
      break;
    case P_PICKS: Spawn(kPicks[g_picks[g_sel]].model); break;
    case P_CARS: Spawn(g_cars[g_sel]); break;
    case P_BIKES: Spawn(g_bikes[g_sel]); break;
    case P_ADDONS: Spawn(g_addons[g_sel]); break;
    case P_SOUND:
      g_sound = SoundAt(g_sel);
      Notify(!g_sound ? "SuperGarageV: vehicles use their own engine sound." : "SuperGarageV: engine sound chosen; new spawns use it.");
      g_page = P_MAIN; g_sel = M_SOUND; break;
  }
}

static void MenuTick() {
  int up = (int)Pressed(0x26) + (int)Pressed(0x68), down = (int)Pressed(0x28) + (int)Pressed(0x62);
  int left = (int)Pressed(0x25) + (int)Pressed(0x64), right = (int)Pressed(0x27) + (int)Pressed(0x66);
  int select = (int)Pressed(0x0D) + (int)Pressed(0x65), back = (int)Pressed(0x08) + (int)Pressed(0x60) + (int)Pressed(0x1B);
  int count = PageCount();
  if (count > 0) {
    if (up) g_sel = (g_sel + count - 1) % count;
    if (down) g_sel = (g_sel + 1) % count;
    if (left && g_page != P_MAIN) { g_sel -= 10; if (g_sel < 0) g_sel = 0; }
    if (right && g_page != P_MAIN) { g_sel += 10; if (g_sel > count - 1) g_sel = count - 1; }
  }
  if (back) { if (g_page == P_MAIN) g_open = false; else { g_page = P_MAIN; g_sel = g_mainSel; } return; }
  if (select) Select();
  if (g_open) Draw();
}

static void ScriptMain() {
  LoadSettings();
  Notify("SuperGarageV loaded. Open it from the Mods keys (F6).");
  for (;;) {
    if (Pressed(g_menuKey)) {
      g_open = !g_open; g_page = P_MAIN; g_sel = 0;
      if (g_open && !g_scanned) Scan();
    }
    if (g_open) MenuTick();
    scriptWait(0);
  }
}

extern "C" BOOL __stdcall DllMain(HINSTANCE instance, DWORD reason, void *) {
  if (reason == 1) { g_module = instance; scriptRegister(instance, ScriptMain); }
  else if (reason == 0) scriptUnregister(instance);
  return 1;
}
