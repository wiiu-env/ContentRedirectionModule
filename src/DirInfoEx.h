#pragma once
#include "DirInfo.h"
#include <coreinit/filesystem_fsa.h>

typedef struct FSDirectoryEntryEx {
    FSADirectoryEntry realEntry{};
    bool isMarkedAsDeleted = false;
} FSDirectoryEntryEx;

struct DirInfoEx : public DirInfo {
public:
    FSDirectoryEntryEx *readResult  = nullptr;
    int readResultCapacity          = 0;
    int readResultNumberOfEntries   = 0;
    FSDirectoryHandle realDirHandle = 0;
};