#include "NodeHandler.h"
#include "hello.pb.h"
#include "google/protobuf/text_format.h"

void cb(hello::hellorequest request, hello::helloreply &reply) {
    std::cout << "request index\t" << request.index() << "\n";
    reply.set_index(request.index() + 1);
}

int main() {
    core::NodeHandler nh;
    core::Rate rate(1);
    core::ServiceServer<hello::hellorequest, hello::helloreply> &srv = nh.serviceServer<hello::hellorequest, hello::helloreply>("hello", cb);
    while (1)
    {
        std::cout << rate.sleep() << "\n";
    }
    return 0;
}
