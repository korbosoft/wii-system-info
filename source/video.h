#include <ogc/console.h>

extern PrintConsole gConsole;

#define cls() { printf(CONSOLE_ESC(2J)); }
#define printf_clearln(fmt, ...) printf("%s" fmt, "\r" CONSOLE_ESC(K), ##__VA_ARGS__)
