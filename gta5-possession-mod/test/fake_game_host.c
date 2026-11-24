// Runs PossessionV.asi inside the GTA V iOS port's own Script Hook V interpreter
// (app/Source/NativeASI.c + Blink x86-64 emulator) with a small simulated game world.
// Based on the port's Tests/asi_crt/asi_host.c, but game commands are answered by a fake
// world instead of "not in this game version", so the mod's logic can be checked end to end.
//
//   fake_game_host <PossessionV.asi> <mod folder>
// Exits 0 when every check passes.
#include "NativeASI.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

// ---- guest memory (same allocator as the port's asi_host.c) -------------------------------
static uint8_t *mem;
static const uint64_t memSize = 3ull << 30;
static uint64_t top = 0x100000, bins[48];
static int binOf(uint64_t s) { int b = 5; while ((1ull << b) < s + 16) b++; return b; }
static uint64_t hostAlloc(uint64_t size) {
  int b = binOf(size); uint64_t block;
  if (bins[b]) { block = bins[b]; memcpy(&bins[b], mem + block + 16, 8); }
  else { block = top; top += 1ull << b; if (top > memSize) return 0; }
  uint64_t bin = (uint64_t)b; memcpy(mem + block, &bin, 8); return block + 16;
}
static void hostFree(uint64_t a) { if (!a) return; uint64_t blk = a - 16, b; memcpy(&b, mem + blk, 8); memcpy(mem + blk + 16, &bins[b], 8); bins[b] = blk; }
static int logFaults;
static void hostLog(const char *line) {
  printf("[port] %s\n", line);
  if (strstr(line, "stopped") || strstr(line, "isn't in this game version") || strstr(line, "not available on iOS") || strstr(line, "not loaded")) logFaults++;
}
static uint32_t joaat(const char *s) { uint32_t h = 0; for (; *s; ++s) { char c = *s; if (c >= 'A' && c <= 'Z') c += 32; h += (unsigned char)c; h += h << 10; h ^= h >> 6; } h += h << 3; h ^= h >> 11; h += h << 15; return h; }

// ---- fake world ------------------------------------------------------------------------------
typedef struct { int handle; float x, y, z; int type; int human; int dead; int mission; int scriptOwned; int invincible; int blocking; int standing; int exists; } Ped;
static Ped peds[12]; static int pedCount;
static int playerPed = 1, frame;
static int blips[16], blipCount, nextBlip = 500;
static int notifications; static char pendingText[256], lastNote[256];
static int drawRects, markers; static float markerX;
static int changeCalls, wanted;
static int weaponsGiven, maxWanted = 5, clockHour = 12; static int64_t cash[3] = {500, 500, 500};
static uint64_t zoneStr;
// doors: the main four (front door is registered in the door system as 0xABCD, locked), two extra probe doors
static const float mainDoorX[4] = {127.9552f, 96.09197f, 99.08321f, 113.9822f}, mainDoorY[4] = {-1298.503f, -1284.854f, -1293.701f, -1297.43f};
static int doorSys = 1;          // 0xABCD door-system state: 1 = locked (game original)
static int doorLockedNow[6] = {1, 1, 1, 1, 1, 1};  // per door object: locked flag (game original: all locked)
static int interiorDisabled = 1, interiorCapped = 1;
static int doorIndexAt(float x, float y) {
  for (int i = 0; i < 4; i++) { float dx = x - mainDoorX[i], dy = y - mainDoorY[i]; if (dx * dx + dy * dy < 4.0f) return i; }
  if (y > -1301 && y < -1299) { if (x > 120.5f && x < 121.5f) return 4; if (x > 122.5f && x < 123.5f) return 5; }
  return -1;
}
static Ped *ped(int h) { for (int i = 0; i < pedCount; i++) if (peds[i].handle == h && peds[i].exists) return &peds[i]; return 0; }
static void addPed(int h, float x, float y, float z, int type, int human, int dead) {
  peds[pedCount++] = (Ped){h, x, y, z, type, human, dead, 0, 0, 0, 0, 0, 1};
}
static uint64_t arg(uint64_t args, int i) { uint64_t v; memcpy(&v, mem + args + 8 * i, 8); return v; }
static float argf(uint64_t args, int i) { float f; memcpy(&f, mem + args + 8 * i, 4); return f; }
static int argi(uint64_t args, int i) { return (int)(uint32_t)arg(args, i); }
static void ret(uint64_t r, uint64_t v) { memcpy(mem + r, &v, 8); }
static void retf3(uint64_t r, float x, float y, float z) { memset(mem + r, 0, 24); memcpy(mem + r, &x, 4); memcpy(mem + r + 8, &y, 4); memcpy(mem + r + 16, &z, 4); }
static const char *str(uint64_t a) { return a ? (const char *)mem + a : "(null)"; }
static int unknownHashes;

