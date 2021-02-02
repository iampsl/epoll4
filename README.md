# 前言
使用golang开发后台服务有一段时间了，发现协程是个好东西，比异步回调方式好得多！但golang有gc，而且基本可以认为是抢占式调度，golang的性能与c++相比还是差很多！
我心目中的c++协程库应该是性能强悍，代码简单，协作式，单线程(如果想使用多核cpu功能，那线程与线程之间通过unix socket通信(相当于go的csp方式)，这样代码完全可以不需要锁，大大降低系统开发难度)
，再一个对第三方式的调用不应该用hook方式，因为这种方式鬼才知道有没有问题，其实解决这个问题，完全可以以进程之间的通信方式用其它的语言来解决(比如node.js,golang)。
故我使用boost.Coroutine2这个库，封装了epoll系统调用，让c++也能愉快的使用协程来开发后台服务！本项目只对epoll进行很简单的封装，代码简单，很容易读懂，性能优异！

# epoll4
第四代epoll 协程相结合的网络库

使用boost.Coroutine2对epoll系统调用的简单封装


使用说明

# 1.下载boost

到官网 https://www.boost.org/   下载最新的release版本
# 2.编译boost

进入boost代码目录(比如boost_1_70_0)，先执行./bootstrap.sh 生成编译boost库工具,然后执行

./b2 --with-context variant=release optimization=speed link=shared threading=multi runtime-link=shared stage

在stage目录会生成动态库
# 3.编译本项目

进入epoll3/epoll2/build/linux/debug目录，修改makefile文件中的如下二项为你所编译的boost库目录

库头文件目录

HEAD_DIR=-I/home/iampsl/boost_1_70_0/


库文件目录

LIB_DIR=-L/home/iampsl/boost_1_70_0/stage/lib/


执行make
