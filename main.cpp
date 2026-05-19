#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <csignal>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <list>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>

#include "scanner.hpp"
#include "server.hpp"
#include "sys_classes.hpp"
#include "thread_resource_controller.hpp"

namespace fs = std::filesystem;
namespace ch = std::chrono;

std::shared_ptr<ThreadResourceController> global_controller;

void signalHandler(int /*signum*/) {
  if (global_controller) {
    global_controller->running = false;
    global_controller->cv.notify_all();
  }
  std::cout << "\nЗавершение работы..." << std::endl;
}

int main(int argc, char** argv) {
  ch::milliseconds interval_millisec(5000);
  bool use_network = false;
  std::string arg;
  fs::path search_dir(".");

  for (int i = 1; i < argc; ++i) {
    arg = argv[i];
    if (arg == "--path" && i + 1 < argc)
      search_dir = argv[++i];
    else if (arg == "--interval" && i + 1 < argc)
      interval_millisec = ch::milliseconds(std::stoi(argv[++i]));
    else if (arg == "--network")
      use_network = true;
    else {
      std::cout << "Wrong argument! Arguments usage:\n"
                   "  --path P (Path to directory to scan)\n"
                   "  --interval t (Time in milliseconds between scans)\n"
                   "  --network : if used, results are showed on localhost\n"
                << std::endl;
      return 1;
    }
  }

  global_controller = std::make_shared<ThreadResourceController>(true);
  signal(SIGINT, signalHandler);

  std::shared_ptr<std::string> json_ptr =
      use_network ? std::make_shared<std::string>() : nullptr;

  Scanner scanner(interval_millisec, search_dir, global_controller);

  std::cout << "Starting scanning " << fs::absolute(search_dir) << std::endl;
  if (use_network) {
    Server server(std::string("127.0.0.1"), 1234, global_controller);
    std::thread server_thread(&Server::listen_update_loop, &server,
                              std::ref(json_ptr));
    std::thread scanner_thread(&Scanner::scan_loop, &scanner,
                               std::ref(json_ptr));
    scanner_thread.join();
    server_thread.join();
  } else {
    scanner.scan_loop(json_ptr);
  }
}
