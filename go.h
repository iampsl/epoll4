#ifndef PSL_GO_H
#define PSL_GO_H

#include <boost/coroutine2/all.hpp>
#include <functional>

class GoContext {
 public:
  GoContext();
  ~GoContext();
  void Out();
  void In();

 private:
  friend void go(std::function<void(GoContext &)> func);
  static void goimpl(GoContext *pctx, std::function<void(GoContext &)> func,
                     boost::coroutines2::coroutine<void>::pull_type &pull);

 private:
  boost::coroutines2::coroutine<void>::push_type *m_self;
  boost::coroutines2::coroutine<void>::pull_type *m_yield;
};

void go(std::function<void(GoContext &)> func);

#endif
