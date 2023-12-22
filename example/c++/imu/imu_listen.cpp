#include "NodeHandler.h"
#include "imu.pb.h"
#include "google/protobuf/text_format.h"

std::mutex mutex_;
sensor_msg::imu msg;
void cb(sensor_msg::imu m) {
    mutex_.lock();
    msg = m;
    mutex_.unlock();
}
int main() {
    core::NodeHandler nh;
    core::Rate rate(100);
    core::Subscriber<sensor_msg::imu> &sub = nh.subscribe<sensor_msg::imu>("imu", 50, cb);
    int i = 0;
    while (1)
    {
        core::spinOnce();
        std::string receive;
        mutex_.lock();
        google::protobuf::TextFormat::PrintToString(msg, &receive);
        mutex_.unlock();
        std::cout << receive << "\n";
        std::cout << rate.sleep() << "\n";
    }
    return 0;
}
