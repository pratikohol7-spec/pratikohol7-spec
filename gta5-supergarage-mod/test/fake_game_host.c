// Runs SuperGarageV.asi inside the GTA V iOS port's own Script Hook V interpreter
// (app/Source/NativeASI.c + Blink) with a simulated game world. Exits 0 when every check passes.
//   fake_game_host <SuperGarageV.asi> <mod folder>
#include "NativeASI.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

static uint8_t *mem; static const uint64_t memSize = 3ull << 30; static uint64_t top = 0x100000, bins[48];
static int binOf(uint64_t s) { int b = 5; while ((1ull << b) < s + 16) b++; return b; }
static uint64_t hostAlloc(uint64_t size) {
  int b = binOf(size); uint64_t block;
  if (bins[b]) { block = bins[b]; memcpy(&bins[b], mem + block + 16, 8); } else { block = top; top += 1ull << b; if (top > memSize) return 0; }
  uint64_t bin = (uint64_t)b; memcpy(mem + block, &bin, 8); return block + 16;
}
static void hostFree(uint64_t a) { if (!a) return; uint64_t blk = a - 16, b; memcpy(&b, mem + blk, 8); memcpy(mem + blk + 16, &bins[b], 8); bins[b] = blk; }
static int logFaults;
static void hostLog(const char *line) {
  printf("[port] %s\n", line);
  if (strstr(line, "stopped") || strstr(line, "isn't in this game version") || strstr(line, "not available on iOS") || strstr(line, "not loaded")) logFaults++;
}
static uint32_t joaat(const char *s) { uint32_t h = 0; for (; *s; ++s) { char c = *s; if (c >= 'A' && c <= 'Z') c += 32; h += (unsigned char)c; h += h << 10; h ^= h >> 6; } h += h << 3; h ^= h >> 11; h += h << 15; return h; }

// ---- fake world: which models exist and their class ----------------------------------------
static const char *cars[] = {"adder", "autarch", "banshee2", "bullet", "cheetah", "cyclone", "deveste", "emerus", "entityxf", "entity2",
  "fmj", "furia", "gp1", "infernus", "italigtb", "italigtb2", "krieger", "le7b", "nero", "nero2", "osiris", "penetrator", "pfister811",
  "prototipo", "reaper", "s80", "sc1", "sheava", "sultanrs", "t20", "taipan", "tempesta", "tezeract", "thrax", "tigon", "turismor",
  "tyrant", "tyrus", "vacca", "vagner", "visione", "voltic", "xa21", "zentorno", "zorrusso", "champion", "ignus", "zeno", "torero2",
  "entity3", "lm87", "virtue", "pipistrello", "furia2"};
static const char *bikes[] = {"akuma", "bati", "bati2", "carbonrs", "double", "hakuchou", "hakuchou2", "lectro", "nemesis", "ruffian",
  "vader", "vortex", "defiler", "shotaro", "diablous2", "fcr", "fcr2", "pcj", "bf400", "thrust", "reever", "shinobi", "powersurge",
  "sanchez", "sanchez2", "manchez", "esskey", "vindicator"};
static const char *missing[] = {"lm87", "pipistrello", "furia2", "fcr"};  // newer than this game build
// other game vehicles used by the real-world picks, and one add-on installed by the player
static const char *others[] = {"massacro", "seven70", "paragon", "gauntlet", "gauntlet4", "myaddonbike"};
static const char *nameOf(uint32_t h) {
  for (size_t i = 0; i < sizeof cars / sizeof *cars; i++) if (joaat(cars[i]) == h) return cars[i];
  for (size_t i = 0; i < sizeof bikes / sizeof *bikes; i++) if (joaat(bikes[i]) == h) return bikes[i];
  for (size_t i = 0; i < sizeof others / sizeof *others; i++) if (joaat(others[i]) == h) return others[i];
  return 0;
}
static bool exists(uint32_t h) { const char *n = nameOf(h); if (!n) return false; for (size_t i = 0; i < 4; i++) if (!strcmp(n, missing[i])) return false; return true; }
static int classOf(uint32_t h) {
  const char *n = nameOf(h); if (!n) return -1;
  if (!strcmp(n, "sultanrs")) return 6;  // the game says Sports, not Super: must be left out
  for (size_t i = 0; i < sizeof others / sizeof *others; i++) if (!strcmp(others[i], n)) return strcmp(n, "myaddonbike") ? 6 : 8;
  for (size_t i = 0; i < sizeof cars / sizeof *cars; i++) if (!strcmp(cars[i], n)) return 7;
  return 8;
}
static int expectCars, expectBikes;

