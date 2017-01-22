# Thrift 实现一个非阻塞带有回调机制的客户端

一般Thrift 客户端，给服务端发送请求，阻塞式地等待服务端返回计算结果。但是，客户端有时候需要非阻塞地去发送请求，给服务端发送一个请求，要求其返回结果，但是客户端不想等待服务端处理完，而是想发送完成这个指令后自己去做其他事情，当结果返回时自动地去处理。

举个例子说明这个过程：饭店的Boss让小弟A把本周店里的欠条收集起来放倒自己的桌子上，然后又告诉自己的小秘书坐在自己的办公室等着小A把欠条拿过来，然后统计一下一共有多少，然后Boss自己出去办点事儿。

Boss相当于client，小弟A相当于server，而小秘书相当于client端的回调函数(callback)。

Boss不想等待小弟处理完，因为他老人家公务繁忙，还要去干别的呢。于是他把接下来处理欠条的任务托管给了小秘书，于是自己一个人出去了。

了解了整个工作流程，来看看实现的方法。thrift去实现client异步+回调的方法关键点在于：thrift生成的client中有个`send_XXX()`和`recv_XXX()`方法。`send_XXX()`相当于告知server去处理东西，可以立即返回；而调用`recv_XXX`就是个阻塞的方法了，直到server返回结果。

我们可以在主线程调用完`send_XXX()`之后，然后另开一个线程去等到server回复后自动调用callback方法，对结果进行一些处理（当然callback在修改client状态时需要进行同步操作）。

当然了需要注意的一点就是，**各个线程接受到结果的顺序跟请求顺序不一定一样，**因为server处理不同，请求时间不同或者网络环境的影响都可能导致这种情形。

## c++实现

`test.thrift`

```
service TestServ{
    string ping(1: string message),
}
```

`server.cpp`

```c++
#include "TestServ.h"

#include <iostream>

#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/server/TNonblockingServer.h>
#include <thrift/transport/TServerSocket.h>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/concurrency/PosixThreadFactory.h>
#include <thrift/concurrency/ThreadManager.h>
#include <thrift/concurrency/PlatformThreadFactory.h>

using namespace std;

using namespace ::apache::thrift;
using namespace ::apache::thrift::protocol;
using namespace ::apache::thrift::transport;
using namespace ::apache::thrift::server;
using namespace ::apache::thrift::concurrency;

class TestServHandler : virtual public TestServIf {
 public:
  TestServHandler() {
    // Your initialization goes here
  }

  void ping(std::string& _return, const std::string& message) {
      _return = "hello, i am server! ";
      // do something time-consuming
      sleep(3);
      cout<<"Request from client: "<<message<<endl;
  }

};

int main() {
  int port = 9090;

  boost::shared_ptr<TestServHandler> handler(new TestServHandler());
  boost::shared_ptr<TProcessor> processor(new TestServProcessor(handler));
  boost::shared_ptr<TProtocolFactory> protocolFactory(new TBinaryProtocolFactory());
  boost::shared_ptr<ThreadManager> threadManager = ThreadManager::newSimpleThreadManager(15);
  boost::shared_ptr<PosixThreadFactory> threadFactory = boost::shared_ptr<PosixThreadFactory > (new PosixThreadFactory());
  threadManager->threadFactory(threadFactory);
  threadManager->start();
  TNonblockingServer server(processor, protocolFactory, port, threadManager);
  server.serve();
  return 0;
}

```



`client.cpp`

