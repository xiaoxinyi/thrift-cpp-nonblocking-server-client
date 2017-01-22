#include "TestServ.h"

#include <iostream>
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/transport/TSocket.h>

#include "test_constants.h"

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
    //输出服务器返回信息并把返回计数加1
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
    //发送请求
    d_client->send_ping(message);
    //初始化每个等待回调线程的参数
    PARG *parg = new PARG;
    parg->pthis = this;
    parg->message = message;
    //把新生成的线程id放入全局数组维护
    pthread_t m_id;
    m_ids.push_back(m_id);
    //启动线程，从此只要接受到服务器的返回结果就调用回调函数。
    if (0 != pthread_create(&m_id, NULL, wait_recv,
                            reinterpret_cast<void *>(parg))) {
      return;
    }
  }
};
int main(int argc, char **argv) {

  boost::shared_ptr<TSocket> socket(new TSocket("localhost", 9090));
  boost::shared_ptr<TTransport> transport(new TFramedTransport(socket));
  boost::shared_ptr<TProtocol> protocol(new TBinaryProtocol(transport));

  // TestServClient client(protocol);

  transport->open();
  AsynTestClient client(protocol);
  string message = "hello, i am client! ";
  client.asyn_ping(message);

  while (true) {
    sleep(1); //这里相当于client去做别的事情了
  }

  transport->close();
  return 0;
}
void *wait_recv(void *parg) {
  PARG *t_parg = reinterpret_cast<PARG *>(parg); //强制转化线程参数
  string _return;
  t_parg->pthis->d_client->recv_ping(_return);
  t_parg->pthis->call_back(_return);
}
