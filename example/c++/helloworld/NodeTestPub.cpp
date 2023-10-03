#include "NodeHandler.h"
#include "hello.pb.h"
int main() {
    core::NodeHandler nh;
    core::Rate rate(1000);
    core::Publisher<hello::hello> &pub = nh.advertise<hello::hello>("hello");
    int i = 0;
    while (1)
    {
        const auto start = std::chrono::steady_clock::now();
        i++;

        hello::hello msg_p;
        msg_p.set_string_field(std::to_string(i));
        hello::greeting *struct_ptr = msg_p.mutable_struct_field();
        struct_ptr->set_bool_field(true);
        struct_ptr->set_string_field(std::to_string(i));
        msg_p.set_enum_field(hello::hello::HI);
        for (int j = 0; j < 100; j ++) {
            msg_p.mutable_float_array_field()->Add(j);
        }
        std::cout << "message length\t" << msg_p.ByteSizeLong() << "\n";
        pub.publish(msg_p);
        std::cout << "publishing\t" << msg_p.string_field() << "\n";
	    std::cout << rate.sleep() << "\n";
        const auto diff = std::chrono::steady_clock::now() - start;
        std::cout << std::chrono::duration<double>(diff).count() << " seconds\n";
    }
    return 0;
}
