// *****************************************************
//    Copyright 2023 Videonetics Technology Pvt Ltd
// *****************************************************

#pragma once
#ifndef monitor_h
#define monitor_h

#include <atomic>
#include <logging.h>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include <fpsutil_export.h>

struct Status {
  std::atomic_uint_fast64_t app_id{0};
  std::atomic_uint_fast64_t channel_id{0};
  std::atomic_uint_fast64_t thread_id{0};
  std::atomic_uint_fast64_t value{0};
  std::atomic_uint_fast64_t last_value{0};

  Status(uint64_t app_id, uint64_t channel_id, uint64_t thread_id);
  Status(uint64_t app_id, uint64_t channel_id, uint64_t thread_id, uint64_t value, uint64_t last_value);
  ~Status() = default;
};

class FPSUTIL_EXPORT Monitor {
private:
  bool             is_already_shutting_down_{false};
  std::atomic_bool do_shutdown_{false};
  std::atomic_bool is_internal_shutdown_{false};

  std::string session_dir_;
  std::string file_name_;

  std::shared_ptr<spdlog::logger> logger_;
  std::shared_ptr<spdlog::logger> summary_logger_;

  std::mutex                                                                  resource_map_mtx_;
  std::map<std::tuple<uint64_t, uint64_t, uint64_t>, std::unique_ptr<Status>> resource_map_;
  std::unique_ptr<std::thread>                                                thread_;

  int64_t last_write_ts_;
  bool    do_write_header_{false};

  std::atomic_uint_fast64_t& set_status_(uint64_t app_id, uint64_t channel_id, uint64_t thread_id);
  void                       write_data_();
  void                       write_header_();

  void run_();

  Monitor(std::string session_dir, std::string file_name);
  ~Monitor();

public:
  static Monitor&                   getInstance();
  static Monitor&                   getInstance(std::string session_dir, std::string file_name);
  static std::atomic_uint_fast64_t& set_status(uint64_t app_id, uint64_t channel_id, uint64_t thread_id);
};

#endif // monitor_h
