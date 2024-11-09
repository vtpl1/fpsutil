// *****************************************************
//    Copyright 2024 Videonetics Technology Pvt Ltd
// *****************************************************

#include "fps_counter.h"
#include <chrono>

constexpr float MILLI_SECONDS_IN_SECONDS   = 1000.0F;
constexpr int   MILLI_SECONDS_IN_10SECONDS = 10000;

std::atomic_uint_fast64_t& FpsCounter::set_status(int64_t value) {
  value_ += value;
  return value_;
}

float FpsCounter::get_fps(int64_t ts) {
  calculate_fps_(ts);
  return last_fps_.load();
}

void FpsCounter::calculate_fps_(int64_t ts) {
  int64_t current_ts = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now())
                           .time_since_epoch()
                           .count();
  if (ts >= 0) {
    current_ts = ts;
  }
  const int64_t time_diff = current_ts - last_write_ts_;
  if (time_diff < MILLI_SECONDS_IN_10SECONDS) {
    return;
  }

  auto value_diff = value_ - last_value_;
  auto fps = static_cast<float>(value_diff) * MILLI_SECONDS_IN_SECONDS / static_cast<float>(time_diff);
  last_value_.store(value_);
  last_fps_ = fps;

  last_write_ts_ = current_ts;
}