idf_component_register(SRCS "imu.cc"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_common esp_driver_gpio
                       PRIV_REQUIRES esp_driver_i2c esp_timer freertos log)
