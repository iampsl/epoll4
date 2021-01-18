#include "epoll.h"

#include <unistd.h>

#include <cerrno>
#include <ctime>

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

void GoContext::Sleep(unsigned int s) { m_epoll->sleep(this, s); }

Epoll *GoContext::GetEpoll() { return m_epoll; }

GoChan::GoChan(Epoll *e) { m_epoll = e; }

void GoChan::Add(GoContext *ctx) {
  m_ctxs.push_back(ctx);
  ctx->Out();
}

bool GoChan::Wake() {
  if (m_ctxs.empty()) {
    return false;
  }
  GoContext *ctx = m_ctxs.front();
  m_ctxs.pop_front();
  m_epoll->push([ctx]() { ctx->In(); });
  return true;
}

Epoll *GoChan::GetEpoll() { return m_epoll; }

Epoll::Epoll() {
  m_epollFd = -1;
  m_del = nullptr;
  time(&m_baseTime);
  m_timeIndex = 0;
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

ErrNo Epoll::add(int s, INotify *pnotify) {
  epoll_event e;
  e.events = EPOLLIN | EPOLLOUT | EPOLLET;
  e.data.ptr = pnotify;
  int ictl = epoll_ctl(m_epollFd, EPOLL_CTL_ADD, s, &e);
  if (0 != ictl) {
    return errno;
  }
  m_notifies.insert(pnotify);
  return 0;
}

void Epoll::del(INotify *pnotify) { m_notifies.erase(pnotify); }

bool Epoll::exist(INotify *pnotify) {
  if (m_notifies.find(pnotify) == m_notifies.end()) {
    return false;
  }
  return true;
}

void Epoll::push(std::function<void()> func) {
  m_funcs.push_back(std::move(func));
}

void Epoll::Go(std::function<void(GoContext &)> func) {
  auto pctx = new GoContext(this, std::move(func));
  push([pctx]() { pctx->In(); });
}

ErrNo Epoll::Wait(int ms) {
  onTime();
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
    return 0;
  }
  for (int i = 0; i < iwait; i++) {
    INotify *ptmpNotify = reinterpret_cast<INotify *>(m_events[i].data.ptr);
    if (m_events[i].events & EPOLLOUT) {
      if (exist(ptmpNotify)) {
        ptmpNotify->OnOut();
      }
    }
    if (m_events[i].events != EPOLLOUT) {
      if (exist(ptmpNotify)) {
        ptmpNotify->OnIn();
      }
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

void Epoll::onTime() {
  time_t now;
  time(&now);
  auto sub = now - m_baseTime;
  if (sub <= 0) {
    return;
  }
  m_baseTime = now;
  for (decltype(sub) i = 0; i < sub; i++) {
    tick();
  }
}

void Epoll::tick() {
  const auto MaxSleep = sizeof(m_timeWheel) / sizeof(m_timeWheel[0]);
  m_timeIndex = (m_timeIndex + 1) % MaxSleep;
  if (m_timeWheel[m_timeIndex].empty()) {
    return;
  }
  for (auto v : m_timeWheel[m_timeIndex]) {
    push([ctx = v]() { ctx->In(); });
  }
  m_timeWheel[m_timeIndex].clear();
}

void Epoll::sleep(GoContext *pctx, unsigned int s) {
  if (s <= 0) {
    return;
  }
  const auto MaxSleep = sizeof(m_timeWheel) / sizeof(m_timeWheel[0]);
  if (s > MaxSleep) {
    s = MaxSleep;
  }
  m_timeWheel[(m_timeIndex + s) % MaxSleep].push_back(pctx);
  pctx->Out();
}