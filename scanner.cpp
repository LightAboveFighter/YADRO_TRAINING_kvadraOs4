#include "scanner.hpp"

#include <algorithm>
#include <condition_variable>
#include <fstream>
#include <iostream>
#include <mutex>

std::string Scanner::to_lower(const std::string& s) {
  std::string res = s;
  std::transform(res.begin(), res.end(), res.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return res;
}

char Scanner::get_multimedia_type(const fs::path& file) {
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

void Scanner::scan_directory() {
  video_paths.clear();
  images_paths.clear();
  audio_paths.clear();

  char multimedia_type;
  fs::path filepath;
  for (const auto& elem : fs::recursive_directory_iterator(
           dir, fs::directory_options::skip_permission_denied)) {
    if (!thread_controller->running) {
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

std::string Scanner::list_to_json(std::list<fs::path> items) {
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

std::string Scanner::build_json(const std::list<fs::path>& audio,
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

void Scanner::write_json_to_file() {
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

void Scanner::scan_loop(std::shared_ptr<std::string>& server_json) {
  ch::steady_clock::time_point start_time;
  ch::duration<double> scan_duration;

  while (thread_controller->running) {
    start_time = ch::steady_clock::now();
    scan_directory();
    if (!thread_controller->running) {
      return;
    }
    std::string json = build_json(audio_paths, video_paths, images_paths);

    if (server_json != nullptr) {
      std::lock_guard<std::mutex> lock(thread_controller->json_mutex);
      *server_json = std::move(json);
    } else {
      write_json_to_file();
    }

    scan_duration = ch::steady_clock::now() - start_time;
    if (scan_duration < interval_duration) {
      std::unique_lock<std::mutex> lock(thread_controller->cv_mutex);
      thread_controller->cv.wait_for(
          lock, interval_duration - scan_duration,
          [this] { return !thread_controller->running; });
    }
  }
}
