#ifndef __SCANNER_HPP__
#define __SCANNER_HPP__

#include <chrono>
#include <filesystem>
#include <list>
#include <memory>
#include <unordered_set>

#include "thread_resource_controller.hpp"

namespace fs = std::filesystem;
namespace ch = std::chrono;

class Scanner {
  const std::unordered_set<std::string> audio_extensions = {
      ".mp3", ".wav", ".ogg", ".flac", ".aac", ".wma", ".m4a"};
  const std::unordered_set<std::string> video_extensions = {
      ".mp4", ".mpg", ".mpeg", ".avi", ".mkv", ".mov", ".webm", ".flv", ".wmv"};
  const std::unordered_set<std::string> image_extensions = {
      ".jpg", ".jpeg", ".png", ".gif", ".bmp", ".tiff", ".webp", ".svg"};

  ch::milliseconds interval_duration;
  fs::path dir;
  std::list<fs::path> video_paths{};
  std::list<fs::path> images_paths{};
  std::list<fs::path> audio_paths{};
  std::shared_ptr<ThreadResourceController> thread_controller;

  static std::string to_lower(const std::string& s);

  char get_multimedia_type(const fs::path& file);

 public:
  Scanner(const ch::milliseconds check_duration, const fs::path check_directory,
          std::shared_ptr<ThreadResourceController>& controller)
      : interval_duration{check_duration},
        dir{check_directory},
        thread_controller{controller} {}

  void scan_directory();

  static std::string list_to_json(std::list<fs::path> items);

  static std::string build_json(const std::list<fs::path>& audio,
                                const std::list<fs::path>& video,
                                const std::list<fs::path>& images);

  void write_json_to_file();

  void scan_loop(std::shared_ptr<std::string>& server_json);
};

#endif