```c++
#include "TestServ.h"
#include "test_constants.h"

#include <iostream>
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/transport/TSocket.h>

using namespace std;
using namespace ::apache::thrift;
using namespace ::apache::thrift::protocol;
using namespace ::apache::thrift::transport;
using boost::shared_ptr;

class AsynTestClient;

void *wait_recv(void *parg);

struct PARG {
  AsynTestClient *pthis;
  string message;
};

class AsynTestClient {
private:
  // 客户端接受到server响应次数的计数器.
  unsigned int d_cnt_recv;

  // 计数器的读写锁.
  pthread_rwlock_t m_cnt_recv;
  vector<pthread_t> m_ids;

public:
  TestServClient *d_client;

  void call_back(string &_return) {
    // 输出服务器返回信息并把返回计数加
    cout << "server msg: " << _return << endl;
    pthread_rwlock_wrlock(&m_cnt_recv);
    d_cnt_recv++;
    pthread_rwlock_unlock(&m_cnt_recv);
  }

  explicit AsynTestClient(boost::shared_ptr<TProtocol> &protocol) {
    pthread_rwlock_init(&m_cnt_recv, NULL);
    d_cnt_recv = 0;
    d_client = new TestServClient(protocol);
  }

  ~AsynTestClient() {
    delete d_client;
    pthread_rwlock_destroy(&m_cnt_recv);
  }

  void asyn_ping(const string &message) {
    // 发送请求
    d_client->send_ping(message);
    // 初始化每个等待回调线程的参数
    PARG *parg = new PARG;
    parg->pthis = this;
    parg->message = message;
    // 把新生成的线程id放入全局数组维护
    pthread_t m_id;
    m_ids.push_back(m_id);
    // 启动线程，从此只要接受到服务器的返回结果就调用回调函数。
    if (0 != pthread_create(&m_id, NULL, wait_recv,
                            reinterpret_cast<void *>(parg))) {
      return;
    }
  }
};

void *wait_recv(void *parg) {
  //强制转化线程参数
  PARG *t_parg = reinterpret_cast<PARG *>(parg);
  string _return;
  t_parg->pthis->d_client->recv_ping(_return);
  t_parg->pthis->call_back(_return);
}

int main() {

  boost::shared_ptr<TSocket> socket(new TSocket("localhost", 9090));
  boost::shared_ptr<TTransport> transport(new TFramedTransport(socket));
  boost::shared_ptr<TProtocol> protocol(new TBinaryProtocol(transport));

  // TestServClient client(protocol);

  transport->open();
  AsynTestClient client(protocol);
  string message = "hello, i am client! ";
  client.asyn_ping(message);

  while (true) {
    cout << "Client doing ..." << endl;
    //这里相当于client去做别的事情了
    sleep(1);
  }

  transport->close();
  return 0;
}
```

这里没有使用`asyn_ping(const string & message, void(*)call_back(void));`这种方式去定义它，这是因为`asyn_ping`本身可以获取`callback`函数的指针。回调的本质是任务的托管、时间的复用，也就是说等待结果返回后自动去调用一段代码而已，所以本质上上面就是回调机制。如果你想使用传函数指针的方式，也可以实现出来。

## CMakeLists.txt说明

```
# remove *.skeleton.cpp before run cmake

cmake_minimum_required(VERSION 2.8)
project(thrift-cpp-demo)


include_directories("/usr/include/boost/")

#Make sure gen-cpp files can be included
include_directories("${CMAKE_CURRENT_BINARY_DIR}")
include_directories("${PROJECT_SOURCE_DIR}/gen-cpp")


set(SERVER_SOURCE "server.cpp")
set(CLIENT_SOURCE "client.cpp")
set(THRIFT_STATIC_LIB "/usr/local/lib/libthrift.a")
set(THRIFT_NONBLOCK_LIB "/usr/local/lib/libthriftnb.a")
set(EVENT_LIB "/usr/local/Cellar/libevent/2.0.22/lib/libevent.a")
# find_package(libevent REQUIRED)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -Wextra -pedantic -O3 -Wno-long-long -fno-omit-frame-pointer")

file(GLOB thrift_gen_cpp_SOURCES "gen-cpp/*.cpp")

message(STATUS "haha ${thrift_gen_cpp_SOURCES}")

add_library(thrift_gen_cpp STATIC ${thrift_gen_cpp_SOURCES})
target_link_libraries(thrift_gen_cpp ${THRIFT_STATIC_LIB})
target_link_libraries(thrift_gen_cpp pthread)


add_executable(server ${SERVER_SOURCE})
target_link_libraries(server thrift_gen_cpp)
target_link_libraries(server ${THRIFT_STATIC_LIB})
target_link_libraries(server ${THRIFT_NONBLOCK_LIB})
target_link_libraries(server pthread)
target_link_libraries(server ${EVENT_LIB})

add_executable(client ${CLIENT_SOURCE})
target_link_libraries(client thrift_gen_cpp)
target_link_libraries(client ${THRIFT_STATIC_LIB})
target_link_libraries(client pthread)
```


在server端需要用的库：`libthrift.a`，`libthriftnb.a`，`libevent.a`，链接的时候把这些库都链上。

## 参考资料

[1]: http://www.cnblogs.com/colorfulkoala/p/3487948.html
[2]: https://github.com/xiaoxinyi/thrift-cpp-nonblocking-server-client
