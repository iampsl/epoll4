#include <iostream>

#include "go.h"

class student {
 public:
  student() { std::cout << "student" << std::endl; }
  ~student() { std::cout << "~student" << std::endl; }
};

void test(GoContext &ctx) {
  std::cout << "test in" << std::endl;
  for (int i = 0; i < 1; i++) {
    student stu;
  }
  std::cout << "test out" << std::endl;
}
int main(int argc, char *argv[]) {
  for (int i = 0; i < 1000000; i++) {
    go(test);
  }
  return 0;
}