#ifndef __THREAD_CONTROLLER_HPP__
#define __THREAD_CONTROLLER_HPP__

#include <condition_variable>
#include <mutex>

struct ThreadResourceController {
  std::mutex cv_mutex{};
  std::condition_variable cv{};

  std::atomic<bool> running;
  std::mutex json_mutex{};

  ThreadResourceController(bool is_running) : running{is_running} {}
};

#endif