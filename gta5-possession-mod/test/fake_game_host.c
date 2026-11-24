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

// ---- fake world ------------------------------------------------------------------------------
typedef struct { int handle; float x, y, z; int type; int human; int dead; int mission; int invincible; int blocking; int standing; int exists; } Ped;
static Ped peds[8]; static int pedCount;
static int playerPed = 1, frame;
static int blips[16], blipCount, nextBlip = 500;
static int drawRects, drawTexts, notifications; static char lastNote[256];
static int doorStateCalls, closestDoorCalls, doorFound, interiorDisabled = 1, interiorCapped = 1, objectProbes;
static int changeCalls;
static int weaponsGiven, wanted, maxWanted = 5, clockHour = 12; static int64_t cash[3] = {500, 500, 500};
static uint64_t zoneStr;
static uint32_t joaat(const char *s) { uint32_t h = 0; for (; *s; ++s) { char c = *s; if (c >= 'A' && c <= 'Z') c += 32; h += (unsigned char)c; h += h << 10; h ^= h >> 6; } h += h << 3; h ^= h >> 11; h += h << 15; return h; }
static Ped *ped(int h) { for (int i = 0; i < pedCount; i++) if (peds[i].handle == h && peds[i].exists) return &peds[i]; return 0; }
static void addPed(int h, float x, float y, float z, int type, int human, int dead) {
  peds[pedCount++] = (Ped){h, x, y, z, type, human, dead, 0, 0, 0, 0, 1};
}
static uint64_t arg(uint64_t args, int i) { uint64_t v; memcpy(&v, mem + args + 8 * i, 8); return v; }
static float argf(uint64_t args, int i) { float f; memcpy(&f, mem + args + 8 * i, 4); return f; }
static int argi(uint64_t args, int i) { return (int)(uint32_t)arg(args, i); }
static void ret(uint64_t r, uint64_t v) { memcpy(mem + r, &v, 8); }
static void retf3(uint64_t r, float x, float y, float z) { memset(mem + r, 0, 24); memcpy(mem + r, &x, 4); memcpy(mem + r + 8, &y, 4); memcpy(mem + r + 16, &z, 4); }
static const char *str(uint64_t a) { return a ? (const char *)mem + a : "(null)"; }
static char pendingText[256];
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
    case 0x7239B21A38F536BAull: ret(r, ped(argi(a, 0)) != 0 || (argi(a, 0) >= 900 && argi(a, 0) < 910)); return true;  // DOES_ENTITY_EXIST
    case 0x5F9532F3B5CC2551ull: { Ped *p = ped(argi(a, 0)); ret(r, !p || p->dead); return true; }             // IS_ENTITY_DEAD
    case 0x3FEF770D40960D5Aull: { int h = argi(a, 0); Ped *p = ped(h);                                       // GET_ENTITY_COORDS
      if (p) retf3(r, p->x, p->y, p->z); else if (h >= 900) retf3(r, 120.0f + (h - 900), -1300.0f, 29.4f); else retf3(r, 0, 0, 0); return true; }
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
    case 0xE143FA2249364369ull: {                                                                           // GET_CLOSEST_OBJECT_OF_TYPE
      objectProbes++; float x = argf(a, 0), y = argf(a, 1);
      // two extra dressing-room doors at (121,-1300) and (123,-1300)
      int h = 0; if (x > 117 && x < 127 && y > -1305 && y < -1295) h = x < 122 ? 901 : 903;
      ret(r, (uint32_t)h); return true; }
    case 0xF82D8F1926A02C3Dull: closestDoorCalls++; return true;                                            // SET_STATE_OF_CLOSEST_DOOR_OF_TYPE
    case 0x589F80B325CC82C5ull: { float x = argf(a, 0); if (x > 127 && x < 129) { uint64_t h = 0xABCD; memcpy(mem + arg(a, 4), &h, 8); doorFound++; ret(r, 1); } else ret(r, 0); return true; }
    case 0x6BAB9442830C7F53ull: if (argi(a, 0) == 0xABCD && argi(a, 1) == 0) doorStateCalls++; return true; // DOOR_SYSTEM_SET_DOOR_STATE
    case 0xB0F7F8663821D9C3ull: ret(r, 77); return true;                                                     // GET_INTERIOR_AT_COORDS
    case 0xBC5115A5A939DD15ull: ret(r, interiorDisabled); return true;
    case 0x6170941419D7D8ECull: interiorDisabled = argi(a, 1); return true;
    case 0x92BAC8ACF88CEC26ull: ret(r, interiorCapped); return true;
    case 0xD9175F941610DB54ull: interiorCapped = argi(a, 1); return true;
    case 0x3A618A217E5154F0ull: drawRects++; return true;                                                    // DRAW_RECT
    case 0x66E0276CC5F6B9DAull: case 0x07C837F9A01C34C9ull: case 0xBE6B23FFA53FB442ull: case 0x2513DFB0FB8400FEull: return true;
    case 0x25FBB336DF1804CBull: case 0x202709F4C58A0424ull: if (strcmp(str(arg(a, 0)), "STRING")) unknownHashes++; return true;
    case 0x6C188BE134E074AAull: snprintf(pendingText, sizeof pendingText, "%s", str(arg(a, 0))); return true;
    case 0xCD015E5BB0D96A57ull: drawTexts++; return true;
    case 0x2ED7843F8F801023ull: notifications++; snprintf(lastNote, sizeof lastNote, "%s", pendingText); printf("[frame %3d] notification: %s\n", frame, pendingText); return true;
    case 0x9CD27B0045628463ull: ret(r, (uint64_t)(frame * 100)); return true;                               // GET_GAME_TIMER (100 ms per frame)
    case 0x937C71165CF334B3ull: ret(r, (uint32_t)arg(a, 0) != joaat("WEAPON_RAYPISTOL")); return true;      // IS_WEAPON_VALID (one "missing" weapon)
    case 0xBF0FD6E56C964FCBull: if (argi(a, 0) == playerPed && argi(a, 2) == 9999) weaponsGiven++; return true;  // GIVE_WEAPON_TO_PED
    case 0x9F47B058362C84B5ull: ret(r, argi(a, 0) == 1 ? joaat("player_zero") : joaat("a_m_y_business_01")); return true;  // GET_ENTITY_MODEL
    case 0x767FBC2AC802EF3Dull: { uint32_t st = (uint32_t)arg(a, 0); int i = st == joaat("SP0_TOTAL_CASH") ? 0 : st == joaat("SP1_TOTAL_CASH") ? 1 : st == joaat("SP2_TOTAL_CASH") ? 2 : -1;
      if (i < 0) { ret(r, 0); return true; } int v = (int)cash[i]; memcpy(mem + arg(a, 1), &v, 4); ret(r, 1); return true; }  // STAT_GET_INT
    case 0xB3271D7AB655B441ull: { uint32_t st = (uint32_t)arg(a, 0); if (st == joaat("SP0_TOTAL_CASH")) cash[0] = argi(a, 1); return true; }  // STAT_SET_INT
    case 0xCD90657D4C30E1CAull: { if (!zoneStr) zoneStr = hostAlloc(16); Ped *p = ped(playerPed);                  // GET_NAME_OF_ZONE
      strcpy((char *)mem + zoneStr, p && p->x < -1500 ? "ARMYB" : "STRIP"); ret(r, zoneStr); return true; }
    case 0xE28E54788CE8F12Dull: ret(r, (uint32_t)wanted); return true;                                       // GET_PLAYER_WANTED_LEVEL
    case 0xB302540597885499ull: wanted = 0; return true;                                                      // CLEAR_PLAYER_WANTED_LEVEL
    case 0xAA5F02DB48D704B9ull: maxWanted = argi(a, 0); return true;                                          // SET_MAX_WANTED_LEVEL
    case 0x47C3B5848C3E45D8ull: clockHour = argi(a, 0); return true;                                          // SET_CLOCK_TIME
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
  addPed(6, 112.0f, -1290.0f, 29.4f, 5, 1, 0);   // pedestrian, 3 m (nearest human)

  printf("== load\n");
  int id = pg_asi_load(name, argv[2], bytes, (uint64_t)st.st_size);
  CHECK(id > 0, "the port's loader accepts PossessionV.asi and runs DllMain");
  tick(15);
  CHECK(notifications >= 1, "script thread runs and posts its start-up notification");
  CHECK(!interiorDisabled && !interiorCapped, "Vanilla Unicorn interior re-enabled when it was disabled/capped");
  CHECK(doorFound > 0 && doorStateCalls > 0, "front door found in the door system and set to unlocked");
  CHECK(closestDoorCalls >= 4, "all four club doors (front, back, office, dressing room) unlocked");
  int beforeExtra = closestDoorCalls; tick(10);
  CHECK(closestDoorCalls - beforeExtra >= 6, "extra internal doors found by probing are unlocked too (4 + 2 per pass)");

  printf("== F9: possess nearest pedestrian\n");
  press(0x78);
  CHECK(playerPed == 6, "player now controls ped 6 (nearest living pedestrian; dead ped 4 skipped)");
  CHECK(ped(1)->mission && ped(1)->blocking && ped(1)->standing && ped(1)->invincible, "original character kept, protected and parked");
  CHECK(blipCount == 1, "original character marked with a blip");

  printf("== F10: possess nearest animal\n");
  press(0x79);
  CHECK(playerPed == 3, "player now controls the animal (ped 3)");
  CHECK(!ped(6)->mission, "previous possessed pedestrian released back to the game");

  printf("== F11: return to original\n");
  press(0x7A);
  CHECK(playerPed == 1, "player is the original character again");
  CHECK(!ped(1)->invincible && !ped(1)->blocking, "original's protection removed");
  CHECK(blipCount == 0, "blip removed");
  CHECK(!ped(3)->mission, "animal released");

  printf("== F7 menu: Down, Enter (possess nearest animal), Back\n");
  int rects = drawRects; press(0x76); tick(2);
  CHECK(drawRects > rects, "menu is drawn (DRAW_RECT), so the port shows its touch D-pad");
  press(0x28); press(0x0D);
  CHECK(playerPed == 3, "menu item 2 possesses the animal");
  press(0x08); rects = drawRects; tick(3);
  CHECK(drawRects == rects, "Backspace closes the menu");

  printf("== possessed body dies -> automatic return\n");
  ped(3)->dead = 1; tick(3);
  CHECK(playerPed == 1, "auto-returned to the original character on death");

  printf("== nothing to possess\n");
  for (int i = 1; i < pedCount; i++) peds[i].exists = 0;
  int notes = notifications; press(0x78);
  CHECK(playerPed == 1 && notifications > notes, "no target: stays put and tells the player");

  printf("== numpad navigation (Simple-Trainer style keys)\n");
  addPed(7, 117.0f, -1290.0f, 29.4f, 4, 1, 0);
  press(0x76); press(0x65);
  CHECK(playerPed == 7, "NumPad5 selects the first item (possess pedestrian)");
  press(0x60); rects = drawRects; tick(2);  // NumPad0 closes the menu
  CHECK(drawRects == rects, "NumPad0 closes the menu");
  press(0x7A);
  CHECK(playerPed == 1, "F11 returns again");

  printf("== unlocks menu\n");
  // menu rows: 0 ped, 1 animal, 2 anyone, 3 return, 4 doors, 5 weapons, 6 money, 7 restricted, 8 never wanted, 9 night, 10 close
  press(0x76); for (int i = 0; i < 5; i++) press(0x28); press(0x0D);
  CHECK(weaponsGiven > 80, "Give all weapons: every valid weapon given with full ammo (invalid one skipped)");
  press(0x28); press(0x0D);
  CHECK(cash[0] == 500 + 1000000, "Add money: Michael's SP0_TOTAL_CASH goes up by MoneyAmount from the .ini");
  wanted = 3; tick(2);
  CHECK(wanted == 3, "wanted level kept outside restricted areas");
  ped(1)->x = -2300.0f; tick(2);
  CHECK(wanted == 0, "wanted level cleared inside Fort Zancudo (zone ARMYB)");
  ped(1)->x = 115.0f;
  press(0x28); press(0x28); press(0x0D);  // row 8: never wanted
  wanted = 2; tick(2);
  CHECK(maxWanted == 0 && wanted == 0, "Never wanted ON: max wanted 0 and wanted level cleared anywhere");
  press(0x0D);  // toggle off again
  CHECK(maxWanted == 5, "Never wanted OFF: max wanted level restored to 5");
  press(0x28); press(0x0D);
  CHECK(clockHour == 22, "Set time to night: clock set to 22:00");
  press(0x08);

  uint64_t us, instr; pg_asi_stats(&us, &instr);
  printf("== interpreter: last tick %llu instructions, %llu us\n", (unsigned long long)instr, (unsigned long long)us);
  CHECK(logFaults == 0, "no faults, missing imports or missing game commands logged by the port");
  CHECK(unknownHashes == 0, "every game command used is one the simulated world knows");
  printf("%s: %d failure(s)\n", failures ? "FAILED" : "ALL CHECKS PASSED", failures);
  return failures ? 1 : 0;
}
