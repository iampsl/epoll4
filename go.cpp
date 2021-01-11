#include "go.h"

thread_local GoContext *gdel = nullptr;

GoContext::GoContext() : m_self(nullptr), m_yield(nullptr) {}
GoContext::~GoContext() {
  if (m_self != nullptr) {
    delete m_self;
  }
}

void GoContext::Out() { (*m_yield)(); }

void GoContext::In() { (*m_self)(); }

void GoContext::goimpl(GoContext *pctx, std::function<void(GoContext &)> func,
                       boost::coroutines2::coroutine<void>::pull_type &pull) {
  pctx->m_yield = &pull;
  try {
    func(*pctx);
  } catch (...) {
  }
  if (gdel != nullptr) {
    delete gdel;
  }
  gdel = pctx;
}

void go(std::function<void(GoContext &)> func) {
  auto pctx = new GoContext;
  pctx->m_self = new boost::coroutines2::coroutine<void>::push_type(std::bind(
      GoContext::goimpl, pctx, std::move(func), std::placeholders::_1));
  pctx->In();
}