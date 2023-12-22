#ifndef IMU_HPP
#define IMU_HPP

#include <mip/mip_all.hpp>
#include <mip/mip_device.hpp>
#include "mip/platform/serial_connection.hpp"
#include <array>
#include <chrono>
#include <thread>

using namespace mip;
std::mutex _imu_mutex;
mip::Timestamp getCurrentTimestamp()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>( steady_clock::now().time_since_epoch() ).count();
}

struct Utils
{
    std::unique_ptr<mip::Connection> connection;
    std::unique_ptr<mip::DeviceInterface> device;
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
// the function used in mip inner function

struct Accel
{
    public:
        double x = 0;
        double y = 0;
        double z = 0;
};
struct AngVel
{
    public:
        double x = 0;
        double y = 0;
        double z = 0;
};
struct Attitude
{
    public:
        double w = 1;
        double x = 0;
        double y = 0;
        double z = 0;
};

class CX5_AHRS
{
    private:
        data_sensor::ScaledGyro raw_gyro;
        data_sensor::ScaledAccel raw_accel;
        data_filter::AttitudeQuaternion attitude;
        data_filter::Timestamp    filter_time;
        data_filter::Status       filter_status;
        data_filter::QuaternionAttitudeUncertainty attitude_covariance;
        uint16_t sensor_sample_rate;
        uint16_t filter_sample_rate;
        std::unique_ptr<Utils> utils;
        
    public:
        Accel acceleration;
        AngVel angular_velocity;
        Attitude quaternion;
        CX5_AHRS(std::string port, uint32_t baud, uint16_t _sensor_sample_rate, uint16_t _filter_sample_rate) 
        {
            utils = assign_serial(port, baud);
            sensor_sample_rate = _sensor_sample_rate;
            filter_sample_rate = _filter_sample_rate;
        }
        void setup()
        {
            std::unique_ptr<mip::DeviceInterface>& device = utils->device;
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
            float sensor_to_vehicle_transformation_euler[3] = {0, 0, 0}; // M_PI, 0, 0
            if(commands_filter::writeSensorToVehicleRotationEuler(*device, sensor_to_vehicle_transformation_euler[0], sensor_to_vehicle_transformation_euler[1], sensor_to_vehicle_transformation_euler[2]) != CmdResult::ACK_OK)
                exit_gracefully("ERROR: Could not set sensor-to-vehicle transformation!");
            if(commands_filter::reset(*device) != CmdResult::ACK_OK)
                exit_gracefully("ERROR: Could not reset the filter!");
            DispatchHandler sensor_data_handlers[2];
            device->registerExtractor(sensor_data_handlers[0], &raw_accel);
            device->registerExtractor(sensor_data_handlers[1], &raw_gyro);

            DispatchHandler filter_data_handlers[4];
            device->registerExtractor(filter_data_handlers[0], &filter_time);
            device->registerExtractor(filter_data_handlers[1], &filter_status);
            device->registerExtractor(filter_data_handlers[2], &attitude);
            device->registerExtractor(filter_data_handlers[3], &attitude_covariance);

            if(commands_base::resume(*device) != CmdResult::ACK_OK)
                exit_gracefully("ERROR: Could not resume the device!");
            while (true)
            {
                device->update();
                _imu_mutex.lock();
                acceleration.x = raw_accel.scaled_accel[0];
                acceleration.y = raw_accel.scaled_accel[1];
                acceleration.z = raw_accel.scaled_accel[2];
                angular_velocity.x = raw_gyro.scaled_gyro[0];
                angular_velocity.y = raw_gyro.scaled_gyro[1];
                angular_velocity.z = raw_gyro.scaled_gyro[2];
                quaternion.w = attitude.q[0];
                quaternion.x = attitude.q[1];
                quaternion.y = attitude.q[2];
                quaternion.z = attitude.q[3];
                _imu_mutex.unlock();
            }
        }

        std::vector<double> Attitude_Covariance()
        {
            std::vector<double> qc {attitude_covariance.q[0], attitude_covariance.q[1], attitude_covariance.q[2], attitude_covariance.q[3]};
            return qc;
        }
        void exit_gracefully(const char *message)
        {
            if(message)
                printf("%s\n", message);
            exit(0);
        }

        Accel accel() {
            std::lock_guard<std::mutex> lock(_imu_mutex);
            return this->acceleration;
        }
        AngVel twist() {
            std::lock_guard<std::mutex> lock(_imu_mutex);
            return this->angular_velocity;
        }
        Attitude orient() {
            std::lock_guard<std::mutex> lock(_imu_mutex);
            return this->quaternion;
        }
};

#endif