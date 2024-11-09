// *****************************************************
//    Copyright 2024 Videonetics Technology Pvt Ltd
// *****************************************************

#pragma once
#ifndef fps_counter_h
#define fps_counter_h

#include <atomic>
#include <cstdint>
#include <fpsutil_export.h>

class FPSUTIL_EXPORT FpsCounter {
private:
  int64_t                   last_write_ts_{0};
  std::atomic_uint_fast64_t value_{0};
  std::atomic_uint_fast64_t last_value_{0};
  std::atomic<float>        last_fps_{0.0};
  void                      calculate_fps_(int64_t ts);

public:
  std::atomic_uint_fast64_t& set_status(int64_t value = 1);
  float                      get_fps(int64_t ts = -1);
};
#endif // fps_counter_h
