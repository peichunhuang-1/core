#include "NodeHandler.h"
#include "hello.pb.h"
#include "google/protobuf/text_format.h"

std::mutex mutex_;
hello::hello msg;
void cb(hello::hello m) {
    mutex_.lock();
    msg = m;
    mutex_.unlock();
}
int main() {
    core::NodeHandler nh;
    core::Rate rate(1000);
    core::Subscriber<hello::hello> &sub = nh.subscribe<hello::hello>("hello", 1000, cb);
    int i = 0;
    while (1)
    {
        core::spinOnce();
        std::string receive;
        mutex_.lock();
        // google::protobuf::TextFormat::PrintToString(msg, &receive);
        mutex_.unlock();
        // std::cout << receive << "\n";
        // std::cout << rate.sleep() << "\n";
        rate.sleep();
    }
    return 0;
}
