add_executable(IMU_sim "IMU_sim.cpp")
target_link_libraries(IMU_sim
  ${_CORE_LIBRARIES}
  imu_proto)