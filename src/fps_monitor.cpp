// *****************************************************
//  Copyright 2023 Videonetics Technology Pvt Ltd
// *****************************************************

#include "fps_monitor.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <fmt/chrono.h>
#include <fmt/core.h>
// #include <logging.h>
#include <memory>
#include <mutex>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/spdlog.h>
#include <sstream>
#include <string>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

// NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
static_assert(__cplusplus >= 201703L, "This file expects a C++17 compatible compiler.");

constexpr int32_t LOG_INTERVAL_SEC         = 10;
constexpr int32_t MAX_VALID_LIST_SIZE      = 100;
constexpr int32_t MIN_ABNORMAL_FPS         = 8;
constexpr int32_t MAX_ABNORMAL_FPS         = 1000;
constexpr float   MILLI_SECONDS_IN_SECONDS = 1000.0F;
constexpr int32_t STRFTIME_FORMAT_LENGTH   = 20;
namespace {
constexpr int max_size      = 1048576 * 5;
constexpr int max_files     = 3;
constexpr int banner_spaces = 80;
std::string   get_current_time_str() {
  const std::time_t t = std::time(nullptr);
  std::stringstream ss;
  ss << fmt::format("UTC: {:%Y-%m-%d %H:%M:%S}", fmt::gmtime(t));
  return ss.str();
}
std::string printable_current_time() {
  return fmt::format("\n┌{0:─^{2}}┐\n"
                     "│{1: ^{2}}│\n"
                     "└{0:─^{2}}┘\n",
                     "", get_current_time_str(), banner_spaces);
}

void write_header(std::shared_ptr<spdlog::logger> logger, const std::string& header_msg) {
  if (logger) {
    logger->info(printable_current_time());
    logger->info(header_msg);
  }
}
void write_log(std::shared_ptr<spdlog::logger> logger, const std::string& log_msg) {
  if (logger) {
    logger->info(log_msg);
    logger->flush();
  }
}
std::shared_ptr<spdlog::logger> get_logger_st_internal(const std::string& logger_name, const std::string& logger_path) {
  std::shared_ptr<spdlog::logger> logger = spdlog::get(logger_name);
  if (logger == nullptr) {
    logger = spdlog::rotating_logger_st(logger_name, logger_path, max_size, max_files);
    logger->set_pattern("%v");
  }
  return logger;
}
std::shared_ptr<spdlog::logger> get_logger_st(const std::string& session_folder, const std::string& base_name,
                                              int16_t channel_id = 0, int16_t app_id = 0) {
  bool enable_logging = true;

  std::string logger_name = fmt::format("{}", base_name);
  {
    //     std::stringstream base_path_cnf;
    //     base_path_cnf << session_folder;
    // #if defined(_WIN32)
    //     base_path_cnf << "\\";
    // #else
    //     base_path_cnf << "/";
    // #endif
    //     base_path_cnf << logger_name;
    //     base_path_cnf << ".cnf";
    //     // Poco::Path base_path_cnf(session_folder);
    //     // base_path_cnf.append(fmt::format("{}.cnf", logger_name));
    //     ConfigFile f(base_path_cnf.str());
    //     auto       d = static_cast<double>(f.Value(base_name, logger_name, 1.0));
    //     if (d > 0) {
    //       enable_logging = true;
    //     }
    //     if ((channel_id != 0) || (app_id != 0)) {
    //       logger_name = fmt::format("{}_{}_{}", logger_name, channel_id, app_id);
    //     }
    //     d = static_cast<double>(f.Value(base_name, logger_name, 1.0));
    //     if (d > 0) {
    //       enable_logging = true;
    //     }
  }
  if (enable_logging) {
    std::stringstream base_path_log;
    base_path_log << session_folder;
#if defined(_WIN32)
    base_path_log << "\\";
#else
    base_path_log << "/";
#endif
    base_path_log << logger_name;
    base_path_log << ".log";
    // Poco::Path base_path_log(session_folder);
    // base_path_log.append(fmt::format("{}.log", logger_name));
    return get_logger_st_internal(logger_name, base_path_log.str());
  }
  return nullptr;
}
} // namespace

FpsStatus::FpsStatus(uint64_t app_id, uint64_t channel_id, uint64_t thread_id, bool dump_in_log)
    : FpsStatus(app_id, channel_id, thread_id, 0, 0, dump_in_log) {}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
FpsStatus::FpsStatus(uint64_t app_id, uint64_t channel_id, uint64_t thread_id, uint64_t value, uint64_t last_value,
                     bool dump_in_log)
    : app_id(app_id), channel_id(channel_id), thread_id(thread_id), value(value), last_value(last_value),
      dump_in_log(dump_in_log), last_fps(0.0) {}

FpsMonitor::FpsMonitor(std::string session_dir, std::string file_name)
    : session_dir_(std::move(session_dir)), file_name_(std::move(file_name)), last_write_ts_(0) {
  thread_ = std::make_unique<std::thread>(&FpsMonitor::run_, this);
}

