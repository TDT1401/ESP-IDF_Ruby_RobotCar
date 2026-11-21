idf_component_register(SRCS "motor_controller.cc"
                            "manual/manual_control.cc"
                            "distance/distance_drive.cc"
                            "control/motion_executor.cc"
                            "control/motor_output.cc"
                            "control/speed_control.cc"
                            "odometry/odometry_control.cc"
                            "safety/sensor_behavior.cc"
                       INCLUDE_DIRS "include"
                       PRIV_INCLUDE_DIRS "."
                       REQUIRES motor_driver wheel_encoder imu ultrasonic_sensor
                       PRIV_REQUIRES esp_pm esp_timer esp_system esp_hw_support freertos log)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_23)
