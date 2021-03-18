#pragma once

#include "protorpc.h"

class GoRPC : public ProtoRPC {
 public:
  void QueryUserInfo(GoContext *ctx, const std::string &username);
};