#include <iostream>

#include "server.h"

int getNum(int argc, char *argv[]) {
  if (argc <= 1) {
    return 1;
  }
  int num = atoi(argv[1]);
  if (num <= 0) {
    return 1;
  }
  return num;
}

int main(int argc, char *argv[]) {
  server ser;
  ser.Start(getNum(argc, argv));
  return 0;
}