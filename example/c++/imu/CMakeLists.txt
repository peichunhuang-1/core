add_executable(IMU_real "imu.cpp")
target_link_libraries(IMU_real
  ${_CORE_LIBRARIES}
  imu_proto mip "${SERIAL_LIB}" Eigen3::Eigen)
target_compile_definitions(IMU_real PUBLIC "MIP_USE_SERIAL")

add_executable(IMU_listen "imu_listen.cpp")
target_link_libraries(IMU_listen
  ${_CORE_LIBRARIES}
  imu_proto)