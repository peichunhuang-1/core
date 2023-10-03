#include "NodeHandler.h"
#include "hello.pb.h"
#include "google/protobuf/text_format.h"

void cb(hello::hellorequest request, hello::helloreply &reply) {
    std::cout << "request index\t" << request.index() << "\n";
    reply.set_index(request.index() + 1);
}

int main() {
    core::NodeHandler nh;
    core::Rate rate(1000);
    core::Publisher<hello::hello> &pub = nh.advertise<hello::hello>("hello");
    core::ServiceServer<hello::hellorequest, hello::helloreply> &srv = nh.serviceServer<hello::hellorequest, hello::helloreply>("hello", cb);
    int i = 0;
    while (1)
    {
        hello::hello msg_p;
        i++;
        msg_p.set_string_field(std::to_string(i));
        hello::greeting *struct_ptr = msg_p.mutable_struct_field();
        struct_ptr->set_bool_field(true);
        struct_ptr->set_string_field(std::to_string(i));
        msg_p.set_enum_field(hello::hello::HI);
        for (int j = 0; j < 100; j ++) {
            msg_p.mutable_float_array_field()->Add(j);
        }
        pub.publish(msg_p);
        std::cout << rate.sleep() << "\n";
    }
    return 0;
}
