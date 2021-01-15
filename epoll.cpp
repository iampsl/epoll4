#include "epoll.h"

#include <errno.h>
#include <unistd.h>

void goimpl(GoContext *pctx, std::function<void(GoContext &)> func,
            boost::coroutines2::coroutine<void>::pull_type &pull) {
  pctx->m_yield = &pull;
  try {
    func(*pctx);
  } catch (...) {
  }
  pctx->GetEpoll()->release(pctx);
}

GoContext::GoContext(Epoll *e, std::function<void(GoContext &)> func)
    : m_self(std::bind(goimpl, this, std::move(func), std::placeholders::_1)) {
  m_epoll = e;
  m_yield = nullptr;
}

void GoContext::Out() { (*m_yield)(); }

void GoContext::In() { m_self(); }

Epoll *GoContext::GetEpoll() { return m_epoll; }

GoChan::GoChan(Epoll *e) { m_epoll = e; }

void GoChan::Add(GoContext *ctx) {
  m_ctxs.push_back(ctx);
  ctx->Out();
}

void GoChan::Wake() {
  if (m_ctxs.empty()) {
    return;
  }
  GoContext *ctx = m_ctxs.front();
  m_ctxs.pop_front();
  m_epoll->push([ctx]() { ctx->In(); });
}

Epoll *GoChan::GetEpoll() { return m_epoll; }

Epoll::Epoll() {
  m_epollFd = -1;
  m_del = nullptr;
}

Epoll::~Epoll() {
  if (m_epollFd != -1) {
    close(m_epollFd);
  }
  if (m_del != nullptr) {
    delete m_del;
  }
}

ErrNo Epoll::Create() {
  m_epollFd = epoll_create(sizeof(m_events) / sizeof(m_events[0]));
  if (m_epollFd == -1) {
    return errno;
  }
  return 0;
}

ErrNo Epoll::add(int s, std::shared_ptr<INotify> pnotify) {
  epoll_event e;
  e.events = EPOLLIN | EPOLLOUT | EPOLLET;
  e.data.ptr = pnotify.get();
  int ictl = epoll_ctl(m_epollFd, EPOLL_CTL_ADD, s, &e);
  if (0 != ictl) {
    return errno;
  }
  m_notifies.insert(std::make_pair(pnotify.get(), std::move(pnotify)));
  return 0;
}

void Epoll::del(INotify *pnotify) {
  auto iter = m_notifies.find(pnotify);
  if (iter == m_notifies.end()) {
    return;
  }
  m_notifies.erase(iter);
}

void Epoll::push(std::function<void()> func) {
  m_funcs.push_back(std::move(func));
}

void Epoll::Go(std::function<void(GoContext &)> func) {
  auto pctx = new GoContext(this, std::move(func));
  push([pctx]() { pctx->In(); });
}

ErrNo Epoll::Wait(int ms) {
  while (!m_funcs.empty()) {
    (m_funcs.front())();
    m_funcs.pop_front();
  }
  int iwait = epoll_wait(m_epollFd, m_events,
                         sizeof(m_events) / sizeof(m_events[0]), ms);
  if (iwait < 0) {
    return errno;
  }
  if (iwait == 0) {
    //超时处理
    return 0;
  }
  for (int i = 0; i < iwait; i++) {
    INotify *ptmpNotify = reinterpret_cast<INotify *>(m_events[i].data.ptr);
    auto iter = m_notifies.find(ptmpNotify);
    if (iter == m_notifies.end()) {
      continue;
    }
    if (m_events[i].events & EPOLLOUT) {
      iter->second->OnOut();
    }
    if (m_events[i].events != EPOLLOUT) {
      iter->second->OnIn();
    }
  }
  return 0;
}

void Epoll::release(GoContext *pctx) {
  if (m_del != nullptr) {
    delete m_del;
  }
  m_del = pctx;
}

std::shared_ptr<GoChan> Epoll::Chan() {
  return std::shared_ptr<GoChan>(new GoChan(this));
}