static bool hostNative(uint64_t hash, uint64_t a, uint32_t n, uint64_t r) {
  (void)n;
  switch (hash) {
    case 0x4F8644AF03D0E0D6ull: ret(r, 0); return true;                                   // PLAYER_ID
    case 0xD80958FC74E988A6ull: ret(r, (uint32_t)playerPed); return true;                 // PLAYER_PED_ID
    case 0x048189FAC643DEEEull: { changeCalls++; Ped *p = ped(argi(a, 1)); if (p && !p->dead) playerPed = p->handle; return true; }  // CHANGE_PLAYER_PED
    case 0x23F8F5FC7E8C4A6Bull: {                                                          // GET_PED_NEARBY_PEDS
      uint64_t arr = arg(a, 1), cap; memcpy(&cap, mem + arr, 8); int c = 0;
      for (int i = 0; i < pedCount && (uint64_t)c < cap; i++) if (peds[i].exists && peds[i].handle != argi(a, 0)) {
        uint64_t h = (uint32_t)peds[i].handle; memcpy(mem + arr + 8 + 8 * c, &h, 8); c++; }
      ret(r, (uint64_t)c); return true; }
    case 0xFF059E1E4C01E63Cull: { Ped *p = ped(argi(a, 0)); ret(r, p ? (uint32_t)p->type : 0); return true; }   // GET_PED_TYPE
    case 0xB980061DA992779Dull: { Ped *p = ped(argi(a, 0)); ret(r, p && p->human); return true; }             // IS_PED_HUMAN
    case 0x12534C348C6CB68Bull: ret(r, argi(a, 0) == playerPed); return true;                                // IS_PED_A_PLAYER
    case 0x524AC5ECEA15343Eull: ret(r, ped(argi(a, 0)) != 0); return true;                                   // IS_ENTITY_A_PED
    case 0x0A7B270912999B3Cull: { Ped *p = ped(argi(a, 0)); ret(r, p && (p->mission || p->scriptOwned)); return true; }  // IS_ENTITY_A_MISSION_ENTITY
    case 0xD839450756ED5A80ull: { Ped *p = ped(argi(a, 0)); ret(r, !p || p->dead); return true; }             // IS_PED_FATALLY_INJURED
    case 0x7239B21A38F536BAull: ret(r, ped(argi(a, 0)) != 0 || (argi(a, 0) >= 900 && argi(a, 0) < 920)); return true;  // DOES_ENTITY_EXIST
    case 0x5F9532F3B5CC2551ull: { Ped *p = ped(argi(a, 0)); ret(r, !p || p->dead); return true; }             // IS_ENTITY_DEAD
    case 0x3FEF770D40960D5Aull: { int h = argi(a, 0); Ped *p = ped(h);                                       // GET_ENTITY_COORDS
      if (p) retf3(r, p->x, p->y, p->z); else if (h == 904 || h == 905) retf3(r, h == 904 ? 121.0f : 123.0f, -1300.0f, 29.4f);
      else if (h >= 910 && h < 914) retf3(r, mainDoorX[h - 910], mainDoorY[h - 910], 29.42f); else retf3(r, 0, 0, 0); return true; }
    case 0xAD738C3085FE7E11ull: { Ped *p = ped(argi(a, 0)); if (p) p->mission = 1; return true; }            // SET_ENTITY_AS_MISSION_ENTITY
    case 0x2595DD4236549CE3ull: { int h; memcpy(&h, mem + arg(a, 0), 4); Ped *p = ped(h); if (p) p->mission = 0; return true; }  // SET_PED_AS_NO_LONGER_NEEDED (Ped*)
    case 0x3882114BDE571AD4ull: { Ped *p = ped(argi(a, 0)); if (p) p->invincible = argi(a, 1); return true; }
    case 0x919BE13EED931959ull: { Ped *p = ped(argi(a, 0)); if (p) p->standing = 1; return true; }
    case 0xE1EF3C1216AFF2CDull: { Ped *p = ped(argi(a, 0)); if (p) p->standing = 0; return true; }
    case 0x9F8AA94D6D97DBF4ull: { Ped *p = ped(argi(a, 0)); if (p) p->blocking = argi(a, 1); return true; }
    case 0x5CDE92C702A8FCE7ull: blips[blipCount++] = nextBlip; ret(r, (uint32_t)nextBlip++); return true;   // ADD_BLIP_FOR_ENTITY
    case 0x86A652570E5F25DDull: { int b; memcpy(&b, mem + arg(a, 0), 4); for (int i = 0; i < blipCount; i++) if (blips[i] == b) blips[i] = blips[--blipCount]; return true; }
    case 0xA6DB27D19ECBB7DAull: { int b = argi(a, 0), f = 0; for (int i = 0; i < blipCount; i++) if (blips[i] == b) f = 1; ret(r, f); return true; }
    case 0x03D7FB09E75D6B7Eull: return true;                                                                 // SET_BLIP_COLOUR
    case 0x28477EC23D892089ull: markers++; markerX = argf(a, 1); return true;                                // DRAW_MARKER
    case 0xE143FA2249364369ull: {                                                                           // GET_CLOSEST_OBJECT_OF_TYPE
      float x = argf(a, 0), y = argf(a, 1); int d = doorIndexAt(x, y);
      if (d >= 0 && d < 4) { ret(r, (uint32_t)(910 + d)); return true; }
      int h = 0; if (x > 117 && x < 127 && y > -1305 && y < -1295) h = x < 122 ? 904 : 905;                // two extra dressing-room doors
      ret(r, (uint32_t)h); return true; }
    case 0xEDC1A5B84AEF33FFull: { int d = doorIndexAt(argf(a, 1), argf(a, 2)); uint64_t l = d >= 0 ? (uint64_t)doorLockedNow[d] : 0;  // GET_STATE_OF_CLOSEST_DOOR_OF_TYPE
      memcpy(mem + arg(a, 4), &l, 8); return true; }
    case 0xF82D8F1926A02C3Dull: { int d = doorIndexAt(argf(a, 1), argf(a, 2)); if (d >= 0) doorLockedNow[d] = argi(a, 4); return true; }  // SET_STATE_OF_CLOSEST_DOOR_OF_TYPE
    case 0x589F80B325CC82C5ull: { float x = argf(a, 0); if (x > 127 && x < 129) { uint64_t h = 0xABCD; memcpy(mem + arg(a, 4), &h, 8); ret(r, 1); } else ret(r, 0); return true; }
    case 0x160AA1B32F6139B8ull: ret(r, argi(a, 0) == 0xABCD ? (uint32_t)doorSys : 0); return true;          // DOOR_SYSTEM_GET_DOOR_STATE
    case 0x6BAB9442830C7F53ull: if (argi(a, 0) == 0xABCD) doorSys = argi(a, 1); return true;                // DOOR_SYSTEM_SET_DOOR_STATE
    case 0xB0F7F8663821D9C3ull: ret(r, 77); return true;                                                     // GET_INTERIOR_AT_COORDS
    case 0xBC5115A5A939DD15ull: ret(r, interiorDisabled); return true;
    case 0x6170941419D7D8ECull: interiorDisabled = argi(a, 1); return true;
    case 0x92BAC8ACF88CEC26ull: ret(r, interiorCapped); return true;
    case 0xD9175F941610DB54ull: interiorCapped = argi(a, 1); return true;
    case 0x3A618A217E5154F0ull: drawRects++; return true;                                                    // DRAW_RECT
    case 0x66E0276CC5F6B9DAull: case 0x07C837F9A01C34C9ull: case 0xBE6B23FFA53FB442ull: case 0x2513DFB0FB8400FEull: return true;
    case 0x25FBB336DF1804CBull: case 0x202709F4C58A0424ull: if (strcmp(str(arg(a, 0)), "STRING")) unknownHashes++; return true;
    case 0x6C188BE134E074AAull: snprintf(pendingText, sizeof pendingText, "%s", str(arg(a, 0))); return true;
    case 0xCD015E5BB0D96A57ull: return true;
    case 0x2ED7843F8F801023ull: notifications++; snprintf(lastNote, sizeof lastNote, "%s", pendingText); printf("[frame %3d] notification: %s\n", frame, pendingText); return true;
    case 0x9CD27B0045628463ull: ret(r, (uint64_t)(frame * 100)); return true;                               // GET_GAME_TIMER (100 ms per frame)
    case 0x937C71165CF334B3ull: ret(r, (uint32_t)arg(a, 0) != joaat("WEAPON_RAYPISTOL")); return true;      // IS_WEAPON_VALID
    case 0xBF0FD6E56C964FCBull: if (argi(a, 0) == playerPed && argi(a, 2) == 9999) weaponsGiven++; return true;  // GIVE_WEAPON_TO_PED
    case 0x9F47B058362C84B5ull: ret(r, argi(a, 0) == 1 ? joaat("player_zero") : joaat("a_m_y_business_01")); return true;  // GET_ENTITY_MODEL
    case 0x767FBC2AC802EF3Dull: { uint32_t st = (uint32_t)arg(a, 0); int i = st == joaat("SP0_TOTAL_CASH") ? 0 : -1;
      if (i < 0) { ret(r, 0); return true; } int v = (int)cash[i]; memcpy(mem + arg(a, 1), &v, 4); ret(r, 1); return true; }  // STAT_GET_INT
    case 0xB3271D7AB655B441ull: { uint32_t st = (uint32_t)arg(a, 0); if (st == joaat("SP0_TOTAL_CASH")) cash[0] = argi(a, 1); return true; }
    case 0xCD90657D4C30E1CAull: { if (!zoneStr) zoneStr = hostAlloc(16); Ped *p = ped(playerPed);                  // GET_NAME_OF_ZONE
      strcpy((char *)mem + zoneStr, p && p->x < -1500 ? "ARMYB" : "STRIP"); ret(r, zoneStr); return true; }
    case 0xE28E54788CE8F12Dull: ret(r, (uint32_t)wanted); return true;
    case 0xB302540597885499ull: wanted = 0; return true;
    case 0xAA5F02DB48D704B9ull: maxWanted = argi(a, 0); return true;
    case 0x47C3B5848C3E45D8ull: clockHour = argi(a, 0); return true;
  }
  printf("[frame %3d] UNHANDLED native 0x%016llx\n", frame, (unsigned long long)hash); unknownHashes++;
  return false;
}
static int held;
static bool hostKey(int k) { return k == held; }
void PGMeshTouch(const void *s, uint64_t l) { (void)s; (void)l; }

