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

#include "sys_classes.hpp"

namespace fs = std::filesystem;
namespace ch = std::chrono;
std::mutex cv_mutex;
std::condition_variable cv;

std::atomic<bool> running{true};
std::mutex json_mutex;

const std::unordered_set<std::string> audio_extensions = {
    ".mp3", ".wav", ".ogg", ".flac", ".aac", ".wma", ".m4a"};
const std::unordered_set<std::string> video_extensions = {
    ".mp4", ".mpg", ".mpeg", ".avi", ".mkv", ".mov", ".webm", ".flv", ".wmv"};
const std::unordered_set<std::string> image_extensions = {
    ".jpg", ".jpeg", ".png", ".gif", ".bmp", ".tiff", ".webp", ".svg"};

void signalHandler(int /*signum*/) {
  running = false;
  cv.notify_all();
  std::cout << "\nЗавершение работы..." << std::endl;
}

class Server {
  Socket main_socket;

 public:
  Server() : main_socket{AF_INET, SOCK_STREAM, 0} {
    main_socket.bind_s(AF_INET, 1236, string_to_in_addr_t("127.0.0.1"));
    main_socket.set_socket_option(SO_REUSEADDR, 1);
  }

  ~Server() { main_socket.close_s(); }

  void listen_update_loop(std::shared_ptr<std::string>& server_json) {
    main_socket.listen_s(5);
    int ret;
    Socket client;
    std::string request;
    char buf[4096];
    ssize_t total = 0;
    ssize_t n = 0;

    while (running) {
      ret = main_socket.poll_s(POLLIN, 200);
      if (ret < 0) {
        if (errno == EINTR) {
          continue;
        }
        break;
      }
      if (ret == 0) {
        continue;
      }

      try {
        client = main_socket.accept_s();
        std::string method, path, version, response, json;

        total = 0;
        while (total < (ssize_t)sizeof(buf) - 1) {
          n = client.read_s(buf + total, 4096);
          if (n <= 0) {
            break;
          }
          total += n;
          buf[total] = '\0';
          if (strstr(buf, "\r\n\r\n")) {
            break;
          }
        }

        std::istringstream req_stream(buf);
        req_stream >> method >> path >> version;

        if (method == "GET" && path == "/media_files") {
          {
            std::lock_guard<std::mutex> lock(json_mutex);
            json = *server_json;
          }
          response =
              "HTTP/1.1 200 OK\r\n"
              "Content-Type: application/json\r\n"
              "Content-Length: " +
              std::to_string(json.size()) + "\r\nConnection: close\r\n\r\n" +
              json;
        } else {
          response =
              "HTTP/1.1 404 Not Found\r\n"
              "Content-Length: 0\r\n"
              "Connection: close\r\n\r\n";
        }
        client.writeAll((void*)response.c_str(), response.size());
      } catch (...) {
      }
    }
  }
};

class Scanner {
  ch::milliseconds interval_duration;
  fs::path dir;
  std::list<fs::path> video_paths{};
  std::list<fs::path> images_paths{};
  std::list<fs::path> audio_paths{};

  static std::string to_lower(const std::string& s) {
    std::string res = s;
    std::transform(res.begin(), res.end(), res.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return res;
  }

  char get_multimedia_type(const fs::path& file) {
    std::string s = to_lower(file.extension().string());
    for (const auto& ext : audio_extensions) {
      if (s.ends_with(ext)) {
        return 'a';
      }
    }
    for (const auto& ext : video_extensions) {
      if (s.ends_with(ext)) {
        return 'v';
      }
    }
    for (const auto& ext : image_extensions) {
      if (s.ends_with(ext)) {
        return 'i';
      }
    }
    return ' ';
  }

 public:
  Scanner(const ch::milliseconds check_duration, const fs::path check_directory)
      : interval_duration{check_duration}, dir{check_directory} {}

  void scan_directory() {
    video_paths.clear();
    images_paths.clear();
    audio_paths.clear();

    char multimedia_type;
    fs::path filepath;
    for (const auto& elem : fs::recursive_directory_iterator(
             dir, fs::directory_options::skip_permission_denied)) {
      if (!running) {
        return;
      }
      if (!elem.is_regular_file()) {
        continue;
      }
      filepath = elem.path();

      multimedia_type = get_multimedia_type(filepath);
      switch (multimedia_type) {
        case ' ':
          continue;
        case 'v':
          video_paths.push_back(filepath);
          break;
        case 'i':
          images_paths.push_back(filepath);
          break;
        case 'a':
          audio_paths.push_back(filepath);
          break;
      }
    }
  }

  static std::string list_to_json(std::list<fs::path> items) {
    std::ostringstream oss;
    std::string filename;
    size_t pos;

    oss << "[";
    auto iter = items.begin();
    while (iter != items.end()) {
      if (iter != items.begin()) {
        oss << ", ";
      }
      filename = iter->filename().string();
      pos = 0;
      while ((pos = filename.find_first_of("\"\\", pos)) != std::string::npos) {
        filename.insert(pos, "\\");
        pos += 2;
      }
      oss << "\"" << filename << "\"";
      iter++;
    }
    oss << "]";
    return oss.str();
  }

  static std::string build_json(const std::list<fs::path>& audio,
                                const std::list<fs::path>& video,
                                const std::list<fs::path>& images) {
    std::ostringstream json;
    json << "{\n";
    json << "  \"audio\": " << list_to_json(audio) << ",\n";
    json << "  \"video\": " << list_to_json(video) << ",\n";
    json << "  \"images\": " << list_to_json(images) << "\n";
    json << "}\n";
    return json.str();
  }

  void write_json_to_file() {
    const char* home_dir = getenv("HOME");
    if (!home_dir) {
      std::cout << "Can't get HOME directory!" << std::endl;
      return;
    }
    fs::path filepath = fs::path(home_dir) / ".media_files";

    try {
      std::ofstream ofs(filepath);
      if (!ofs) {
        std::cerr << "Can't open file for writing: " << filepath << std::endl;
        return;
      }
      ofs << build_json(audio_paths, video_paths, images_paths);
    } catch (const std::exception& e) {
      std::cerr << "Error with writing file: " << e.what() << std::endl;
    }
  }

  void scan_loop(std::shared_ptr<std::string>& server_json) {
    ch::steady_clock::time_point start_time;
    ch::duration<double> scan_duration;

    while (running) {
      start_time = ch::steady_clock::now();
      scan_directory();
      if (!running) {
        return;
      }
      std::string json = build_json(audio_paths, video_paths, images_paths);

      if (server_json != nullptr) {
        std::lock_guard<std::mutex> lock(json_mutex);
        *server_json = std::move(json);
      } else {
        write_json_to_file();
      }

      scan_duration = ch::steady_clock::now() - start_time;
      if (scan_duration < interval_duration) {
        std::unique_lock<std::mutex> lock(cv_mutex);
        cv.wait_for(lock, interval_duration - scan_duration,
                    [] { return !running; });
      }
    }
  }
};

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

  signal(SIGINT, signalHandler);
  std::shared_ptr<std::string> json_ptr =
      use_network ? std::make_shared<std::string>() : nullptr;

  Scanner scanner(interval_millisec, search_dir);

  std::cout << "Starting scanning " << fs::absolute(search_dir) << std::endl;
  if (use_network) {
    Server server;
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
