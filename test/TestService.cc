#include "NodeHandler.h"
#include "big.pb.h"

int main(int argc, char* argv[])
{
    boost::asio::io_context node_ios(4);
    core::NodeHandler nh("127.0.0.1", 50051, "127.0.0.1", node_ios);
    core::serviceServer IntServer = nh.servicepublisher("Int");
    core::serviceClient IntClient = nh.servicesubscriber("Int");
    std::thread node_thread_ = std::thread([&]() {
        node_ios.run();
    });
    boost::asio::deadline_timer timer(node_ios);
    
    big::BigData data;
    for (int i = 0; i < 20; i++)
        data.add_data(i);
    big::BigData r_data;
    int i = 0;
    while(1) {
        timer.expires_from_now(boost::posix_time::microseconds(1000000));
        IntServer.send(data);
        IntClient.get(r_data);
        data.set_str(std::to_string(i));
        std::cout << r_data.str() << "\n";
        i++;
        timer.wait();
    }
    return 0;
}
