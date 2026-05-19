#ifndef __SERVER_HPP__
#define __SERVER_HPP__

#include <memory>

#include "sys_classes.hpp"
#include "thread_resource_controller.hpp"

class Server {
  Socket main_socket;
  std::shared_ptr<ThreadResourceController> thread_controller;

 public:
  Server(std::string address, int port,
         std::shared_ptr<ThreadResourceController>& controller)
      : main_socket{AF_INET, SOCK_STREAM, 0}, thread_controller{controller} {
    main_socket.set_socket_option(SO_REUSEADDR, 1);
    main_socket.bind_s(AF_INET, port, string_to_in_addr_t(address));
  }

  ~Server() { main_socket.close_s(); }

  void listen_update_loop(std::shared_ptr<std::string>& server_json);
};

#endif