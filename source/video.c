#include <stdio.h>
#include <stdlib.h>
#include <ogc/system.h>
#include <ogc/irq.h>
#include <ogc/video.h>
#include <ogc/color.h>

#include "video.h"
// #include "font.h"

PrintConsole gConsole;

__attribute__((constructor))
void video_init() {
	GXRModeObj vmode;
	void* framebuffer = NULL;

	VIDEO_Init();
	VIDEO_SetBlack(true);
	VIDEO_GetPreferredMode(&vmode);

	uint32_t level = IRQ_Disable();
	{
		unsigned int framebuffer_size = VIDEO_GetFrameBufferSize(&vmode);
		framebuffer = (void *)__builtin_align_down((uintptr_t)SYS_GetArenaHi() - framebuffer_size, 0x20);
		if (framebuffer < SYS_GetArenaLo()) __builtin_trap(); // ??
		SYS_SetArenaHi(framebuffer);
	}
	IRQ_Restore(level);

	VIDEO_ClearFrameBuffer(&vmode, MEM_K0_TO_K1(framebuffer), COLOR_BLACK);
	VIDEO_Configure(&vmode);
	VIDEO_SetNextFramebuffer(MEM_K0_TO_K1(framebuffer));
	VIDEO_SetBlack(false);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	VIDEO_WaitVSync();

	// const int padX = 24, padY = 16;
	const int padX = 8, padY = 24;
	CON_InitEx(&vmode, padX, padY, vmode.fbWidth - (padX * 2), vmode.xfbHeight - (padY * 2));

	gConsole = *consoleSelect(&gConsole);
	// consoleSetFont(&gConsole, &the_cool_font);
	// consoleSetWindow(&gConsole, 2, 2, gConsole.con_cols - 2, gConsole.con_rows - 2);
}
