#define _GNU_SOURCE
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <ogc/system.h>
#include <ogc/cache.h>
#include <ogc/ipc.h>

#include "realcode.h"
#include "common.h"
#include "sha.h"
#include "realcode_bin.h"
#include "realcode/source/shared.h"

static unsigned char realcode_stack[0x1000] [[gnu::aligned(0x20)]];
static int fd = IPC_ENOENT;

int realcode_init() {
	if (fd >= 0)
		return 0;

	int ret = do_sha_exploit(realcode_bin, true, 0, realcode_stack, sizeof realcode_stack, 0x7F, 0);
	if (ret < 0)
		return ret;

	int try = 500;
	while (fd == IPC_ENOENT) {
		ret = fd = IOS_Open(REALCODE_DEVICE_NAME, 0);
		usleep(50);
		if (!try--)
			break;
	}

	return ret;
}

void realcode_close() {
	if (fd >= 0) {
		IOS_Close(fd);
		fd = IPC_ENOENT;
	}
}

void* IOS_VirtualToPhysical(void* addr) {
	// Pointer is not checked by IPC if size == 0
	return (void *)IOS_Ioctl(fd, 0, (void *)addr, 0, 0, 0);
}

void IOS_InvalidateDCache(const void* addr, uint32_t len) {
	uint32_t params[2] = { (uint32_t)addr, len };
	IOS_Ioctl(fd, 2, params, sizeof params, 0, 0);
}

void IOS_FlushDCache(void* addr, uint32_t len) {
	uint32_t params[2] = { (uint32_t)addr, len };
	IOS_Ioctl(fd, 3, params, sizeof params, 0, 0);
}

void IOS_InvalidateICache(void) {
	IOS_Ioctl(fd, 1, 0, 0, 0, 0);
}

int IOS_SetUID(uint32_t uid, uint16_t gid) {
	uint32_t params[2] = { uid, gid };

	return IOS_Ioctl(fd, 0x2B, params, sizeof params, 0, 0);
}

int realcode_patch(void* start, unsigned len, const void* pattern, unsigned pattern_len, const void* patch, unsigned patch_len) {
	struct codepatch_params params = { .start = start, .len = len };
	ioctlv vectors[3] = {
		{ &params, sizeof params },
		{ (void *)pattern, pattern_len },
		{ (void *)patch, patch_len },
	};

	return IOS_Ioctlv(fd, 0xC0DE, patch ? 3 : 2, 0, vectors);
}

// weird place to put this but mehhh
int patch_flash_access() {
	/*
	 * Three things can happen here.
	 * >= 0: /dev/flash access is enabled.
	 * -6:   /dev/flash access is disabled, the IOS does not check for flash.
	 * -106: /dev/flash access is disabled, but the IOS does check for flash.
	 *
	 * Honestly, I'm not sure why they didn't hit the -6 as soon as the strncmp with /dev/ succeeded. Who is creating files under such a directory?
	 */
	int fd = IOS_Open("/dev/flash", 0);
	if (fd >= 0) {
		// puts("(/dev/flash is already accessible, how?)");

		IOS_Close(fd);
		return 0;
	}

	void* fs_text_va = (void *)0x20000000;
	void* fs_text = (void *)MEM_PHYSICAL_TO_K0(IOS_VirtualToPhysical(fs_text_va));
	int translate_offset = fs_text_va - fs_text;

	static const uint16_t patternA[] = { // new
		0x2300, // mov r3, #0
		0x2b01, // cmp r3, #1
		0xd102, // bne 0x8
		        // bl FS_OpenInterface
	};

	static const uint16_t patternB[] = { // old
		0x4643, // mov r3, r8
		0x2b01, // cmp r3, #1
		0xd102, // bne 0x8
		        // bl FS_OpenInterface
	};

	static const uint16_t pattern2[] = {
		0x68ab, // ldr r3, [r5, #0x8] @ request->open.uid
		0x2b00, // cmp r3, #0x0 @ (root)
		0xd002, // beq 0x8
		0x2001, // mov r0,#1
		0x4240, // rsb r0,r0 @ -1 (EPERM)
		0xe008, // b 0xa
		        // bl FS_OpenBoot2
	};

	uint16_t* pc = memmem(fs_text, 0x8000, patternA, sizeof patternA);
	if (!pc) {
		pc = memmem(fs_text, 0x8000, patternB, sizeof patternB);
		if (!pc) {
			errorf("Could not find stubbed /dev/flash check");
			return -1;
		}
	}

	pc += 3;

	/* https://graphics.stanford.edu/~seander/bithacks.html#FixedSignExtend */
	int x = ((pc[0] & 0x7FF) << 11) | (pc[1] & 0x7FF);
	int m = 1 << 21;
	int dst = (x ^ m) - m;

	uint16_t* FS_OpenFlash = pc + 2 + dst;
	// printf("FS_OpenFlash = %p\n", FS_OpenFlash);

	pc = memmem(pc, 0x100, pattern2, sizeof pattern2);
	if (!pc) {
		errorf("Where is the /dev/boot2 uid check?");
		return -2;
	}
	pc += 6;

	char* thestring = NULL;
	if (fd == -6) {
		thestring = memmem(fs_text, 0x8000, (const char[9]){"\0boot2"}, 8);
		if (!thestring++) {
			errorf("Could not find the string");
			return -3;
		}

		strcpy(thestring, "flash");
	} else if (fd == -106) {
		thestring = memmem(fs_text, 0x8000, (const char[16]){"flash\0\0\0boot2"}, 16);
		if (!thestring) {
			errorf("Could not find the string");
			return -4;
		}

		strcpy(thestring + 0, "boot2");
		strcpy(thestring + 8, "flash");
	} else {
		// syfau about trigraphs please
		errorf("IOS_Open(/dev/flash) returned %i(?\?)\n", fd);
		return fd;
	}


	dst = FS_OpenFlash - (pc + 2);
	pc[0] = 0xF000 | ((dst >> 11) & 0x7FF);
	pc[1] = 0xF800 | (dst & 0x7FF);
	DCFlushRange(pc, 4);

	DCFlushRange(thestring, 32);
	IOS_InvalidateICache();
	IOS_InvalidateDCache(thestring + translate_offset, 32);

	return 0;
}
