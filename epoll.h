#pragma once

#include <sys/epoll.h>

#include <boost/coroutine2/all.hpp>
#include <functional>
#include <list>
#include <memory>
#include <unordered_map>

typedef int ErrNo;

class INotify {
 public:
  virtual void OnIn() = 0;
  virtual void OnOut() = 0;
};

class Epoll;
class GoContext {
 public:
  void Out();
  void In();
  void Sleep(unsigned int s);
  Epoll *GetEpoll();

 private:
  friend Epoll;
  friend void goimpl(GoContext *pctx, std::function<void(GoContext &)> func,
                     boost::coroutines2::coroutine<void>::pull_type &pull);
  GoContext(Epoll *e, std::function<void(GoContext &)> func);

 private:
  Epoll *m_epoll;
  boost::coroutines2::coroutine<void>::push_type m_self;
  boost::coroutines2::coroutine<void>::pull_type *m_yield;
};

class GoChan {
 public:
  GoChan(Epoll *e);
  void Add(GoContext *ctx);
  void Wake();
  Epoll *GetEpoll();

 private:
  Epoll *m_epoll;
  std::list<GoContext *> m_ctxs;
};

class Epoll {
 public:
  Epoll();
  Epoll(const Epoll &) = delete;
  Epoll &operator=(const Epoll &) = delete;
  ~Epoll();
  ErrNo Create();
  ErrNo Wait(int ms);
  void Go(std::function<void(GoContext &)> func);

 private:
  ErrNo add(int s, std::shared_ptr<INotify> pnotify);
  void del(INotify *pnotify);
  void push(std::function<void()> func);
  void release(GoContext *pctx);
  void sleep(GoContext *pctx, unsigned int s);
  void tick();
  void onTime();

 private:
  friend class AcceptSocket;
  friend class TcpSocket;
  friend GoChan;
  friend GoContext;
  friend void goimpl(GoContext *pctx, std::function<void(GoContext &)> func,
                     boost::coroutines2::coroutine<void>::pull_type &pull);

 private:
  int m_epollFd;
  GoContext *m_del;
  std::unordered_map<INotify *, std::shared_ptr<INotify>> m_notifies;
  std::list<std::function<void()>> m_funcs;
  epoll_event m_events[10000];

  time_t m_baseTime;
  size_t m_timeIndex;
  std::list<GoContext *> m_timeWheel[60];
};