#include "NodeHandler.h"
#include "big.pb.h"

big::BigData gep;
void gep_callback(big::BigData msg)
{
    gep = msg;
}

int main(int argc, char* argv[])
{
    boost::asio::io_context node_ios(4);
    int freq = atoi(argv[1]);
    core::NodeHandler nh("127.0.0.1", 50051, "127.0.0.1", node_ios);
    core::Publisher pub = nh.publisher("hello");
    core::Subscriber sub = nh.subscriber("hello", freq);
    std::thread node_thread_ = std::thread([&]() {
        node_ios.run();
    });
    big::BigData ep;
    for (int i = 0; i < 20; i++)
        ep.add_data(i);
    boost::asio::deadline_timer timer(node_ios);
    uint sleep_us = 1000000 / freq;
    int i = 0;
    while(1) {
        i += 1;
        timer.expires_from_now(boost::posix_time::microseconds(sleep_us));
        ep.set_str(std::to_string(i));
        pub.publish(ep);
        sub.spinOnce(gep_callback);
        if (gep.data_size() != 0)
            std::cout << gep.str() << "\n";
        timer.wait();
    }
    return 0;
}
