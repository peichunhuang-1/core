#include "NodeHandler.h"
#include "hello.pb.h"
#include "google/protobuf/text_format.h"


int main() {
    core::NodeHandler nh;
    core::Rate rate(1);
    core::ServiceClient<hello::hellorequest, hello::helloreply> &clt = nh.serviceClient<hello::hellorequest, hello::helloreply>("hello");
    while (1)
    {
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
