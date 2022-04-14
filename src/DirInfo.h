#pragma once
#include <coreinit/filesystem.h>
#include <sys/dirent.h>

struct DirInfo {
    virtual ~DirInfo() = default;
    FSDirectoryHandle handle{};
    DIR *dir{};
    char path[0x280]{};
};
