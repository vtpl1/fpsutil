// *****************************************************
//    Copyright 2023 Videonetics Technology Pvt Ltd
// *****************************************************

#include "fps_counter.h"
#include "fps_monitor.h"
#include <atomic>
#include <chrono>
#include <functional>
#include <iostream>
#include <thread>
#include <vector>

void run(std::atomic_uint_fast64_t& monitor_set_status, int millis) {
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
  int limit = 3000;
  while (limit-- > 0) {
    monitor_set_status++;
    std::this_thread::sleep_for(std::chrono::milliseconds(millis));
  }
}

auto main(int /*argc*/, char const* /*argv*/[]) -> int {
  std::cout << "Started\n";

  FpsMonitor::getInstance("session", "fps1");
  std::vector<std::thread> threads;

  const int thread_per_group{10};

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
  FpsMonitor::close();
  return 0;
}

auto main2(int /*argc*/, char const* /*argv*/[]) -> int {
  std::cout << "Hello World\n";
  FpsMonitor::getInstance("session", "fps_test");

  FpsMonitor::close();
  return 0;
}

auto main3(int /*argc*/, char const* /*argv*/[]) -> int {
  std::cout << "Hello World\n";
  int        count = 100;
  // int64_t    ts    = 0;
  FpsCounter fps_counter;
  while (count--) {
    fps_counter.set_status(1024);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::cout << "FPS: " << fps_counter.get_fps() << std::endl;
    // ts += 1000;
  }
  std::cout << "FPS1: " << fps_counter.get_fps() << std::endl;

  return 0;
}