static int failures;
#define CHECK(cond, what) do { if (cond) printf("  PASS  %s\n", what); else { printf("  FAIL  %s\n", what); failures++; } } while (0)
static void tick(int n) { for (int i = 0; i < n; i++) { pg_asi_tick(); frame++; } }
static void press(int vk) { held = vk; pg_asi_key(vk, true); tick(3); held = 0; pg_asi_key(vk, false); tick(2); }
static void menuTo(int row) { press(0x76); for (int i = 0; i < row; i++) press(0x28); press(0x0D); press(0x08); }  // F7, Down x row, Enter, Back

int main(int argc, char **argv) {
  if (argc < 3) { fprintf(stderr, "usage: fake_game_host <mod.asi> <folder>\n"); return 2; }
  mem = mmap(0, memSize, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE | MAP_NORESERVE, -1, 0);
  // Same host fields as the iOS app passes (EngineHost.c): no global pointer, no world_entities, game version 2189.
  PGAsiHost host = {mem, memSize, hostAlloc, hostFree, hostLog, hostNative, 0, hostKey, 0, 2189, argv[2]};
  if (!pg_asi_init(&host)) { puts("interpreter did not start"); return 1; }
  int fd = open(argv[1], O_RDONLY); struct stat st; if (fd < 0 || fstat(fd, &st)) { perror(argv[1]); return 1; }
  uint8_t *bytes = malloc((size_t)st.st_size); if (read(fd, bytes, (size_t)st.st_size) != st.st_size) return 1; close(fd);
  const char *name = strrchr(argv[1], '/'); name = name ? name + 1 : argv[1];

  // world: player (Michael) inside the Vanilla Unicorn
  addPed(1, 115.0f, -1290.0f, 29.4f, 0, 1, 0);   // original player character
  addPed(2, 119.0f, -1290.0f, 29.4f, 4, 1, 0);   // pedestrian, 4 m
  addPed(3, 123.0f, -1290.0f, 29.4f, 28, 0, 0);  // animal, 8 m
  addPed(4, 116.0f, -1290.0f, 29.4f, 4, 1, 1);   // dead pedestrian, 1 m (must be skipped)
  addPed(5, 215.0f, -1290.0f, 29.4f, 4, 1, 0);   // pedestrian, 100 m (out of range)
  addPed(6, 112.0f, -1290.0f, 29.4f, 5, 1, 0);   // pedestrian, 3 m (nearest possible)
  addPed(8, 114.0f, -1290.0f, 29.4f, 4, 1, 0); peds[pedCount - 1].scriptOwned = 1;  // mission character, 1 m (must be skipped)

  printf("== load\n");
  int id = pg_asi_load(name, argv[2], bytes, (uint64_t)st.st_size);
  CHECK(id > 0, "the port's loader accepts PossessionV.asi and runs DllMain");
  tick(15);
  CHECK(notifications >= 1, "script thread runs and posts its start-up notification");

  printf("== Vanilla Unicorn doors UNLOCKED (default)\n");
  CHECK(!interiorDisabled && !interiorCapped, "club interior re-enabled (it was disabled and capped)");
  CHECK(doorSys == 0, "front door (registered in the door system) set to unlocked");
  CHECK(!doorLockedNow[0] && !doorLockedNow[1] && !doorLockedNow[2] && !doorLockedNow[3], "front, back, office and dressing-room doors unlocked");
  CHECK(!doorLockedNow[4] && !doorLockedNow[5], "two extra internal doors found by probing are unlocked too");

  printf("== target marker and selection\n");
  tick(6);
  CHECK(markers > 0 && markerX > 111.9f && markerX < 112.1f, "marker drawn over the nearest possible ped (ped 6; dead ped 4 and mission ped 8 skipped)");
  press(0x74); tick(1);  // F5 next target
  CHECK(markerX > 118.9f && markerX < 119.1f, "F5 moves the marker to the next nearest (ped 2)");

  printf("== F9: possess the marked target\n");
  press(0x78);
  CHECK(playerPed == 2, "player now controls ped 2 (the marked target)");
  CHECK(ped(1)->mission && ped(1)->blocking && ped(1)->standing && ped(1)->invincible, "original character kept, protected and parked");
  CHECK(blipCount == 1, "original character marked with a blip");
  CHECK(!ped(8)->mission || ped(8)->scriptOwned, "mission ped untouched");

  printf("== F10: possess nearest animal, F11: return\n");
  press(0x79);
  CHECK(playerPed == 3 && !ped(2)->mission, "animal possessed; previous body released to the game");
  wanted = 2; press(0x7A);
  CHECK(playerPed == 1 && !ped(1)->invincible && !ped(1)->blocking && blipCount == 0, "back to the original: protection and blip removed");
  CHECK(wanted == 2, "wanted level kept on return (ClearWantedOnReturn=0)");
  wanted = 0;

  printf("== possession mode OFF / ON (menu row 0 and F12)\n");
  menuTo(0);
  int m = markers; tick(6);
  CHECK(markers == m, "mode OFF: no target marker");
  press(0x78);
  CHECK(playerPed == 1 && strstr(lastNote, "OFF"), "mode OFF: F9 does nothing and says so");
  press(0x7B);  // F12 toggles back ON
  tick(6);
  CHECK(markers > m, "F12 turns possession back ON (marker shows again)");

  printf("== mission characters are never possessed\n");
  for (int i = 1; i < pedCount; i++) if (peds[i].handle != 8) peds[i].exists = 0;
  int notes = notifications; press(0x78);
  CHECK(playerPed == 1 && notifications > notes, "only a mission ped nearby: nothing possessed, player told");
  peds[1].exists = 1; peds[1].mission = 0;  // ped 2 back

  printf("== possessed body dies -> automatic return\n");
  press(0x78); CHECK(playerPed == 2, "possessed ped 2 again");
  ped(2)->dead = 1; tick(3);
  CHECK(playerPed == 1, "auto-returned to the original character on death");

  printf("== doors LOCKED: original states restored (F8)\n");
  press(0x77); tick(2);
  CHECK(doorSys == 1, "front door's door-system state restored to its recorded original (locked)");
  CHECK(doorLockedNow[0] && doorLockedNow[1] && doorLockedNow[2] && doorLockedNow[3] && doorLockedNow[4] && doorLockedNow[5], "every door's recorded lock flag restored");
  tick(12);
  CHECK(doorSys == 1 && doorLockedNow[0], "doors stay as the game had them (no re-unlocking while LOCKED)");
  CHECK(!interiorDisabled, "interior not switched off while you're inside the club");
  ped(1)->x = 300.0f; tick(12);
  CHECK(interiorDisabled && interiorCapped, "interior's original flags restored once you're away from the club");
  ped(1)->x = 115.0f;
  press(0x77); tick(12);
  CHECK(doorSys == 0 && !doorLockedNow[0], "F8 again: doors UNLOCKED again");

  printf("== unlocks menu (rows 6-10)\n");
  menuTo(6); CHECK(weaponsGiven > 80, "Give all weapons: every valid weapon given");
  menuTo(7); CHECK(cash[0] == 500 + 1000000, "Add money: Michael's SP0_TOTAL_CASH + MoneyAmount");
  ped(1)->x = -2300.0f; wanted = 3; tick(3);
  CHECK(wanted == 0, "wanted level cleared inside Fort Zancudo (zone ARMYB)");
  ped(1)->x = 115.0f;
  menuTo(9); wanted = 2; tick(2);
  CHECK(maxWanted == 0 && wanted == 0, "Never wanted ON");
  menuTo(9); CHECK(maxWanted == 5, "Never wanted OFF restores max wanted 5");
  menuTo(10); CHECK(clockHour == 22, "Set time to night");
  int rects = drawRects; tick(3);
  CHECK(drawRects == rects, "menu closed (Back)");

  uint64_t us, instr; pg_asi_stats(&us, &instr);
  printf("== interpreter: last tick %llu instructions, %llu us\n", (unsigned long long)instr, (unsigned long long)us);
  CHECK(logFaults == 0, "no faults, missing imports or missing game commands logged by the port");
  CHECK(unknownHashes == 0, "every game command used is one the simulated world knows");
  printf("%s: %d failure(s)\n", failures ? "FAILED" : "ALL CHECKS PASSED", failures);
  return failures ? 1 : 0;
}