static int frame, playerPed = 1, inVehicle, nextVeh = 2000, lastCreated, lastCreatedModel, deleted, released, modelsReleased, loaded[4096];
static int warped, engineOn; static char soundOnVehicle[64]; static int soundVehicle;
static int vehExists[4096];
static char drawn[64][128]; static int drawnCount; static char pending[128];
static uint64_t labelBuf, textBuf;
static uint64_t arg(uint64_t a, int i) { uint64_t v; memcpy(&v, mem + a + 8 * i, 8); return v; }
static int argi(uint64_t a, int i) { return (int)(uint32_t)arg(a, i); }
static void ret(uint64_t r, uint64_t v) { memcpy(mem + r, &v, 8); }
static const char *str(uint64_t a) { return a ? (const char *)mem + a : ""; }
static int unknownHashes, notifications;
static int kbState = 3; static char kbTitle[64]; static uint64_t kbBuf; static const char *kbAnswer = "MyAddonBike";

static bool hostNative(uint64_t hash, uint64_t a, uint32_t n, uint64_t r) {
  (void)n;
  switch (hash) {
    case 0xD80958FC74E988A6ull: ret(r, (uint32_t)playerPed); return true;
    case 0x7239B21A38F536BAull: { int h = argi(a, 0); ret(r, h == playerPed || (h >= 2000 && h < 4096 && vehExists[h])); return true; }
    case 0x5F9532F3B5CC2551ull: ret(r, 0); return true;
    case 0x963D27A58DF860ACull: { uint32_t h = (uint32_t)arg(a, 0); loaded[h % 4096] = 1; return true; }  // REQUEST_MODEL
    case 0x98A4EB5D89A0C952ull: ret(r, loaded[(uint32_t)arg(a, 0) % 4096]); return true;                 // HAS_MODEL_LOADED
    case 0x35B9E0803292B641ull: ret(r, exists((uint32_t)arg(a, 0))); return true;                       // IS_MODEL_IN_CDIMAGE
    case 0x19AAC8F07BFEC53Eull: ret(r, nameOf((uint32_t)arg(a, 0)) != 0); return true;                  // IS_MODEL_A_VEHICLE
    case 0xDEDF1C8BD47C2200ull: ret(r, (uint32_t)classOf((uint32_t)arg(a, 0))); return true;
    case 0xAF35D0D2583051B0ull: { int v = nextVeh++; vehExists[v] = 1; lastCreated = v; lastCreatedModel = argi(a, 0); ret(r, (uint32_t)v); return true; }
    case 0xF75B0D629E1C063Dull: if (argi(a, 0) == playerPed && argi(a, 2) == -1) { inVehicle = argi(a, 1); warped++; } return true;
    case 0xE532F5D78798DAABull: modelsReleased++; return true;
    case 0xE83D4F9BA2A38914ull: { float f = 90.0f; memcpy(mem + r, &f, 4); return true; }
    case 0x49733E92263139D1ull: case 0x79D3B596FE44EE8Bull: return true;
    case 0x2497C4717C8B881Eull: engineOn++; return true;
    case 0xB736A491E64A32CFull: { int h; memcpy(&h, mem + arg(a, 0), 4); released = h; return true; }
    case 0xEA386986E786A54Full: { int h; memcpy(&h, mem + arg(a, 0), 4); if (h >= 2000 && h < 4096) vehExists[h] = 0; deleted = h; return true; }
    case 0xB215AAC32D25D019ull: { if (!labelBuf) labelBuf = hostAlloc(64); const char *nm = nameOf((uint32_t)arg(a, 0)); char *o = (char *)mem + labelBuf;
      size_t i = 0; for (; nm && nm[i] && i < 60; i++) o[i] = (char)(nm[i] >= 'a' && nm[i] <= 'z' ? nm[i] - 32 : nm[i]); o[i] = 0; ret(r, labelBuf); return true; }
    case 0xAC09CA973C564252ull: ret(r, 1); return true;
    case 0x7B5280EBA9840C72ull: { if (!textBuf) textBuf = hostAlloc(96); snprintf((char *)mem + textBuf, 90, "Car %s", str(arg(a, 0))); ret(r, textBuf); return true; }
    case 0x1899F328B0E12848ull: { float v[6] = {100, 0, 200, 0, 30, 0}; memcpy(mem + r, v, 24); return true; }
    case 0x4F0C413926060B38ull: soundVehicle = argi(a, 0); snprintf(soundOnVehicle, sizeof soundOnVehicle, "%s", str(arg(a, 1))); return true;
    case 0x9A9112A0FE9A4713ull: ret(r, (uint32_t)inVehicle); return true;
    case 0x997ABD671D25CA0Bull: ret(r, inVehicle != 0); return true;
    case 0x3A618A217E5154F0ull: case 0x66E0276CC5F6B9DAull: case 0x07C837F9A01C34C9ull: case 0xBE6B23FFA53FB442ull: case 0x2513DFB0FB8400FEull: return true;
    case 0x25FBB336DF1804CBull: case 0x202709F4C58A0424ull: return true;
    case 0x6C188BE134E074AAull: snprintf(pending, sizeof pending, "%s", str(arg(a, 0))); return true;
    case 0xCD015E5BB0D96A57ull: if (drawnCount < 64) snprintf(drawn[drawnCount++], 128, "%s", pending); return true;
    case 0x00DC833F2568DBF6ull: snprintf(kbTitle, sizeof kbTitle, "%s", str(arg(a, 1))); kbState = 0; return true;      // DISPLAY_ONSCREEN_KEYBOARD
    case 0x0CF2B696BBF945AEull: ret(r, (uint32_t)kbState); if (kbState == 0 && frame % 4 == 3) kbState = 1; return true;  // UPDATE: player types for a few frames
    case 0x8362B09B91893647ull: if (!kbBuf) kbBuf = hostAlloc(64); strcpy((char *)mem + kbBuf, kbAnswer); ret(r, kbState == 1 ? kbBuf : 0); return true;
    case 0x2ED7843F8F801023ull: notifications++; printf("[frame %3d] notification: %s\n", frame, pending); return true;
  }
  printf("[frame %3d] UNHANDLED native 0x%016llx\n", frame, (unsigned long long)hash); unknownHashes++; return false;
}
static int held; static bool hostKey(int k) { return k == held; }
void PGMeshTouch(const void *s, uint64_t l) { (void)s; (void)l; }
static int failures;
#define CHECK(c, what) do { if (c) printf("  PASS  %s\n", what); else { printf("  FAIL  %s\n", what); failures++; } } while (0)
static void tick(int n) { for (int i = 0; i < n; i++) { drawnCount = 0; pg_asi_tick(); frame++; } }
static void press(int vk) { held = vk; pg_asi_key(vk, true); tick(3); held = 0; pg_asi_key(vk, false); tick(2); }
static bool drew(const char *s) { for (int i = 0; i < drawnCount; i++) if (strstr(drawn[i], s)) return true; return false; }

