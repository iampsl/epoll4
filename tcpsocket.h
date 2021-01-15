#pragma once

#include <tuple>
#include <vector>

#include "epoll.h"

class AcceptSocket : public INotify,
                     public std::enable_shared_from_this<AcceptSocket> {
 public:
  AcceptSocket(Epoll* e);
  ~AcceptSocket();
  ErrNo Open();
  ErrNo Bind(const char* szip, uint16_t port);
  ErrNo Listen(int backlog);
  std::tuple<int, ErrNo> Accept(GoContext* ctx);
  void Close();

 private:
  virtual void OnIn() override;
  virtual void OnOut() override;

 private:
  Epoll* m_epoll;
  GoContext* m_inWait;
  int m_fd;
};

class ConnectSocket : public INotify {
 public:
  ConnectSocket();
  ~ConnectSocket();
  ErrNo Open();
  ErrNo Connect(GoContext* ctx, const char* szip, uint16_t port);
  void Write();
  ErrNo Read();
  void Close();
};

class TcpSocket : public INotify {
 public:
  TcpSocket(Epoll* e);
  ~TcpSocket();
  ErrNo Open(int fd);
  void Write();
  ErrNo Read();
  void Close();

 private:
  virtual void OnIn() override;
  virtual void OnOut() override;

 private:
  Epoll* m_epoll;
  GoContext* m_inWait;
  GoContext* m_outWait;
  int m_fd;
  std::vector<uint8_t> m_writeBuffer;
};
