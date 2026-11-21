idf_component_register(SRCS "web_server.cc"
                       INCLUDE_DIRS "include"
                       REQUIRES motor_controller espressif__cjson
                       PRIV_REQUIRES esp_http_server esp_system esp_timer log
                       EMBED_FILES "web/index.html" "web/css/style.css"
                                   "web/js/app.js" "web/js/state.js" "web/js/api.js"
                                   "web/js/actions.js" "web/js/hud.js" "web/js/telemetry.js"
                                   "web/js/trajectory.js" "web/js/ui.js")

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_23)
