idf_component_register(SRCS "telegram.cc"
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES esp_http_client esp_netif esp_timer mbedtls freertos log
                                     espressif__cjson)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_23)
