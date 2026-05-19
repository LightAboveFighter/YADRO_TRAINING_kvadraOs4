#ifndef __SYS_CLASSES_HPP__
#define __SYS_CLASSES_HPP__

#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <filesystem>
#include <string>
#include <vector>

class Socket {
  int fd = -1;
  struct sockaddr_in sock_addr {};

 public:
  Socket() {}

  Socket(int domain, int type, int protocol);

  Socket(int socket_descriptor);

  ~Socket();

  Socket(Socket&& other) noexcept;

  Socket& operator=(Socket&& other) noexcept;

  Socket(const Socket&) = delete;
  Socket& operator=(const Socket&) = delete;

  int get_f_descriptor() const;

  int get_address_size() const;

  int get_address_port() const;

  void connect_s(sa_family_t sin_family, in_port_t sin_port,
                 in_addr_t sin_addr);

  void bind_s(sa_family_t sin_family, in_port_t sin_port, in_addr_t sin_addr);

  void writeAll(void* buffer, size_t n) const;

  void readAll(void* buffer, size_t n) const;

  int read_s(void* buffer, size_t n) const;

  void listen_s(size_t n_connections) const;

  int poll_s(short events, int timeout);

  template <typename T>
  void set_socket_option(int option, const T& optval);

  in_addr_t get_peer_addr() const;

  Socket accept_s() const;

  explicit operator bool() const;

  void close_s();
};

in_addr_t string_to_in_addr_t(std::string address);

#endif