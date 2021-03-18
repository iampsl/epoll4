#pragma once

#include <sys/epoll.h>

#include <boost/coroutine2/all.hpp>
#include <functional>
#include <list>
#include <unordered_set>
#include <vector>

typedef int ErrNo;

time_t curtime();

class INotify {
 public:
  virtual void OnIn() = 0;
  virtual void OnOut() = 0;
};

class Epoll;
class GoContext {
 public:
  GoContext(const GoContext &) = delete;
  GoContext &operator=(const GoContext &) = delete;
  void Out() { (*m_yield)(); }
  void In() { m_self(); }
  void Sleep(unsigned int s);
  Epoll *GetEpoll() { return m_epoll; }

 private:
  friend Epoll;
  friend void goimpl(GoContext *pctx, std::function<void(GoContext &)> func,
                     boost::coroutines2::coroutine<void>::pull_type &pull);
  GoContext(Epoll *e, std::function<void(GoContext &)> func,
            std::size_t stackSize);

 private:
  Epoll *m_epoll;
  boost::coroutines2::coroutine<void>::push_type m_self;
  boost::coroutines2::coroutine<void>::pull_type *m_yield;
};

class GoChan {
 public:
  GoChan(Epoll *e) {
    m_epoll = e;
    m_wait = nullptr;
  }
  GoChan(const GoChan &) = delete;
  GoChan &operator=(const GoChan &) = delete;
  void Wait(GoContext *ctx) {
    m_wait = ctx;
    ctx->Out();
  }
  bool Wake();
  Epoll *GetEpoll() { return m_epoll; }

 private:
  Epoll *m_epoll;
  GoContext *m_wait;
};

class Epoll {
 public:
  Epoll();
  Epoll(const Epoll &) = delete;
  Epoll &operator=(const Epoll &) = delete;
  ~Epoll();
  ErrNo Create();
  ErrNo Wait(int ms);
  void Go(std::function<void(GoContext &)> func,
          std::size_t stackSize = 1024 * 1024 * 8);

 private:
  ErrNo add(int s, INotify *pnotify);
  void del(int s, INotify *pnotify);
  bool exist(INotify *pnotify);
  void push(std::function<void()> func);
  void release(GoContext *pctx);
  void sleep(GoContext *pctx, unsigned int s);
  void tick();
  void onTime();

 private:
  friend class AcceptSocket;
  friend class TcpSocket;
  friend class UdpSocket;
  friend GoChan;
  friend GoContext;
  friend void goimpl(GoContext *pctx, std::function<void(GoContext &)> func,
                     boost::coroutines2::coroutine<void>::pull_type &pull);

 private:
  int m_epollFd;
  GoContext *m_del;
  std::unordered_set<INotify *> m_notifies;
  std::vector<std::function<void()>> m_funcs;
  epoll_event m_events[10000];

  time_t m_baseTime;
  size_t m_timeIndex;
  std::vector<GoContext *> m_timeWheel[60];
};