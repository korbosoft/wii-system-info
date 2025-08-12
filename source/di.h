#include <stdint.h>
#include <stdbool.h>
#include <errno.h>

#include "common.h"
#include "es_structs.h"

#define WII_DISK_MAGIC 0x5D1C9EA3
#define GCN_DISK_MAGIC 0xC2339F3D

#define DISK_WOFFSET(x) (x >> 2)
#define DISK_SIZE(x) (x << 2)

typedef struct DI_DriveVersion {
    uint16_t rev;
    uint16_t dev_code;
    union {
        uint32_t date;
        struct {
            uint32_t year: 16;
            uint32_t month: 8;
            uint32_t day: 8;
        };
    };
} DI_DriveVersion;
CHECK_STRUCT_SIZE(DI_DriveVersion, 0x8);

typedef struct DI_DiskID {
    char game_id[4];
    char maker_id[2];
    uint8_t disk_no;
    uint8_t disk_ver;
    uint8_t audio_streaming;
    uint8_t audio_stream_sz;
    uint8_t padding[14];
    uint32_t wii_magic;
    uint32_t gcn_magic;
} DI_DiskID;
CHECK_STRUCT_SIZE(DI_DiskID, 0x20);

typedef struct DI_DiskPartition {
    uint32_t wOffset;
    uint32_t type;
} DI_DiskPartition;

typedef struct DI_DiskPartitionGroup {
    uint32_t count;
    uint32_t wOffset;
} DI_DiskPartitionGroup;

int DI_Init(void);
void DI_Close(void);

int DI_DriveInquiry(DI_DriveVersion*);

int DI_ResetDrive(bool spinup);
int DI_StopMotor(bool eject);
int DI_GetCoverRegister(uint32_t* DICVR);
int DI_GetCoverStatus(uint32_t* status);

int DI_ReadDiskID(DI_DiskID* out);
int DI_ReadDiskRaw(uint32_t wOffset, uint32_t size, void* out);
int DI_GetLastReadLength(uint32_t* out);

int DI_OpenPartition(uint32_t wOffset, const Certificate* certs, uint32_t certs_len, const Ticket* ticket, TitleMetadata* tmd);
int DI_ClosePartition(void);
int DI_ReadDisk(uint32_t wOffset, uint32_t size, void* out);