int main(int argc, char **argv) {
  if (argc < 3) return 2;
  for (size_t i = 0; i < sizeof cars / sizeof *cars; i++) if (exists(joaat(cars[i])) && classOf(joaat(cars[i])) == 7) expectCars++;
  for (size_t i = 0; i < sizeof bikes / sizeof *bikes; i++) if (exists(joaat(bikes[i])) && classOf(joaat(bikes[i])) == 8) expectBikes++;
  mem = mmap(0, memSize, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE | MAP_NORESERVE, -1, 0);
  PGAsiHost host = {mem, memSize, hostAlloc, hostFree, hostLog, hostNative, 0, hostKey, 0, 2189, argv[2]};
  if (!pg_asi_init(&host)) return 1;
  int fd = open(argv[1], O_RDONLY); struct stat st; if (fd < 0 || fstat(fd, &st)) return 1;
  uint8_t *bytes = malloc((size_t)st.st_size); if (read(fd, bytes, (size_t)st.st_size) != st.st_size) return 1; close(fd);
  const char *name = strrchr(argv[1], '/'); name = name ? name + 1 : argv[1];

  char iniPath[1200]; snprintf(iniPath, sizeof iniPath, "%s/SuperGarageV.ini", argv[2]);
  FILE *ini = fopen(iniPath, "a"); if (ini) { fputs("Vehicle1=myaddonbike\nVehicle2=notinstalled\n", ini); fclose(ini); }
  printf("== load\n");
  CHECK(pg_asi_load(name, argv[2], bytes, (uint64_t)st.st_size) > 0, "the port's loader accepts SuperGarageV.asi and runs DllMain");
  tick(5);
  CHECK(notifications == 1, "script runs and posts its start-up notification");

  printf("== F6 opens the menu; the game decides which vehicles exist and their class\n");
  press(0x75); tick(1);
  char want[64]; snprintf(want, sizeof want, "Super cars (%d)", expectCars);
  CHECK(drew(want), "car count = models the game has in class Super (missing and non-Super models left out)");
  snprintf(want, sizeof want, "Super bikes (%d)", expectBikes);
  CHECK(drew(want), "bike count = models the game has in class Motorcycles");
  CHECK(drew("Real-world picks (17)"), "all 17 real-world picks are in this (simulated) game");
  CHECK(drew("Add-on vehicles (1)"), "add-on page lists only the installed add-on from SuperGarageV.ini");

  printf("== Dodge Challenger shortcut and its V8 sound on every vehicle\n");
  CHECK(drew("Dodge Challenger (Gauntlet Hellfire)"), "Challenger is the first menu row");
  press(0x0D);
  CHECK(lastCreatedModel == (int)joaat("gauntlet4") && inVehicle == lastCreated, "Challenger row spawns the Gauntlet Hellfire with the player in it");
  press(0x28); press(0x0D); tick(2);
  CHECK(soundVehicle == inVehicle && !strcmp(soundOnVehicle, "gauntlet4"), "sound-everywhere ON: current vehicle gets the Challenger sound");
  inVehicle = 3000; vehExists[3000] = 1; tick(2);
  CHECK(soundVehicle == 3000 && !strcmp(soundOnVehicle, "gauntlet4"), "getting in another vehicle gives it the Challenger sound too");
  press(0x0D); inVehicle = 0; soundVehicle = 0; soundOnVehicle[0] = 0; inVehicle = 3001; tick(2);
  CHECK(soundVehicle == 0, "sound-everywhere OFF: other vehicles keep their own sound");
  inVehicle = 0;

  printf("== real-world picks\n");
  press(0x28); press(0x0D); tick(1);
  CHECK(drew("Bugatti Chiron style: Car NERO"), "picks show the real-world style next to the in-game name");
  press(0x0D);
  CHECK(lastCreatedModel == (int)joaat("nero") && inVehicle == lastCreated && engineOn == 2, "Chiron pick spawns the Nero, player in driver's seat, engine on");
  for (int k = 0; k < 11; k++) press(0x28);   // row 11 = Bentley Continental GT style
  press(0x0D);
  CHECK(lastCreatedModel == (int)joaat("paragon"), "Bentley pick spawns the Paragon R (non-Super class still allowed on this page)");
  int bentley = lastCreated; inVehicle = 0;
  press(0x08);

  printf("== super cars list\n");
  press(0x28); press(0x0D); tick(1);
  CHECK(drew("Car ADDER") && !drew("Car SULTANRS"), "car list: in-game names, Sports-class model left out");
  press(0x0D);
  CHECK(lastCreatedModel == (int)joaat("adder") && !vehExists[bentley], "Adder spawned; previous spawn deleted (player had got out)");
  int adder = lastCreated;
  press(0x28); press(0x27); press(0x0D);
  CHECK(lastCreatedModel == (int)joaat("furia") && vehExists[adder] && released == adder, "paging row 1 -> 11 spawns the Furia; Adder kept (player sat in it)");
  press(0x08);

  printf("== add-on vehicles and spawn by name (iPhone keyboard)\n");
  press(0x28); press(0x28); press(0x0D); tick(1);
  CHECK(drew("myaddonbike") && !drew("notinstalled"), "add-on page shows the installed add-on only");
  press(0x0D);
  CHECK(lastCreatedModel == (int)joaat("myaddonbike"), "add-on spawned by its spawn name from the .ini");
  press(0x08); press(0x28); press(0x0D); tick(12);
  CHECK(!strcmp(kbTitle, "FMMC_KEY_TIP8"), "Spawn by name opens the game's text box (the port shows the iPhone keyboard)");
  CHECK(lastCreatedModel == (int)joaat("myaddonbike") && kbState == 1, "typed name (any case) spawned after the keyboard finished");
  kbAnswer = "nosuchcar"; int before = lastCreated; press(0x0D); tick(12);
  CHECK(lastCreated == before, "unknown typed name: nothing spawned (player told)");

  printf("== engine sound\n");
  press(0x28); press(0x0D); tick(1);                    // row 5: Engine sound list
  CHECK(drew("Own sound (default)"), "sound list opens with Own sound first");
  press(0x28); press(0x0D); tick(1);                    // row 1 = first pick (nero)
  CHECK(drew("Engine sound: Car NERO"), "chosen sound shown on the main page");
  press(0x28); press(0x0D);                             // row 6: apply to current vehicle
  CHECK(soundVehicle == inVehicle && !strcmp(soundOnVehicle, "nero"), "Apply: FORCE_USE_AUDIO_GAME_OBJECT(current vehicle, \"nero\")");
  press(0x28); press(0x0D); press(0x28); press(0x0D);   // warp + delete OFF
  for (int k = 0; k < 6; k++) press(0x26);              // row 8 -> row 2 (Super bikes)
  press(0x0D); press(0x0D);
  CHECK(lastCreatedModel == (int)joaat("akuma") && soundVehicle == lastCreated && !strcmp(soundOnVehicle, "nero"), "new bike spawn gets the chosen engine sound");
  CHECK(inVehicle != lastCreated, "with 'Put me in vehicle' OFF the player stays outside");
  press(0x08); press(0x08); tick(2); drawnCount = 0; tick(1);
  CHECK(drawnCount == 0, "Back twice closes the menu");

  CHECK(logFaults == 0, "no faults, missing imports or missing game commands logged by the port");
  CHECK(unknownHashes == 0, "every game command used is one the simulated world knows");
  printf("%s: %d failure(s)\n", failures ? "FAILED" : "ALL CHECKS PASSED", failures);
  return failures ? 1 : 0;
}
