#pragma once

#include "epoll.h"
#include "goclient.h"

class server {
 public:
  void Start(int num);

 private:
  Epoll m_epoll;
  GoClient m_goclient;
};