void FpsMonitor::shutDown() {
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
FpsMonitor::~FpsMonitor() { shutDown(); }

auto FpsMonitor::getInstance() -> FpsMonitor& { return getInstance("session", "fps_common"); }
auto FpsMonitor::close() -> void { getInstance().shutDown(); }

auto FpsMonitor::getInstance(std::string session_dir, std::string file_name) -> FpsMonitor& {
  static FpsMonitor instance(std::move(session_dir), std::move(file_name));
  return instance;
}

auto FpsMonitor::set_status(uint64_t app_id, uint64_t channel_id, uint64_t thread_id, bool dump_in_log)
    -> std::atomic_uint_fast64_t& {
  return FpsMonitor::getInstance().set_status_(app_id, channel_id, thread_id, dump_in_log);
}

auto FpsMonitor::set_status_(uint64_t app_id, uint64_t channel_id, uint64_t thread_id, bool dump_in_log)
    -> std::atomic_uint_fast64_t& {
  const std::lock_guard<std::mutex> lock(resource_map_mtx_);

  auto key = std::tuple<uint64_t, uint64_t, uint64_t>(app_id, channel_id, thread_id);
  auto itr = resource_map_.find(key);
  if (itr != resource_map_.end()) {
    itr->second->value++;
    itr->second->dump_in_log = dump_in_log;
    return itr->second->value;
  }

  auto map =
      resource_map_.emplace(key, std::move(std::make_unique<FpsStatus>(app_id, channel_id, thread_id, dump_in_log)));
  return map.first->second->value;
}

void FpsMonitor::write_header_(std::shared_ptr<spdlog::logger> logger_,
                               std::shared_ptr<spdlog::logger> summary_logger_) {
  {
    std::stringstream ss_header;
    std::stringstream ss_data;
    const std::time_t time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

    char time_buf[STRFTIME_FORMAT_LENGTH]; // NOLINT
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
    strftime(time_buf, STRFTIME_FORMAT_LENGTH, "%y-%m-%d", localtime(&time));
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
    std::string const current_time(time_buf);

    {
      const std::lock_guard<std::mutex> lock(resource_map_mtx_);
      for (auto&& itr : resource_map_) {
        if (itr.second->dump_in_log) {
          ss_header << "Time     App .Chn .Thr        Fps|";
          ss_data << fmt::format("{} {:04}.{:04}.{:04}       fps|", current_time, itr.second->app_id.load(),
                                 itr.second->channel_id.load(), itr.second->thread_id.load());
        }
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

void FpsMonitor::calculate_fps_() {
  const int64_t current_ts = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now())
                                 .time_since_epoch()
                                 .count();

  const int64_t time_diff = current_ts - last_write_ts_;
  if (time_diff <= 0) {
    return;
  }
  const std::lock_guard<std::mutex> lock(resource_map_mtx_);
  for (auto&& itr : resource_map_) {
    auto value_diff = itr.second->value - itr.second->last_value;
    auto fps        = static_cast<float>(value_diff) * MILLI_SECONDS_IN_SECONDS / static_cast<float>(time_diff);
    itr.second->last_value.store(itr.second->value);
    itr.second->last_fps = fps;
  }
  last_write_ts_ = current_ts;
}

void FpsMonitor::write_data_(std::shared_ptr<spdlog::logger> logger_, std::shared_ptr<spdlog::logger> summary_logger_) {
  calculate_fps_();
  if (do_write_header_) {
    do_write_header_ = false;
    write_header_(logger_, summary_logger_);
  }

  std::vector<std::tuple<uint64_t, uint64_t, uint64_t>> valid_list;
  std::vector<std::tuple<uint64_t, uint64_t, uint64_t>> invalid_list;

  const std::time_t time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

  char time_buf[STRFTIME_FORMAT_LENGTH]; // NOLINT
  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
  strftime(time_buf, STRFTIME_FORMAT_LENGTH, "%H:%M:%S", localtime(&time));
  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
  std::string const current_time(time_buf);

  {
    std::stringstream ss_data;
    {
      const std::lock_guard<std::mutex> lock(resource_map_mtx_);
      for (auto&& itr : resource_map_) {
        if (itr.second->dump_in_log) {

          auto fps        = itr.second->last_fps.load();
          auto app_id     = itr.second->app_id.load();
          auto channel_id = itr.second->channel_id.load();
          auto thread_id  = itr.second->thread_id.load();
          ss_data << fmt::format("{} {:04}.{:04}.{:04} {:>9.{}f}|", current_time, app_id, channel_id, thread_id, fps,
                                 1);

          if (MIN_ABNORMAL_FPS <= fps && MAX_ABNORMAL_FPS >= fps) {
            valid_list.emplace_back(itr.first);
          } else {
            invalid_list.emplace_back(itr.first);
          }
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
    if (0 < valid_list_size && MAX_VALID_LIST_SIZE > valid_list_size) {
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
    if (0 < invalid_list_size && MAX_VALID_LIST_SIZE > invalid_list_size) {
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
}

void FpsMonitor::run_() {
  int sleep_upto_sec = LOG_INTERVAL_SEC;

  std::shared_ptr<spdlog::logger> logger_         = get_logger_st(session_dir_, file_name_);
  std::shared_ptr<spdlog::logger> summary_logger_ = get_logger_st(session_dir_, fmt::format("{}_summary", file_name_));

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
      write_data_(logger_, summary_logger_);
    }
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  write_data_(logger_, summary_logger_);
}

std::atomic<float>& FpsMonitor::get_fps_(uint64_t app_id, uint64_t channel_id, uint64_t thread_id) {
  const std::lock_guard<std::mutex> lock(resource_map_mtx_);

  auto key = std::tuple<uint64_t, uint64_t, uint64_t>(app_id, channel_id, thread_id);
  auto itr = resource_map_.find(key);
  if (itr != resource_map_.end()) {
    return itr->second->last_fps;
  }
  auto map = resource_map_.emplace(key, std::move(std::make_unique<FpsStatus>(app_id, channel_id, thread_id, false)));
  return map.first->second->last_fps;
}

std::atomic<float>& FpsMonitor::get_fps(uint64_t app_id, uint64_t channel_id, uint64_t thread_id) {
  return FpsMonitor::getInstance().get_fps_(app_id, channel_id, thread_id);
}
