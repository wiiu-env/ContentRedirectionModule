#pragma once
#include "DirInfo.h"
#include <coreinit/filesystem_fsa.h>

typedef struct FSDirectoryEntryEx {
    FSADirectoryEntry realEntry = {};
    bool isMarkedAsDeleted      = false;
} FSDirectoryEntryEx;

struct DirInfoEx final : DirInfo {
    FSDirectoryEntryEx *readResult  = nullptr;
    int readResultCapacity          = 0;
    int readResultNumberOfEntries   = 0;
    FSDirectoryHandle realDirHandle = 0;
};

struct DirInfoExSingleFile final : DirInfoBase {
    FSADirectoryEntry directoryEntry = {};
    bool entryRead                   = false;
    bool entryReadSuccess            = false;
    FSDirectoryHandle realDirHandle  = 0;
};
