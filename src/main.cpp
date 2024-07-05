// *****************************************************
//    Copyright 2023 Videonetics Technology Pvt Ltd
// *****************************************************

#include "fps_monitor.h"
#include <atomic>
#include <chrono>
#include <functional>
#include <iostream>
#include <thread>
#include <vector>

void run(std::atomic_uint_fast64_t& monitor_set_status, int millis) {
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
  int limit = 30000000;
  while (limit-- > 0) {
    monitor_set_status++;
    std::this_thread::sleep_for(std::chrono::milliseconds(millis));
  }
}

auto main1(int /*argc*/, char const* /*argv*/[]) -> int {
  std::cout << "Started\n";

  FpsMonitor::getInstance();
  std::vector<std::thread> threads;

  const int thread_per_group{1};

  for (int i = 1; i <= (3 * thread_per_group); i++) {
    if (i <= thread_per_group) {
      // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
      threads.emplace_back(run, std::ref(FpsMonitor::set_status(0, i, 0)), 200);
      // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
      threads.emplace_back(run, std::ref(FpsMonitor::set_status(0, i, 1)), 100);
    } else if (i > thread_per_group && i <= (2 * thread_per_group)) {
      // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
      threads.emplace_back(run, std::ref(FpsMonitor::set_status(0, i, 0)), 50);
      // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
      threads.emplace_back(run, std::ref(FpsMonitor::set_status(0, i, 1)), 40);
    } else if (i > (2 * thread_per_group) && i <= (3 * thread_per_group)) {
      // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
      threads.emplace_back(run, std::ref(FpsMonitor::set_status(0, i, 0)), 25);
      // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
      threads.emplace_back(run, std::ref(FpsMonitor::set_status(0, i, 1)), 20);
    }
  }

  for (auto&& thread : threads) {
    if (thread.joinable()) {
      thread.join();
    }
  }
  std::cout << "Stopped\n";

  return 0;
}

auto main(int /*argc*/, char const* /*argv*/[]) -> int {
  std::cout << "Hello World\n";
  FpsMonitor::getInstance("session", "fps_test");

  // auto a = FpsMonitor::giveMyUniqueId("rtsp://1");
  // auto b = FpsMonitor::giveMyUniqueId("rtsp://2");
  // auto c = FpsMonitor::giveMyUniqueId("rtsp://1");
  // std::cout << "a: " << a.to_string() << '\n';
  // std::cout << "b: " << b.to_string() << '\n';
  // std::cout << "c: " << c.to_string() << '\n';
  // FpsMonitor::removeMyUniqueId(a);
  // FpsMonitor::removeMyUniqueId(c);
  // auto d = FpsMonitor::giveMyUniqueId("rtsp://1");
  // assert(d == a);
  return 0;
}