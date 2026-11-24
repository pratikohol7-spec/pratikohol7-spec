// Stubs for the three macOS-only Blink files (CPU count, ioctl, sysinfo) and one app hook,
// so the port's ASI interpreter links on Linux for testing. None are reached by .asi mods.
#include <stdbool.h>
int GetCpuCount(void) { return 1; }
int SysIoctl(void *m, int fd, unsigned long request, long long addr) { (void)m; (void)fd; (void)request; (void)addr; return -1; }
int sysinfo_linux(void *m, long long addr) { (void)m; (void)addr; return -1; }
int PGMeshReleaseOn(void) { return 0; }
