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
    core::Rate rate(1);
    core::Subscriber<hello::hello> &sub = nh.subscribe<hello::hello>("hello", 500, cb);
    core::ServiceClient<hello::hellorequest, hello::helloreply> &clt = nh.serviceClient<hello::hellorequest, hello::helloreply>("hello");
    int i = 0;
    while (1)
    {
        core::spinOnce();
        std::string receive;
        mutex_.lock();
        google::protobuf::TextFormat::PrintToString(msg, &receive);
        mutex_.unlock();
        std::cout << receive << "\n";
        hello::hellorequest request;
        hello::helloreply reply;
        request.set_index(10);
        if (clt.pull_request(request, reply)) {
            std::string receive;
            google::protobuf::TextFormat::PrintToString(reply, &receive);
            std::cout << receive << "\n";
        }
        std::cout << rate.sleep() << "\n";
    }
    return 0;
}