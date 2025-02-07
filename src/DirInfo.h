#pragma once
#include <coreinit/filesystem.h>
#include <sys/dirent.h>

struct DirInfoBase {
    virtual ~DirInfoBase() = default;
    FSDirectoryHandle handle{};
};

struct DirInfo : DirInfoBase {
    ~DirInfo() override = default;
    DIR *dir{};
    char path[0x280]{};
};
