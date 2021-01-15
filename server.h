#pragma once

#include "epoll.h"

class server {
 public:
  void Start();

 private:
  Epoll m_epoll;
};