#include "common.h"

static const uint8_t Boot2BlockmapSignature[8] = { 0x26, 0xf2, 0x9a, 0x40, 0x1e, 0xe6, 0x84, 0xcf };
typedef struct {
    uint8_t signature[8];
    uint32_t generation;
    uint8_t blocks[0x40];
} Boot2Blockmap;
CHECK_STRUCT_SIZE(Boot2Blockmap, 0x4C);

typedef struct {
    uint32_t header_size; // 0x20
    uint32_t data_offset;
    uint32_t certs_size;
    uint32_t ticket_size;
    uint32_t tmd_size;
    uint32_t padding[3];
} Boot2Header;
CHECK_STRUCT_SIZE(Boot2Header, 0x20);
