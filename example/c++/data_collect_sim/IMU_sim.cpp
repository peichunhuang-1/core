#include "NodeHandler.h"
#include "imu.pb.h"

double qx, qy, qz, qw;
double ax, ay, az, tx, ty, tz;
double bax, bay, baz, btx, bty, btz;

double clib_ax = 0, clib_ay = 0, clib_az = 0;
double clib_tx = 0, clib_ty = 0, clib_tz = 0;

double accel_noise = 0.1;
double twist_noise = 0.01;

std::mutex mutex_;
inline double random_noise(double sigma_2) {
    return ((double) rand() / (RAND_MAX + 1.0) - 0.5) * sigma_2;
}
void calibration_function(int calibration_time) {
    mutex_.lock();
    core::Rate rate(1000);
    double sum_ax = 0; double sum_ay = 0; double sum_az = 0;
    double sum_tx = 0; double sum_ty = 0; double sum_tz = 0;
    for (int i = 0; i < calibration_time; i++)
    {
        sum_ax += 0 + random_noise(accel_noise) + bax; sum_ay += 0 + random_noise(accel_noise) + bay; sum_az += random_noise(accel_noise) + baz;
        sum_tx += 0 + random_noise(twist_noise) + btx; sum_ty += 0 + random_noise(twist_noise)+ bty; sum_tz += 0 + random_noise(twist_noise)+ btz;
	    rate.sleep();
    }
    clib_ax = sum_ax / (double) calibration_time; clib_ay = sum_ay / (double) calibration_time; clib_az = sum_az / (double) calibration_time;
    clib_tx = sum_tx / (double) calibration_time; clib_ty = sum_ty / (double) calibration_time; clib_tz = sum_tz / (double) calibration_time;
    std::cout << "calibration accel, bias: \t" << clib_ax << "\t"  << clib_ay << "\t" << clib_az <<"\n";
    std::cout << "calibration twist bias: \t" << clib_tx << "\t"  << clib_ty << "\t" << clib_tz <<"\n";
    qx = 0; qy = 0; qz = 0; qw = 1;
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
    qx = 0; qy = 0; qz = 0; qw = 1;
    ax = 0; ay = 0; az = 0;
    tx = 0; ty = 0; tz = 0;
    bax = 0.01; bay = 0.01; baz = 0.01;
    btx = 0.1; bty = 0.001; btz = 0.001;
    int i = 0;
    while (1)
    {
        mutex_.lock();
        i ++;
        sensor_msg::imu msg_imu;
        sensor_msg::Array3d *accel_ptr = msg_imu.mutable_acceleration();
        sensor_msg::Array3d *twist_ptr = msg_imu.mutable_twist();
        sensor_msg::Array4d *attitude_ptr = msg_imu.mutable_attitude();
        msg_imu.set_timestamp(i);
        ax = 0 + random_noise(accel_noise) + bax - clib_ax; 
        ay = 0 + random_noise(accel_noise) + bay - clib_ay; 
        az = 1 + random_noise(accel_noise) + baz - clib_az;
        tx = 0 + random_noise(twist_noise) + btx - clib_tx; 
        ty = 0 + random_noise(twist_noise) + bty - clib_ty; 
        tz = 0 + random_noise(twist_noise) + btz - clib_tz;
        accel_ptr->set_x(ax);
        accel_ptr->set_y(ay);
        accel_ptr->set_z(az);
        twist_ptr->set_x(tx);
        twist_ptr->set_y(ty);
        twist_ptr->set_z(tz);
        qw += 0.5 * 0.001 * (-tx * qx - ty * qy - tz * qz);
        qx += 0.5 * 0.001 * (tx * qw - ty * qz + tz * qy);
        qy += 0.5 * 0.001 * (tx * qz + ty * qw - tz * qx);
        qz += 0.5 * 0.001 * (-tx * qy + ty * qx + tz * qw);
        attitude_ptr->set_x(qx);
        attitude_ptr->set_y(qy);
        attitude_ptr->set_z(qz);
        attitude_ptr->set_w(qw);
        mutex_.unlock();
        pub.publish(msg_imu);
	    rate.sleep();
    }
    return 0;
}
