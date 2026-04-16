/*
 * Copyright ©2025 Chris Thachuk & Naomi Alterman.  All rights reserved.
 * Permission is hereby granted to students registered for University of
 * Washington CSE 333 for use solely during Autumn Quarter 2025 for
 * purposes of the course.  No other use, copying, distribution, or
 * modification is permitted without prior written consent. Copyrights
 * for third-party components of this work must be honored.  Instructors
 * interested in reusing these course materials should contact the
 * author.
 */

#include <stdio.h>       // for snprintf()
#include <unistd.h>      // for close(), fcntl()
#include <sys/types.h>   // for socket(), getaddrinfo(), etc.
#include <sys/socket.h>  // for socket(), getaddrinfo(), etc.
#include <arpa/inet.h>   // for inet_ntop()
#include <netdb.h>       // for getaddrinfo()
#include <errno.h>       // for errno, used by strerror()
#include <string.h>      // for memset, strerror()
#include <iostream>      // for std::cerr, etc.

#include "./ServerSocket.h"
#define MAX_PORT_LEN 5

extern "C" {
  #include "libhw1/CSE333.h"
}

using std::string;

namespace hw4 {

ServerSocket::ServerSocket(uint16_t port) {
  port_ = port;
  listen_sock_fd_ = -1;
}

ServerSocket::~ServerSocket() {
  // Close the listening socket if it's not zero.  The rest of this
  // class will make sure to zero out the socket if it is closed
  // elsewhere.
  if (listen_sock_fd_ != -1)
    close(listen_sock_fd_);
  listen_sock_fd_ = -1;
}

bool ServerSocket::BindAndListen(int ai_family, int *const listen_fd) {
  // Use "getaddrinfo," "socket," "bind," and "listen" to
  // create a listening socket on port port_.  Return the
  // listening socket through the output parameter "listen_fd"
  // and set the ServerSocket data member "listen_sock_fd_"

  // STEP 1:

  // set up hints for getaddrinfo
  sock_family_ = ai_family;
  struct addrinfo hints;
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = sock_family_;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;
  hints.ai_flags |= AI_V4MAPPED;
  hints.ai_protocol = IPPROTO_TCP;
  hints.ai_canonname = nullptr;
  hints.ai_addr = nullptr;
  hints.ai_next = nullptr;

  // find server's addrinfo
  struct addrinfo *result;
  char port[MAX_PORT_LEN + 1];
  snprintf(port, MAX_PORT_LEN + 1, "%u", port_);
  int res = getaddrinfo(nullptr, port, &hints, &result);
  if (res != 0) {
    std::cerr << "getaddrinfo failed!" << std::endl;
    return false;
  }

  // go through each result and attempt to listen from each one
  listen_sock_fd_ = -1;
  for (struct addrinfo *rp = result; rp != nullptr; rp = rp->ai_next) {
    listen_sock_fd_ = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (listen_sock_fd_ == -1) {
      std::cerr << "socket creation failed";
      continue;
    }

    int optval = 1;
    setsockopt(listen_sock_fd_, SOL_SOCKET, SO_REUSEADDR,
               &optval, sizeof(optval));

    if (bind(listen_sock_fd_, rp->ai_addr, rp->ai_addrlen) == 0) {
      break;
    }

    close(listen_sock_fd_);
    listen_sock_fd_ = -1;
  }

  freeaddrinfo(result);
  if (listen_sock_fd_ == -1) {
    return false;
  }

  if (listen(listen_sock_fd_, SOMAXCONN) != 0) {
    close(listen_sock_fd_);
    listen_sock_fd_ = -1;
    return false;
  }

  *listen_fd = listen_sock_fd_;
  return true;
}

bool ServerSocket::Accept(int *const accepted_fd,
                          string *const client_addr,
                          uint16_t *const client_port,
                          string *const client_dns_name,
                          string *const server_addr,
                          string *const server_dns_name) const {
  // STEP 2:
  struct sockaddr_storage caddr;
  socklen_t caddr_len = sizeof(caddr);
  while (1) {
    *accepted_fd = accept(listen_sock_fd_,
                          reinterpret_cast<struct sockaddr *>(&caddr),
                          &caddr_len);
    if (*accepted_fd < 0) {
      if ((errno == EINTR) || (errno == EAGAIN) || (errno = EWOULDBLOCK)) {
        continue;
      }
      std::cerr << "Failure on accept" << std::endl;
      close(listen_sock_fd_);
      return false;
    } else {
      break;
    }
  }

  // save client address and port
  char server_name[1024];
  server_name[0] = '\0';
  struct sockaddr *addr = reinterpret_cast<struct sockaddr *> (&caddr);
  if (addr->sa_family == AF_INET) {
    // IPv4

    // get client address
    struct sockaddr_in *in4 = reinterpret_cast<sockaddr_in *>(addr);
    char c_addr4[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(in4->sin_addr), c_addr4, INET_ADDRSTRLEN);
    *client_addr = string(c_addr4);

    // get port
    *client_port = ntohs(in4->sin_port);

    // get server address
    struct sockaddr_in srvr;
    socklen_t srvrlen = sizeof(srvr);
    char addrbuf[INET_ADDRSTRLEN];
    getsockname(*accepted_fd,
                reinterpret_cast<struct sockaddr *>(&srvr),
                &srvrlen);
    inet_ntop(AF_INET, &srvr.sin_addr, addrbuf, INET_ADDRSTRLEN);
    *server_addr = string(addrbuf);

    // get server name/DNS
    getnameinfo(reinterpret_cast<struct sockaddr *>(&srvr),
                srvrlen, server_name, 1024, nullptr, 0, 0);
    *server_dns_name = string(server_name);
  } else if (addr->sa_family == AF_INET6) {
    // IPv6

    // get client address
    struct sockaddr_in6 *in6 = reinterpret_cast<sockaddr_in6 *>(addr);
    char c_addr6[INET6_ADDRSTRLEN];
    inet_ntop(AF_INET6, &(in6->sin6_addr), c_addr6, INET6_ADDRSTRLEN);
    *client_addr = string(c_addr6);

    // get port
    *client_port = ntohs(in6->sin6_port);

    // get server address
    struct sockaddr_in6 srvr;
    socklen_t srvrlen = sizeof(srvr);
    char addrbuf[INET6_ADDRSTRLEN];
    getsockname(*accepted_fd,
                reinterpret_cast<struct sockaddr *>(&srvr),
                &srvrlen);
    inet_ntop(AF_INET6, &srvr.sin6_addr, addrbuf, INET6_ADDRSTRLEN);
    *server_addr = string(addrbuf);

    // get server name/DNS
    getnameinfo(reinterpret_cast<struct sockaddr *>(&srvr),
                srvrlen, server_name, 1024, nullptr, 0, 0);
    *server_dns_name = string(server_name);
  } else {
    std::cerr << "Cannot determine IPv4 or IPv6" << std::endl;
    close(listen_sock_fd_);
    return false;
  }

  // get DNS
  char client_dns[1024];
  if (getnameinfo(addr, caddr_len, client_dns, 1024, nullptr, 0, 0) != 0) {
    std::cerr << "Getting DNS Failed" << std::endl;
    close(listen_sock_fd_);
    return false;
  }
  *client_dns_name = string(client_dns);

  return true;
}

}  // namespace hw4
