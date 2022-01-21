#pragma once
#include <stdint.h>
#define TYPE_FILE 0
#define TYPE_DIR 1
#define TYPE_HRD_LNK 2
#define TYPE_SYM_LNK 3

#define PERM_RD 1
#define PERM_WR 2
#define PERM_EX 4

#define OS_FUSE 0
#define OS_LUKARNE 1
#define OS_BINCOWS 2
#define OS_LINUX 3
#define OS_BSD 4
#define OS_DARWIN 5
#define OS_REDOX 6
#define OS_NT 7

struct SUPERBLOCK {
    uint8_t MAGIC[4];
    uint16_t OS;
    uint8_t VERSION;
    uint8_t BLK_SZ;
    uint64_t BLK_LK_TBL;
    uint64_t RT;
    uint8_t GUID[16];
};

struct DESCRIPTOR {
    uint8_t TYPE;
    uint8_t PERMS[3];
    uint32_t OS_SPEC;
    uint64_t NAME_BLK;
    uint64_t UID;
    uint64_t GID;
    uint8_t GUID[16];
    uint64_t CONTENT_BLK;
    uint64_t CTIME;
    uint64_t MTIME;
    uint64_t ATIME;
    uint64_t SIZE;
    uint8_t HARD_LNK_CNT;
    uint8_t NAME[1];
};