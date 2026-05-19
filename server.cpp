#include "server.hpp"

#include <cstring>
#include <mutex>
#include <sstream>

void Server::listen_update_loop(std::shared_ptr<std::string>& server_json) {
  main_socket.listen_s(5);
  int ret;
  std::string request;
  char buf[4096];
  ssize_t total = 0;
  ssize_t n = 0;

  while (thread_controller->running) {
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
      Socket client = main_socket.accept_s();
      std::string method, path, version, response, json;

      total = 0;
      while (total < (ssize_t)sizeof(buf) - 1) {
        n = client.read_s(buf + total, sizeof(buf) - total - 1);
        if (n == 0) {
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
          std::lock_guard<std::mutex> lock(thread_controller->json_mutex);
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