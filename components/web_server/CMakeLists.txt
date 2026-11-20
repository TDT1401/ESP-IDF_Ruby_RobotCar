idf_component_register(SRCS "web_server.cc"
                       INCLUDE_DIRS "include"
                       REQUIRES motor_controller espressif__cjson
                       PRIV_REQUIRES esp_http_server esp_system esp_timer log
                       EMBED_FILES "web/index.html" "web/css/style.css" "web/js/app.js")

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_23)
