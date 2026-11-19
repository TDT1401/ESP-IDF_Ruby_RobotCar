idf_component_register(SRCS "motor_controller.cc"
                            "motor_output.cc"
                            "motion_executor.cc"
                            "speed_control.cc"
                            "odometry_control.cc"
                            "sensor_behavior.cc"
                            "web_control.cc"
                            "commands/command_interface.cc"
                            "commands/motion_goal_command.cc"
                       INCLUDE_DIRS "include"
                       PRIV_INCLUDE_DIRS "."
                       REQUIRES motor_driver wheel_encoder imu ultrasonic_sensor
                       PRIV_REQUIRES esp_pm esp_timer esp_system esp_hw_support log)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_23)
