#include "tcpsocket.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

ErrNo SetNoblock(int fd) {
  int iflag = fcntl(fd, F_GETFL, 0);
  if (-1 == iflag) {
    return errno;
  }
  iflag = fcntl(fd, F_SETFL, iflag | O_NONBLOCK);
  if (-1 == iflag) {
    return errno;
  }
  return 0;
}

AcceptSocket::AcceptSocket(Epoll* e) {
  m_epoll = e;
  m_fd = -1;
  m_inWait = nullptr;
}

AcceptSocket::~AcceptSocket() { Close(); }

ErrNo AcceptSocket::Open() {
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd == -1) {
    return errno;
  }
  m_fd = fd;
  return 0;
}

ErrNo AcceptSocket::Bind(const char* szip, uint16_t port) {
  sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = inet_addr(szip);
  addr.sin_port = htons(port);
  int ibind = ::bind(m_fd, (const sockaddr*)(&addr), sizeof(addr));
  if (-1 == ibind) {
    return errno;
  }
  return 0;
}

ErrNo AcceptSocket::Listen(int backlog) {
  if (backlog < 128) {
    backlog = 128;
  }
  int ilisten = listen(m_fd, backlog);
  if (-1 == ilisten) {
    return errno;
  }
  int iset = SetNoblock(m_fd);
  if (iset != 0) {
    return iset;
  }
  return m_epoll->add(m_fd, shared_from_this());
}

std::tuple<int, ErrNo> AcceptSocket::Accept(GoContext* ctx) {
  assert(m_inWait == nullptr);
  while (true) {
    int s = accept(m_fd, nullptr, nullptr);
    if (s != -1) {
      return std::make_tuple(s, 0);
    }
    if (errno != EAGAIN && errno != EWOULDBLOCK) {
      return std::make_tuple(-1, errno);
    }
    m_inWait = ctx;
    ctx->Out();
    m_inWait = nullptr;
  }
}

void AcceptSocket::Close() {
  if (-1 == m_fd) {
    return;
  }
  m_epoll->del(this);
  close(m_fd);
  m_fd = -1;
  if (m_inWait == nullptr) {
    return;
  }
  m_epoll->push([wait = m_inWait]() { wait->In(); });
}

void AcceptSocket::OnIn() {
  if (m_inWait == nullptr) {
    return;
  }
  m_inWait->In();
}

void AcceptSocket::OnOut() {}

TcpSocket::TcpSocket(Epoll* e) {
  m_epoll = e;
  m_inWait = nullptr;
  m_connWait = nullptr;
  m_fd = -1;
  m_sendFail = false;
}

TcpSocket::~TcpSocket() { Close(); }

ErrNo TcpSocket::Open() {
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd == -1) {
    return errno;
  }
  int iset = SetNoblock(fd);
  if (iset != 0) {
    close(fd);
    return iset;
  }
  m_fd = fd;
  return m_epoll->add(m_fd, shared_from_this());
}

ErrNo TcpSocket::Open(int fd) {
  int iset = SetNoblock(fd);
  if (iset != 0) {
    close(fd);
    return iset;
  }
  m_fd = fd;
  return m_epoll->add(m_fd, shared_from_this());
}

ErrNo TcpSocket::Connect(GoContext* ctx, const char* szip, uint16_t port) {
  sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = inet_addr(szip);
  addr.sin_port = htons(port);
  int iconn = connect(m_fd, (const sockaddr*)(&addr), sizeof(addr));
  if (0 == iconn) {
    return 0;
  }
  if (errno != EINPROGRESS && errno != EAGAIN) {
    return errno;
  }
  m_connWait = ctx;
  ctx->Out();
  m_connWait = nullptr;
  int error = 0;
  socklen_t length = sizeof(error);
  if (getsockopt(m_fd, SOL_SOCKET, SO_ERROR, &error, &length) != 0) {
    return errno;
  }
  return error;
}

void TcpSocket::Write(const void* buf, size_t nbytes) {
  if (m_sendFail) {
    return;
  }
  if (nbytes == 0 || buf == nullptr) {
    return;
  }
  auto len = m_writeBuffer.size();
  if (len != 0) {
    m_writeBuffer.resize(len + nbytes);
    memcpy(&(m_writeBuffer[len]), buf, nbytes);
    return;
  }
  size_t total = 0;
  while (total != nbytes) {
    auto isend = send(m_fd, (uint8_t*)buf + total, nbytes - total, 0);
    if (isend >= 0) {
      total += isend;
      continue;
    }
    if (errno != EAGAIN && errno != EWOULDBLOCK) {
      m_sendFail = true;
      return;
    }
    break;
  }
  if (total == nbytes) {
    return;
  }
  m_writeBuffer.resize(nbytes - total);
  memcpy(&(m_writeBuffer[0]), (uint8_t*)buf + total, nbytes - total);
}

std::tuple<size_t, ErrNo> TcpSocket::Read(GoContext* ctx, void* buf,
                                          size_t nbytes) {
  while (true) {
    auto irecv = recv(m_fd, buf, nbytes, 0);
    if (irecv >= 0) {
      return std::make_tuple<size_t, ErrNo>(size_t(irecv), 0);
    }
    if (errno != EAGAIN && errno != EWOULDBLOCK) {
      return std::make_tuple<size_t, ErrNo>(size_t(0), ErrNo(errno));
    }
    m_inWait = ctx;
    ctx->Out();
    m_inWait = nullptr;
  }
}

void TcpSocket::Close() {
  if (-1 == m_fd) {
    return;
  }
  m_epoll->del(this);
  close(m_fd);
  m_fd = -1;
  if (m_connWait != nullptr) {
    m_epoll->push([connWait = m_connWait]() { connWait->In(); });
    return;
  }
  if (m_inWait == nullptr) {
    return;
  }
  m_epoll->push([wait = m_inWait]() { wait->In(); });
}

void TcpSocket::OnIn() {
  if (m_connWait != nullptr) {
    m_connWait->In();
    return;
  }
  if (m_inWait == nullptr) {
    return;
  }
  m_inWait->In();
}

void TcpSocket::OnOut() {
  if (m_connWait != nullptr) {
    m_connWait->In();
    return;
  }
  auto len = m_writeBuffer.size();
  if (len == 0) {
    return;
  }
  size_t total = 0;
  while (total != len) {
    auto isend = send(m_fd, &(m_writeBuffer[total]), len - total, 0);
    if (isend >= 0) {
      total += isend;
      continue;
    }
    if (errno != EAGAIN && errno != EWOULDBLOCK) {
      m_sendFail = true;
      m_writeBuffer.clear();
      return;
    }
    break;
  }
  if (total == len) {
    m_writeBuffer.clear();
    return;
  }
  memcpy(&(m_writeBuffer[0]), &(m_writeBuffer[total]), len - total);
  m_writeBuffer.resize(len - total);
}
