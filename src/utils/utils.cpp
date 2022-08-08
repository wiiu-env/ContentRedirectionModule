#include "utils/logger.h"
#include <coreinit/filesystem.h>
#include <cstdio>
#include <string.h>
#include <sys/stat.h>
#include <whb/log.h>

#define PRINTF_BUFFER_LENGTH 2048

// https://gist.github.com/ccbrown/9722406
void dumpHex(const void *data, size_t size) {
    char ascii[17];
    size_t i, j;
    ascii[16] = '\0';
    DEBUG_FUNCTION_LINE("0x%08X (0x0000): ", data);
    for (i = 0; i < size; ++i) {
        WHBLogWritef("%02X ", ((unsigned char *) data)[i]);
        if (((unsigned char *) data)[i] >= ' ' && ((unsigned char *) data)[i] <= '~') {
            ascii[i % 16] = ((unsigned char *) data)[i];
        } else {
            ascii[i % 16] = '.';
        }
        if ((i + 1) % 8 == 0 || i + 1 == size) {
            WHBLogWritef(" ");
            if ((i + 1) % 16 == 0) {
                WHBLogPrintf("|  %s ", ascii);
                if (i + 1 < size) {
                    DEBUG_FUNCTION_LINE("0x%08X (0x%04X); ", ((uint32_t) data) + i + 1, i + 1);
                }
            } else if (i + 1 == size) {
                ascii[(i + 1) % 16] = '\0';
                if ((i + 1) % 16 <= 8) {
                    WHBLogWritef(" ");
                }
                for (j = (i + 1) % 16; j < 16; ++j) {
                    WHBLogWritef("   ");
                }
                WHBLogPrintf("|  %s ", ascii);
            }
        }
    }
}

FSMode translate_permission_mode(mode_t mode) {
    // Convert normal Unix octal permission bits into CafeOS hexadecimal permission bits
    return (FSMode) (((mode & S_IRWXU) << 2) | ((mode & S_IRWXG) << 1) | (mode & S_IRWXO));
}

FSTime translate_time(time_t timeValue) {
    // FSTime stats at 1980-01-01, time_t starts at 1970-01-01
    auto EPOCH_DIFF_SECS_WII_U_FS_TIME = 315532800; //EPOCH_DIFF_SECS(WIIU_FSTIME_EPOCH_YEAR)
    auto adjustedTimeValue             = timeValue - EPOCH_DIFF_SECS_WII_U_FS_TIME;
    // FSTime is in microseconds, time_t is in seconds
    return adjustedTimeValue * 1000000;
}

void translate_stat(struct stat *posStat, FSStat *fsStat) {
    memset(fsStat, 0, sizeof(FSStat));
    fsStat->size = posStat->st_size;

    fsStat->mode = translate_permission_mode(posStat->st_mode);

    fsStat->flags = static_cast<FSStatFlags>(0x1C000000); // These bits are always set
    if (S_ISDIR(posStat->st_mode)) {
        fsStat->flags = static_cast<FSStatFlags>(fsStat->flags | FS_STAT_DIRECTORY);
    } else if (S_ISREG(posStat->st_mode)) {
        fsStat->flags     = static_cast<FSStatFlags>(fsStat->flags | FS_STAT_FILE);
        fsStat->allocSize = posStat->st_size;
        fsStat->quotaSize = posStat->st_size;
    }
    fsStat->modified = translate_time(posStat->st_atime);
    fsStat->created  = translate_time(posStat->st_ctime);
    fsStat->entryId  = posStat->st_ino;

    fsStat->owner = posStat->st_uid;
    fsStat->group = posStat->st_gid;
}
