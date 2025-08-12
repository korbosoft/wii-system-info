/* DI is so ass lmfao it fucking explodes if you give it a mismatching command/ioctl #. Literally it calls diFatalError and the thread cancels itself */

#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <ogc/ipc.h>

#include "common.h"
#include "di.h"

enum DICommandID {
    DI_CMD_INQUIRY = 0x12,

    DI_CMD_RESET = 0x8A,
    DI_CMD_STOP_MOTOR = 0xE3,

    DI_CMD_GET_COVER_REGISTER = 0x7A,
    DI_CMD_GET_COVER_STATUS = 0x88,

    DI_CMD_READ_DISK_ID = 0x70,
    DI_CMD_READ_DISK_RAW = 0x8D,
    DI_CMD_GET_LAST_LENGTH = 0x83,

    DI_CMD_OPEN_PARTITION = 0x8B,
    DI_CMD_CLOSE_PARTITION = 0x8C,

    DI_CMD_READ_DISK = 0x71,
};

typedef struct {
    uint8_t cmd;

    uint32_t arg[7];
} DICommand;
static_assert(offsetof(DICommand, arg) == 0x4);
static_assert(sizeof(DICommand) == 0x20);

typedef uint32_t DIOutBuffer[8] [[gnu::aligned(0x20)]];

static int fd = -1;

int DI_Init(void) {
    if (fd >= 0)
        return 0;

    int ret = fd = IOS_Open("/dev/di", 0);
    if (ret < 0)
        return ret;

    return 0;
}

void DI_Close(void) {
    if (fd >= 0)
        IOS_Close(fd);

    fd = -1;
}

enum DIReturnCode {
    DI_OK = 0x1,
    DI_DRIVE = 0x2,
    DI_COVER_CLOSED = 0x4,

    DI_TIMEOUT = 0x10,
    DI_SECURITY = 0x20,
    DI_VERIFY = 0x40,
    DI_INVALID = 0x80,
};

static int DI_ConvertReturnCode(int ret) {
    if (ret < 0)
        return ret;

    if (ret == DI_OK)
        return 0;

    if (ret == DI_DRIVE)
        return -1;

    return -ret;
}

static int DI_SendCommand(DICommand* command, void* out, unsigned len, bool useOutbuf) {
    if (fd < 0)
        return -10;

    int ret;
    DIOutBuffer outbuf;

    ret = IOS_Ioctl(fd, command->cmd, command, sizeof(DICommand), useOutbuf ? outbuf : out, useOutbuf ? sizeof(outbuf): len);
    if (useOutbuf)
        memcpy(out, outbuf, len);

    return DI_ConvertReturnCode(ret);
}

int DI_DriveInquiry(DI_DriveVersion* out) {
    DICommand command = {
        .cmd = DI_CMD_INQUIRY,
    };

    return DI_SendCommand(&command, out, sizeof(DI_DriveVersion), true);
}

int DI_ResetDrive(bool spinup) {
    DICommand command = {
        .cmd = DI_CMD_RESET,
        .arg = {
            [0] = spinup,
        },
    };

    return DI_SendCommand(&command, 0, 0, false);
}

int DI_StopMotor(bool eject) {
    DICommand command = {
        .cmd = DI_CMD_STOP_MOTOR,
        .arg = {
            [0] = eject,
        },
    };

    return DI_SendCommand(&command, 0, 0, true);
}

// Why are these two different things bro
int DI_GetCoverRegister(uint32_t* DICVR) {
    DICommand command = {
        .cmd = DI_CMD_GET_COVER_REGISTER,
    };

    return DI_SendCommand(&command, DICVR, sizeof *DICVR, true);
}

int DI_GetCoverStatus(uint32_t* status) {
    DICommand command = {
        .cmd = DI_CMD_GET_COVER_STATUS,
    };

    return DI_SendCommand(&command, status, sizeof *status, true);
}

int DI_ReadDiskID(DI_DiskID* out) {
    DICommand command = {
        .cmd = DI_CMD_READ_DISK_ID,
    };

    return DI_SendCommand(&command, out, 0x20, true);
}

// The word "Unencrypted" sucks.
// Enforced 32-byte alignment.
int DI_ReadDiskRaw(uint32_t wOffset, uint32_t size, void* out) {
    DICommand command = {
        .cmd = DI_CMD_READ_DISK_RAW,
        .arg = {
            [0] = size,
            [1] = wOffset,
        },
    };

    return DI_SendCommand(&command, out, size, false);
}

int DI_GetLastReadLength(uint32_t* out) {
    DICommand command = {
        .cmd = DI_CMD_GET_LAST_LENGTH,
    };

    return DI_SendCommand(&command, out, sizeof *out, true);
}

int DI_OpenPartition(uint32_t wOffset, const Certificate* certs, uint32_t certs_len, const Ticket* ticket, TitleMetadata* tmd) {
    if (fd < 0)
        return -10;

    DICommand command = {
        .cmd = DI_CMD_OPEN_PARTITION,
        .arg = {
            [0] = wOffset,
        },
    };

    DIOutBuffer outbuf;
    ioctlv vectors[] = {
        [0] = { &command, sizeof(DICommand) },
        [1] = { (void *)ticket, (ticket == NULL) ? 0 : sizeof(Ticket) },
        [2] = { (void *)certs, certs_len },

        [3] = { tmd, offsetof(TitleMetadata, contents[512]) },
        [4] = { outbuf, sizeof(DIOutBuffer) },
    };

    int ret = IOS_Ioctlv(fd, DI_CMD_OPEN_PARTITION, 3, 2, vectors);
    if (ret == DI_VERIFY)
        ret = (int)outbuf[0];
    else
        ret = DI_ConvertReturnCode(ret);

    return ret;
}

int DI_ClosePartition(void) {
    DICommand command = {
        .cmd = DI_CMD_CLOSE_PARTITION,
    };

    return DI_SendCommand(&command, 0, 0, false);
}

int DI_ReadDisk(uint32_t wOffset, uint32_t size, void* out) {
    DICommand command = {
        .cmd = DI_CMD_READ_DISK,
        .arg = {
            [0] = size,
            [1] = wOffset,
        },
    };

    return DI_SendCommand(&command, out, size, false);
}
