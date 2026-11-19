idf_component_register(SRCS "ultrasonic_sensor.cc"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_driver_gpio esp_driver_ledc
                       PRIV_REQUIRES esp_common esp_pm esp_rom esp_timer freertos log)
