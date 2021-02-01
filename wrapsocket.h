#pragma once

#include "epoll.h"
#include <netinet/in.h>
#include <tuple>
#include <vector>

class AcceptSocket : public INotify {
public:
  AcceptSocket(Epoll *e);
  AcceptSocket(const AcceptSocket &) = delete;
  AcceptSocket &operator=(const AcceptSocket &) = delete;
  ~AcceptSocket();
  ErrNo Open();
  ErrNo Bind(const char *szip, uint16_t port);
  ErrNo Listen(int backlog);
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
  ErrNo Open();
  ErrNo Connect(GoContext *ctx, const char *szip, uint16_t port);
  void Write(const void *buf, size_t nbytes);
  std::tuple<size_t, ErrNo> Read(GoContext *ctx, void *buf, size_t nbytes);
  void Close();

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