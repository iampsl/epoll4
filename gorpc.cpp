#include "gorpc.h"

#include "ctogo.pb.h"

void GoRPC::QueryUserInfo(GoContext *ctx, const std::string &username) {
  QueryUserInfoReq req;
  req.set_username(username);
  QueryUserInfoRsp rsp;
  ErrNo err = Call(ctx, QUERY_USER_INFO, req, rsp);
  if (err) {
    return;
  }
}