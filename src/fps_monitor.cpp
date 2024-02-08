// *****************************************************
//  Copyright 2023 Videonetics Technology Pvt Ltd
// *****************************************************

#include "fps_monitor.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <fmt/core.h>
#include <logging.h>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

constexpr int32_t LOG_INTERVAL_SEC = 10;

Status::Status(uint64_t app_id, uint64_t channel_id, uint64_t thread_id)
    : Status(app_id, channel_id, thread_id, 0, 0) {}

Status::Status(uint64_t app_id_, uint64_t channel_id_, uint64_t thread_id_, uint64_t value_, uint64_t last_value_)
    : app_id(app_id_), channel_id(channel_id_), thread_id(thread_id_), value(value_), last_value(last_value_) {}

FpsMonitor::FpsMonitor(std::string session_dir, std::string file_name)
    : session_dir_(std::move(session_dir)), file_name_(std::move(file_name)), last_write_ts_(0) {
  thread_ = std::make_unique<std::thread>(&FpsMonitor::run_, this);
}

FpsMonitor::~FpsMonitor() {
  do_shutdown_ = true;
  if (thread_) {
    if (thread_) {
      thread_->join();
    }
    thread_ = nullptr;
  }

  for (auto&& itr : resource_map_) {
    itr.second.reset();
    itr.second = nullptr;
  }
}

auto FpsMonitor::getInstance() -> FpsMonitor& { return getInstance("session", "fps_common"); }

auto FpsMonitor::getInstance(std::string session_dir, std::string file_name) -> FpsMonitor& {
  static FpsMonitor instance(std::move(session_dir), std::move(file_name));
  return instance;
}

auto FpsMonitor::set_status(uint64_t app_id, uint64_t channel_id, uint64_t thread_id) -> std::atomic_uint_fast64_t& {
  return FpsMonitor::getInstance().set_status_(app_id, channel_id, thread_id);
}

auto FpsMonitor::set_status_(uint64_t app_id, uint64_t channel_id, uint64_t thread_id) -> std::atomic_uint_fast64_t& {
  const std::lock_guard<std::mutex> lock(resource_map_mtx_);

  auto key = std::tuple<uint64_t, uint64_t, uint64_t>(app_id, channel_id, thread_id);
  auto itr = resource_map_.find(key);
  if (itr != resource_map_.end()) {
    itr->second->value++;
    return itr->second->value;
  }

  auto map = resource_map_.emplace(key, std::move(std::make_unique<Status>(app_id, channel_id, thread_id)));
  return map.first->second->value;
}

void FpsMonitor::write_header_() {
  {
    std::stringstream ss_header;
    std::stringstream ss_data;

    {
      const std::lock_guard<std::mutex> lock(resource_map_mtx_);
      for (auto&& itr : resource_map_) {
        ss_header << "App .Chn .Thr             Fps|";
        ss_data << fmt::format("{:04}.{:04}.{:04}            fps|", itr.second->app_id.load(),
                               itr.second->channel_id.load(), itr.second->thread_id.load());
      }
    }

    if (!ss_header.str().empty() && !ss_data.str().empty()) {
      write_header(logger_, ss_header.str());
      write_log(logger_, ss_data.str());
    }
  }

  {
    std::stringstream ss_summary;
    {
      const std::lock_guard<std::mutex> lock(resource_map_mtx_);
      ss_summary << "Total channels (x2) " << resource_map_.size();
    }

    if (!ss_summary.str().empty()) {
      write_header(summary_logger_, ss_summary.str());
    }
  }
}

void FpsMonitor::write_data_() {
  const int64_t current_ts = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now())
                                 .time_since_epoch()
                                 .count();
  const int64_t time_diff = current_ts - last_write_ts_;

  if (time_diff <= 0) {
    return;
  }
  if (do_write_header_) {
    do_write_header_ = false;
    write_header_();
  }

  std::vector<std::tuple<uint64_t, uint64_t, uint64_t>> valid_list;
  std::vector<std::tuple<uint64_t, uint64_t, uint64_t>> invalid_list;

  const std::time_t time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

  char time_buf[20]; // NOLINT
  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
  strftime(time_buf, 20, "%Y-%m-%d %H:%M:%S", localtime(&time));
  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
  std::string const current_time(time_buf);

  {
    std::stringstream ss_data;
    {
      const std::lock_guard<std::mutex> lock(resource_map_mtx_);
      for (auto&& itr : resource_map_) {
        const float fps = (static_cast<float>(itr.second->value) - static_cast<float>(itr.second->last_value)) *
                          1000.0F / static_cast<float>(time_diff);
        itr.second->last_value.store(itr.second->value);

        ss_data << fmt::format("{} {:>9.{}f}|", current_time, fps, 1);

        if (10 <= fps && 40 >= fps) {
          valid_list.emplace_back(itr.first);
        } else {
          invalid_list.emplace_back(itr.first);
        }
      }
    }

    if (!ss_data.str().empty()) {
      write_log(logger_, ss_data.str());
    }
  }

  {
    std::stringstream ss_summary;

    auto valid_list_size = valid_list.size();
    ss_summary << fmt::format("{} Valid   (x2) ({:04}) ", current_time, valid_list_size);
    if (0 < valid_list_size && 100 > valid_list_size) {
      ss_summary << fmt::format(":");
      for (auto&& itr : valid_list) {
        ss_summary << fmt::format(" {:04}.{:04}.{:04}|", std::get<0>(itr), std::get<1>(itr), std::get<2>(itr));
      }
    }
    write_log(summary_logger_, ss_summary.str());
  }
  {
    std::stringstream ss_summary;

    auto invalid_list_size = invalid_list.size();
    ss_summary << fmt::format("{} Invalid (x2) ({:04}) ", current_time, invalid_list_size);
    if (0 < invalid_list_size && 100 > invalid_list_size) {
      ss_summary << fmt::format(":");
      for (auto&& itr : invalid_list) {
        ss_summary << fmt::format(" {:04}.{:04}.{:04}|", std::get<0>(itr), std::get<1>(itr), std::get<2>(itr));
      }
    }
    write_log(summary_logger_, ss_summary.str());
  }
  {
    std::stringstream ss_summary;
    ss_summary << fmt::format("{} ", current_time);
    ss_summary << "--------------";
    write_log(summary_logger_, ss_summary.str());
  }
  last_write_ts_ = current_ts;
}

void FpsMonitor::run_() {
  int sleep_upto_sec = LOG_INTERVAL_SEC;

  logger_             = get_logger_st(session_dir_, file_name_);
  summary_logger_     = get_logger_st(session_dir_, fmt::format("{}_summary", file_name_));
  auto last_list_size = resource_map_.size();
  do_write_header_    = true;

  while (!do_shutdown_) {
    if (sleep_upto_sec > 0) {
      sleep_upto_sec--;
    } else {
      sleep_upto_sec = LOG_INTERVAL_SEC;
      {
        const std::lock_guard<std::mutex> lock(resource_map_mtx_);
        if (last_list_size != resource_map_.size()) {
          last_list_size   = resource_map_.size();
          do_write_header_ = true;
        }
      }
      write_data_();
    }
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  write_data_();
}
