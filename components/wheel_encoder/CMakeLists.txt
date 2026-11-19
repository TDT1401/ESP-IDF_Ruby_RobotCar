idf_component_register(SRCS "wheel_encoder.cc"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_common esp_driver_pcnt soc
                       PRIV_REQUIRES esp_driver_gpio log)
