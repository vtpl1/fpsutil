// *****************************************************
//  Copyright 2023 Videonetics Technology Pvt Ltd
// *****************************************************

#include "monitor.h"

// #include <Poco/DateTime.h>
#include <Poco/DateTimeFormat.h>
#include <Poco/DateTimeFormatter.h>

constexpr int max_sleep_upto_sec     = 10;
constexpr int time_out_in_sec_30_sec = 30;

Status::Status(uint64_t app_id, uint64_t channel_id, uint64_t thread_id)
    : Status(app_id, channel_id, thread_id, 0, 0) {}

Status::Status(uint64_t app_id_, uint64_t channel_id_, uint64_t thread_id_, uint64_t value_, uint64_t last_value_)
    : app_id(app_id_), channel_id(channel_id_), thread_id(thread_id_), value(value_), last_value(last_value_) {}

Monitor::Monitor(std::string session_dir, std::string file_name)
    : session_dir_(std::move(session_dir)), file_name_(std::move(file_name)) {
  thread_ = std::make_unique<std::thread>(&Monitor::run_, this);
}

Monitor::~Monitor() {
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

Monitor& Monitor::getInstance() { return getInstance("session", "fps_common"); }

Monitor& Monitor::getInstance(std::string session_dir, std::string file_name) {
  static Monitor instance(std::move(session_dir), std::move(file_name));
  return instance;
}

std::atomic_uint_fast64_t& Monitor::set_status(uint64_t app_id, uint64_t channel_id, uint64_t thread_id) {
  return Monitor::getInstance().set_status_(app_id, channel_id, thread_id);
}

std::atomic_uint_fast64_t& Monitor::set_status_(uint64_t app_id, uint64_t channel_id, uint64_t thread_id) {
  const std::lock_guard<std::mutex> lock(resource_map_mtx_);

  auto key = std::tuple<uint64_t, uint64_t, uint64_t>(app_id, channel_id, thread_id);
  auto itr = resource_map_.find(key);
  if (itr != resource_map_.end()) {
    itr->second->value++;
    return itr->second->value;
  }

  auto map =
      resource_map_.emplace(std::make_pair(key, std::move(std::make_unique<Status>(app_id, channel_id, thread_id))));
  return map.first->second->value;
}

int64_t get_current_time_in_ms() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
      .count();
}

void Monitor::write_header_() {
  {
    std::stringstream ss_header;
    std::stringstream ss_data;

    {
      const std::lock_guard<std::mutex> lock(resource_map_mtx_);
      for (auto&& itr : resource_map_) {
        ss_header << "App .Chn .Thr             Fps|";
        ss_data << fmt::format("{:04}.{:04}.{:04}            fps|", static_cast<uint64_t>(itr.second->app_id),
                               static_cast<uint64_t>(itr.second->channel_id),
                               static_cast<uint64_t>(itr.second->thread_id));
      }
    }

    if (!ss_header.str().empty() && !ss_data.str().empty()) {
      write_header(logger_, ss_header.str());
      write_log(logger_, ss_data.str());
    }
  }

  {
    std::stringstream ss;
    {
      const std::lock_guard<std::mutex> lock(resource_map_mtx_);
      ss << "Total channels (x2) " << resource_map_.size();
    }

    if (!ss.str().empty()) {
      write_header(summary_logger_, ss.str());
    }
  }
}

void Monitor::write_data_() {
  int64_t current_ts = get_current_time_in_ms();
  int64_t time_diff  = current_ts - last_write_ts_;
  if (time_diff <= 0) {
    return;
  }
  if (do_write_header_) {
    do_write_header_ = false;
    write_header_();
  }

  std::vector<std::tuple<uint64_t, uint64_t, uint64_t>> valid_list;
  std::vector<std::tuple<uint64_t, uint64_t, uint64_t>> invalid_list;

  std::string current_time = Poco::DateTimeFormatter::format(Poco::Timestamp::fromEpochTime(last_write_ts_ / 1000),
                                                             Poco::DateTimeFormat::SORTABLE_FORMAT);
  {
    std::stringstream ss;
    {
      const std::lock_guard<std::mutex> lock(resource_map_mtx_);
      for (auto&& itr : resource_map_) {
        float fps = (static_cast<float>(itr.second->value) - static_cast<float>(itr.second->last_value)) * 1000.0F /
                    static_cast<float>(time_diff);
        itr.second->last_value.store(itr.second->value);

        ss << fmt::format("{} {:>9.{}f}|", current_time, fps, 1);

        if (10 <= fps && 30 >= fps) {
          valid_list.emplace_back(itr.first);
        } else {
          invalid_list.emplace_back(itr.first);
        }
      }
    }

    if (!ss.str().empty()) {
      write_log(logger_, ss.str());
    }
  }

  {
    std::stringstream ss;

    auto valid_list_size = valid_list.size();
    ss << fmt::format("{} Valid   (x2) ({:04}) ", current_time, valid_list_size);
    if (0 < valid_list_size && 100 > valid_list_size) {
      ss << fmt::format(":");
      for (auto&& i : valid_list) {
        ss << fmt::format(" {:04}.{:04}.{:04}|", std::get<0>(i), std::get<1>(i), std::get<2>(i));
      }
    }
    write_log(summary_logger_, ss.str());
  }
  {
    std::stringstream ss;

    auto invalid_list_size = invalid_list.size();
    ss << fmt::format("{} Invalid (x2) ({:04}) ", current_time, invalid_list_size);
    if (0 < invalid_list_size && 100 > invalid_list_size) {
      ss << fmt::format(":");
      for (auto&& i : invalid_list) {
        ss << fmt::format(" {:04}.{:04}.{:04}|", std::get<0>(i), std::get<1>(i), std::get<2>(i));
      }
    }
    write_log(summary_logger_, ss.str());
  }
  {
    std::stringstream ss;
    ss << fmt::format("{} ", current_time);
    ss << "--------------";
    write_log(summary_logger_, ss.str());
  }
  last_write_ts_ = current_ts;
}

void Monitor::run_() {
  int sleep_upto_sec = max_sleep_upto_sec;
  last_write_ts_     = get_current_time_in_ms();

  logger_             = get_logger_st(session_dir_, file_name_);
  summary_logger_     = get_logger_st(session_dir_, fmt::format("{}_summary", file_name_));
  auto last_list_size = resource_map_.size();
  do_write_header_    = true;

  std::chrono::time_point<std::chrono::system_clock> entry_time = std::chrono::system_clock::time_point::min();

  while (!do_shutdown_) {
    if (sleep_upto_sec > 0) {
      sleep_upto_sec--;
    } else {
      sleep_upto_sec = max_sleep_upto_sec;
      {
        const std::lock_guard<std::mutex> lock(resource_map_mtx_);
        if (last_list_size != resource_map_.size()) {
          last_list_size   = resource_map_.size();
          do_write_header_ = true;
        }
      }
      write_data_();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }

  write_data_();
}
