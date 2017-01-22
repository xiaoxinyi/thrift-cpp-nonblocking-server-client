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
      sleep(3);// do something time-consuming/ 这里我们在server端加一些耗时的操作
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
