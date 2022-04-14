#pragma once
#include <coreinit/filesystem.h>

struct FileInfo {
public:
    FSFileHandle handle;
    int fd;
};
