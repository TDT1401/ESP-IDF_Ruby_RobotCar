idf_component_register(SRCS "settings.cc"
                       INCLUDE_DIRS "include"
                       REQUIRES nvs_flash
                       PRIV_REQUIRES log)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_23)
