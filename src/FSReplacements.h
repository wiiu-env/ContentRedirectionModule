#pragma once

#include <function_patcher/function_patching.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern function_replacement_data_t fs_file_function_replacements[];
extern uint32_t fs_file_function_replacements_size;

#ifdef __cplusplus
}
#endif
