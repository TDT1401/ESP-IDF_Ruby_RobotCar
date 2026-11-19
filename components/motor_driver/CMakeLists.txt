idf_component_register(SRCS "motor_driver.cc"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_common esp_driver_gpio esp_driver_ledc)
