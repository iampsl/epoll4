#pragma once

#include "epoll.h"

class server {
 public:
  void Start(int num);

 private:
  Epoll m_epoll;
};