#ifndef IMU_HPP
#define IMU_HPP

#include "mip/mip_all.hpp"
#include "mip/mip_device.hpp"
#include "mip/platform/serial_connection.hpp"
#include <array>
#include <chrono>
#include <thread>
#include <Eigen/Dense>
#include <Eigen/Geometry>
#include "NodeHandler.h"

using namespace mip;  

Timestamp getCurrentTimestamp() {
    using namespace std::chrono;
    return duration_cast<milliseconds>( steady_clock::now().time_since_epoch() ).count();
}
struct Utils {
    std::unique_ptr<Connection> connection;
    std::unique_ptr<DeviceInterface> device;
    uint8_t buffer[1024];
};
std::unique_ptr<Utils> assign_serial(std::string port, uint32_t baud)
{
    auto utils = std::unique_ptr<Utils>(new Utils());
    if( baud == 0 )
        throw std::runtime_error("Serial baud rate must be a decimal integer greater than 0.");

    utils->connection = std::unique_ptr<mip::platform::SerialConnection>(new mip::platform::SerialConnection(port, baud));
    utils->device = std::unique_ptr<mip::DeviceInterface>(new mip::DeviceInterface(utils->connection.get(), utils->buffer, sizeof(utils->buffer), mip::C::mip_timeout_from_baudrate(baud), 500));
    return utils;
}
void exit_gracefully(const char *message)
{
    if(message)
        printf("%s\n", message);
    exit(0);
}
class CX5_AHRS {
    public:
        CX5_AHRS(std::string port, uint32_t baud, uint16_t _sensor_sample_rate, uint16_t _filter_sample_rate) {
            utils = assign_serial(port, baud);
            sensor_sample_rate = _sensor_sample_rate;
            filter_sample_rate = _filter_sample_rate;
        }
        void start() {
            std::unique_ptr<DeviceInterface>& device = utils->device;
            if(commands_base::ping(*device) != CmdResult::ACK_OK)
               exit_gracefully("ERROR: Could not ping the device!");
            if(commands_base::setIdle(*device) != CmdResult::ACK_OK)
                exit_gracefully("ERROR: Could not set the device to idle!");
            if(commands_3dm::defaultDeviceSettings(*device) != CmdResult::ACK_OK)
                exit_gracefully("ERROR: Could not load default device settings!");
            uint16_t sensor_base_rate;
            if(commands_3dm::imuGetBaseRate(*device, &sensor_base_rate) != CmdResult::ACK_OK)
                exit_gracefully("ERROR: Could not get sensor base rate format!");

            const uint16_t sensor_decimation = sensor_base_rate / sensor_sample_rate;
            std::array<DescriptorRate, 2> sensor_descriptors = {{
                { data_sensor::DATA_ACCEL_SCALED,   sensor_decimation },
                { data_sensor::DATA_GYRO_SCALED,    sensor_decimation }
            }};
            if(commands_3dm::writeImuMessageFormat(*device, sensor_descriptors.size(), sensor_descriptors.data()) != CmdResult::ACK_OK)
                exit_gracefully("ERROR: Could not set sensor message format!");
            float sensor_to_vehicle_transformation_euler[3] = {0, 0, 0}; // M_PI, 0, 0
            if(commands_filter::writeSensorToVehicleRotationEuler(*device, sensor_to_vehicle_transformation_euler[0], sensor_to_vehicle_transformation_euler[1], sensor_to_vehicle_transformation_euler[2]) != CmdResult::ACK_OK)
                exit_gracefully("ERROR: Could not set sensor-to-vehicle transformation!");
            
            uint16_t filter_base_rate;

            if(commands_3dm::filterGetBaseRate(*device, &filter_base_rate) != CmdResult::ACK_OK)
                exit_gracefully("ERROR: Could not get filter base rate format!");
            const uint16_t filter_decimation = filter_base_rate / filter_sample_rate;

            std::array<DescriptorRate, 4> filter_descriptors = {{
                { data_filter::DATA_FILTER_TIMESTAMP, filter_decimation },
                { data_filter::DATA_FILTER_STATUS,    filter_decimation },
                { data_filter::DATA_ATT_QUATERNION , filter_decimation },
                { data_filter::DATA_ATT_UNCERTAINTY_QUATERNION, filter_decimation}
            }};
            if(commands_3dm::writeFilterMessageFormat(*device, filter_descriptors.size(), filter_descriptors.data()) != CmdResult::ACK_OK)
                exit_gracefully("ERROR: Could not set filter message format!");

            if(commands_filter::writeAutoInitControl(*device, 1) != CmdResult::ACK_OK)
                exit_gracefully("ERROR: Could not set filter autoinit control!");
            if(commands_filter::reset(*device) != CmdResult::ACK_OK)
                exit_gracefully("ERROR: Could not reset the filter!");
            DispatchHandler sensor_data_handlers[2];
            device->registerExtractor(sensor_data_handlers[0], &raw_accel);
            device->registerExtractor(sensor_data_handlers[1], &raw_gyro);

            DispatchHandler filter_data_handlers[4];
            device->registerExtractor(filter_data_handlers[0], &filter_time);
            device->registerExtractor(filter_data_handlers[1], &filter_status);
            device->registerExtractor(filter_data_handlers[2], &raw_attitude);
            device->registerExtractor(filter_data_handlers[3], &attitude_covariance);

            if(commands_base::resume(*device) != CmdResult::ACK_OK)
                exit_gracefully("ERROR: Could not resume the device!");
            bool filter_state_ahrs = false;
            while(1) {
                device->update();
                if((!filter_state_ahrs))
                {
                    if (filter_status.filter_state == data_filter::FilterMode::AHRS) filter_state_ahrs = true;
                    else continue;
                }
                _imu_mutex.lock();
                acceleration = Eigen::Vector3f(raw_accel.scaled_accel);
                twist = Eigen::Vector3f(raw_gyro.scaled_gyro);
                attitude = Eigen::Quaternionf(raw_attitude.q[0], raw_attitude.q[1], raw_attitude.q[2], raw_attitude.q[3]);
                _imu_mutex.unlock();
            }
        }
        void calibrate(int block_time_ms) {
            Eigen::Vector3f accum_accel(0, 0, 0);
            Eigen::Vector3f accum_twist(0, 0, 0);
            Eigen::Vector4f accum_attitude(0, 0, 0, 0); // xyzw
            core::Rate rate(1000);
            for (int i = 0; i < block_time_ms; i++) {
                _imu_mutex.lock();
                accum_accel += (acceleration - attitude.toRotationMatrix().transpose() * Eigen::Vector3f(0, 0, 1));
                accum_twist += twist;
                accum_attitude += attitude.coeffs();
                _imu_mutex.unlock();
                rate.sleep();
            }
            _imu_mutex.lock();
            acceleration_bias = accum_accel / (float) block_time_ms;
            twist_bias = accum_twist / (float) block_time_ms;
            Eigen::Quaternionf attitude_intial = Eigen::Quaternionf(accum_attitude / (float) block_time_ms);
            float heading = attitude_intial.toRotationMatrix().eulerAngles(0, 1, 2)[2];
            attitude_bias = Eigen::AngleAxisf(0, Eigen::Vector3f::UnitX()) 
                            * Eigen::AngleAxisf(0, Eigen::Vector3f::UnitY()) 
                            * Eigen::AngleAxisf(heading, Eigen::Vector3f::UnitZ());
            _imu_mutex.unlock();
        }
        void get(Eigen::Vector3f &acceleration_, Eigen::Vector3f &twist_, Eigen::Quaternionf &attitude_) {
            _imu_mutex.lock();
            acceleration_ = (acceleration - attitude.toRotationMatrix().transpose() * Eigen::Vector3f(0, 0, 1)) - acceleration_bias;
            twist_ = twist - twist_bias;
            attitude_ = attitude_bias.inverse() * attitude;
            _imu_mutex.unlock();
        }
    private:
        data_sensor::ScaledGyro raw_gyro;
        data_sensor::ScaledAccel raw_accel;
        data_filter::AttitudeQuaternion raw_attitude;
        data_filter::Timestamp    filter_time;
        data_filter::Status       filter_status;
        data_filter::QuaternionAttitudeUncertainty attitude_covariance;
        uint16_t sensor_sample_rate;
        uint16_t filter_sample_rate;
        std::unique_ptr<Utils> utils;
        Eigen::Vector3f acceleration;
        Eigen::Vector3f twist;
        Eigen::Quaternionf attitude;
        Eigen::Vector3f acceleration_bias;
        Eigen::Vector3f twist_bias;
        Eigen::Quaternionf attitude_bias;
        std::mutex _imu_mutex;
        std::unique_ptr<Utils> assign_serial(std::string port, uint32_t baud) {
            auto utils = std::unique_ptr<Utils>(new Utils());
            if( baud == 0 )
                throw std::runtime_error("Serial baud rate must be a decimal integer greater than 0.");

            utils->connection = std::unique_ptr<platform::SerialConnection>(new platform::SerialConnection(port, baud));
            utils->device = std::unique_ptr<DeviceInterface>(new DeviceInterface(utils->connection.get(), utils->buffer, sizeof(utils->buffer), C::mip_timeout_from_baudrate(baud), 500));
            return utils;
        }
};

#endif