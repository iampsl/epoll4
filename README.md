# epoll4
第四代epoll 协程相结合的网络库

使用boost.Coroutine2对epoll系统调用的简单封装


使用说明

1.下载boost

到官网 https://www.boost.org/   下载最新的release版本
2.编译boost

进入boost代码目录(比如boost_1_70_0)，先执行./bootstrap.sh 生成编译boost库工具,然后执行

./b2 --with-context variant=release optimization=speed link=shared threading=multi runtime-link=shared stage

在stage目录会生成动态库
3.编译本项目

进入epoll3/epoll2/build/linux/debug目录，修改makefile文件中的如下二项为你所编译的boost库目录

#库头文件目录

HEAD_DIR=-I/home/iampsl/boost_1_70_0/


#库文件目录

LIB_DIR=-L/home/iampsl/boost_1_70_0/stage/lib/


执行make
