#pragma once

#include "IFSWrapper.h"
#include <coreinit/filesystem.h>
#include <function_patcher/function_patching.h>
#include <functional>
#include <string>

extern function_replacement_data_t fs_dir_function_replacements[];
extern uint32_t fs_dir_function_replacements_size;
