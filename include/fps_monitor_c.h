// *****************************************************
//    Copyright 2025 Videonetics Technology Pvt Ltd
// *****************************************************

#pragma once
#ifndef fps_monitor_c_h
#define fps_monitor_c_h
#include <fpsutil_export.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
void FPSUTIL_EXPORT set_status(uint64_t app_id, uint64_t channel_id, uint64_t thread_id);
#ifdef __cplusplus
}
#endif
#endif // fps_monitor_c_h
