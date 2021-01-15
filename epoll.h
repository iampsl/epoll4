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
  Epoll *GetEpoll();

 private:
  friend void goimpl(GoContext *pctx, std::function<void(GoContext &)> func,
                     boost::coroutines2::coroutine<void>::pull_type &pull);
  friend Epoll;
  GoContext(Epoll *e, std::function<void(GoContext &)> func);

 private:
  Epoll *m_epoll;
  boost::coroutines2::coroutine<void>::push_type m_self;
  boost::coroutines2::coroutine<void>::pull_type *m_yield;
};

class GoChan {
 public:
  void Add(GoContext *ctx);
  void Wake();
  Epoll *GetEpoll();

 private:
  GoChan(Epoll *e);
  friend Epoll;

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
  std::shared_ptr<GoChan> Chan();

 private:
  ErrNo add(int s, std::shared_ptr<INotify> pnotify);
  void del(INotify *pnotify);
  void push(std::function<void()> func);
  void release(GoContext *pctx);

 private:
  friend class AcceptSocket;
  friend GoChan;
  friend void goimpl(GoContext *pctx, std::function<void(GoContext &)> func,
                     boost::coroutines2::coroutine<void>::pull_type &pull);

 private:
  int m_epollFd;
  GoContext *m_del;
  std::unordered_map<INotify *, std::shared_ptr<INotify>> m_notifies;
  std::list<std::function<void()>> m_funcs;
  epoll_event m_events[10000];
};