#pragma once

#include <netinet/in.h>

#include <tuple>
#include <vector>

#include "epoll.h"

void SockAddr(sockaddr_in &addr, const char *szip, uint16_t port);

class AcceptSocket : public INotify {
 public:
  AcceptSocket(Epoll *e);
  AcceptSocket(const AcceptSocket &) = delete;
  AcceptSocket &operator=(const AcceptSocket &) = delete;
  ~AcceptSocket();
  ErrNo Listen(const char *szip, uint16_t port, int backlog = 128);
  ErrNo Listen(const char *unixPath, int backlog = 128);
  std::tuple<int, ErrNo> Accept(GoContext *ctx);
  void Close();

 private:
  virtual void OnIn() override;
  virtual void OnOut() override;

 private:
  Epoll *m_epoll;
  GoContext *m_inWait;
  int m_fd;
};

class TcpSocket : public INotify {
 public:
  TcpSocket(Epoll *e);
  TcpSocket(const TcpSocket &) = delete;
  TcpSocket &operator=(const TcpSocket &) = delete;
  ~TcpSocket();
  ErrNo Open(int fd);
  ErrNo Connect(GoContext *ctx, const char *szip, uint16_t port,
                unsigned int seconds);
  ErrNo Connect(GoContext *ctx, const char *unixPath, unsigned int seconds);
  void Write(const void *buf, size_t nbytes);
  std::tuple<size_t, ErrNo> Read(GoContext *ctx, void *buf, size_t nbytes);
  void Close();

 private:
  ErrNo doConnectWithTimeout(GoContext *ctx, const sockaddr *addr,
                             socklen_t len, unsigned int seconds);
  ErrNo doConnect(GoContext *ctx, const sockaddr *addr, socklen_t len);

 private:
  virtual void OnIn() override;
  virtual void OnOut() override;

 private:
  Epoll *m_epoll;
  GoContext *m_inWait;
  GoContext *m_connWait;
  int m_fd;
  bool m_sendFail;
  std::vector<uint8_t> m_writeBuffer;
};

class UdpSocket : public INotify {
 public:
  UdpSocket(Epoll *e);
  UdpSocket(const UdpSocket &) = delete;
  UdpSocket &operator=(const UdpSocket &) = delete;
  ~UdpSocket();
  ErrNo Open();
  ErrNo Bind(const char *szip, uint16_t port);
  std::tuple<size_t, ErrNo> Recvfrom(GoContext *ctx, void *buf, size_t len,
                                     sockaddr_in &srcAddr);
  void Sendto(const void *buf, size_t len, sockaddr_in &dstAddr);
  void Close();

 private:
  virtual void OnIn() override;
  virtual void OnOut() override;

 private:
  Epoll *m_epoll;
  GoContext *m_inWait;
  int m_fd;
  std::vector<uint8_t> m_writeBuffer;
};
