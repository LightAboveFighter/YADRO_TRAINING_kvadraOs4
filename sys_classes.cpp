#include "sys_classes.hpp"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <exception>
#include <iostream>
#include <system_error>

Socket::Socket(int domain, int type, int protocol) {
  fd = socket(domain, type, protocol);
  if (fd < 0) throw std::system_error(errno, std::generic_category(), "socket");
}

Socket::Socket(int socket_descriptor) {
  fd = socket_descriptor;

  sock_addr.sin_family = -1;
  sock_addr.sin_port = htons(-1);
  sock_addr.sin_addr.s_addr = htonl(-1);
  if (fd < 0) throw std::system_error(errno, std::generic_category(), "socket");
}

Socket::~Socket() { close_s(); }

Socket::Socket(Socket&& other) noexcept : fd{other.fd} { other.fd = -1; }

Socket& Socket::operator=(Socket&& other) noexcept {
  std::swap(fd, other.fd);
  return *this;
}

int Socket::get_f_descriptor() const { return fd; }

int Socket::get_address_size() const { return sizeof(sock_addr); }

int Socket::get_address_port() const { return ntohl(sock_addr.sin_port); }

void Socket::connect_s(sa_family_t sin_family, in_port_t sin_port,
                       in_addr_t sin_addr) {
  sock_addr.sin_family = sin_family;
  sock_addr.sin_port = htons(sin_port);
  sock_addr.sin_addr.s_addr = htonl(sin_addr);

  if (connect(fd, (const sockaddr*)&sock_addr, get_address_size()) < 0) {
    throw std::system_error(errno, std::generic_category(), "connect");
  }
}

void Socket::bind_s(sa_family_t sin_family, in_port_t sin_port,
                    in_addr_t sin_addr) {
  sock_addr.sin_family = sin_family;
  sock_addr.sin_port = htons(sin_port);
  sock_addr.sin_addr.s_addr = htonl(sin_addr);

  if (bind(fd, (const sockaddr*)&sock_addr, get_address_size()) < 0) {
    throw std::system_error(errno, std::generic_category(), "bind");
  }
}

void Socket::writeAll(void* buffer, size_t n) const {
  char* arr = static_cast<char*>(buffer);
  size_t remaining = n;
  int written_bytes;
  while (remaining != 0) {
    written_bytes = write(fd, arr, remaining);
    if (written_bytes <= 0) {
      if (errno == EINTR) continue;
      throw std::system_error(errno, std::generic_category(), "write");
    }
    arr += written_bytes;
    remaining -= written_bytes;
  }
}

void Socket::readAll(void* buffer, size_t n) const {
  char* arr = static_cast<char*>(buffer);
  size_t remaining = n;
  int read_bytes;
  while (remaining != 0) {
    read_bytes = read(fd, arr, remaining);
    if (read_bytes <= 0) {
      if (errno == EINTR) continue;
      throw std::system_error(errno, std::generic_category(), "read");
    }
    arr += read_bytes;
    remaining -= read_bytes;
  }
}

int Socket::read_s(void* buffer, size_t n) const {
  int read_bytes;
  read_bytes = read(fd, buffer, n);
  if (read_bytes <= 0) {
    throw std::system_error(errno, std::generic_category(), "read");
  }
  return read_bytes;
}

void Socket::listen_s(size_t n_connections) const {
  if (listen(fd, n_connections) < 0) {
    throw std::system_error(errno, std::generic_category(), "listen");
  }
}

int Socket::poll_s(short events, int timeout) {
  struct pollfd poll_struct;
  poll_struct.fd = fd;
  poll_struct.events = events;
  poll_struct.revents = 0;
  return poll(&poll_struct, 1, timeout);
}

template <>
void Socket::set_socket_option(int option, const int& optval) {
  if (setsockopt(fd, SOL_SOCKET, option, &optval, sizeof(optval)) < 0) {
    throw std::system_error(errno, std::generic_category(), "setsockopt");
  }
}

template <>
void Socket::set_socket_option(int option, const struct timeval& optval) {
  if (setsockopt(fd, SOL_SOCKET, option, &optval, sizeof(optval)) < 0) {
    throw std::system_error(errno, std::generic_category(), "setsockopt");
  }
}

in_addr_t Socket::get_peer_addr() const {
  struct sockaddr_in peer;
  socklen_t len = sizeof(peer);
  if (getpeername(fd, (struct sockaddr*)&peer, &len) == -1) {
    throw std::system_error(errno, std::generic_category(), "getpeername");
  }
  return peer.sin_addr.s_addr;
}

Socket Socket::accept_s() const {
  struct sockaddr incoming_addr;
  socklen_t incoming_addr_len = sizeof(incoming_addr);
  int incoming_socket_descriptor =
      accept(fd, &incoming_addr, &incoming_addr_len);
  if (incoming_socket_descriptor < 0) {
    throw std::system_error(errno, std::generic_category(), "listen");
  }
  return Socket(incoming_socket_descriptor);
}

Socket::operator bool() const { return fd != -1; }

void Socket::close_s() {
  if (fd != -1) {
    close(fd);
    fd = -1;
  }
}

/*Convert from string format of an Internet number to the binary host
 * format*/
in_addr_t string_to_in_addr_t(std::string address) {
  in_addr_t ret;
  if (inet_pton(AF_INET, address.c_str(), &ret) == 1) {
    return ntohl(ret);
  } else {
    return -1;
  }
}