#include "NodeHandler.h"
#include "cx5.hpp"
#include "imu.pb.h"

std::mutex mutex_;

void calibration_function(int calibration_time) {
    mutex_.lock();
    usleep(1000);
    mutex_.unlock();
}

void cb(sensor_msg::imu_calibration_request request, sensor_msg::imu_calibration_reply &reply) {
    std::cout << "Calibration...\n";
    calibration_function(request.inteval());
    reply.set_state(true);
    std::cout << "Calibration End\n";
}

int main() {
    core::NodeHandler nh;
    core::Rate rate(1000);
    core::Publisher<sensor_msg::imu> &pub = nh.advertise<sensor_msg::imu>("imu");
    core::ServiceServer<sensor_msg::imu_calibration_request, sensor_msg::imu_calibration_reply> &srv = 
    nh.serviceServer<sensor_msg::imu_calibration_request, sensor_msg::imu_calibration_reply>("imu", cb);
    CX5_AHRS imu("/dev/ttyACM0", 921600, 500, 500);
    std::thread imu_thread_ = std::thread([&imu]() {
        imu.start();
    });
    imu_thread_.detach();
    int i = 0;
    while (1)
    {
        mutex_.lock();
        sensor_msg::imu msg_imu;
        sensor_msg::Array3d *accel_ptr = msg_imu.mutable_acceleration();
        sensor_msg::Array3d *twist_ptr = msg_imu.mutable_twist();
        sensor_msg::Array4d *attitude_ptr = msg_imu.mutable_attitude();
        msg_imu.set_timestamp(i++);
        Eigen::Vector3f a, w;
        Eigen::Quaternionf q;
        imu.get(a, w, q);
        accel_ptr->set_x(a(0));
        accel_ptr->set_y(a(1));
        accel_ptr->set_z(a(2));
        twist_ptr->set_x(w(0));
        twist_ptr->set_y(w(1));
        twist_ptr->set_z(w(2));
        attitude_ptr->set_x(q.x());
        attitude_ptr->set_y(q.y());
        attitude_ptr->set_z(q.z());
        attitude_ptr->set_w(q.w());
        mutex_.unlock();
        pub.publish(msg_imu);
	    rate.sleep();
    }
    return 0;
}
