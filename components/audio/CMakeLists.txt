idf_component_register(SRCS "audio_codec.cc"
                            "audio_debugger.cc"
                            "audio_service.cc"
                            "codecs/no_audio_codec.cc"
                            "demuxer/ogg_demuxer.cc"
                            "engines/afe_audio_engine.cc"
                            "wake_words/custom_wake_word.cc"
                            "wake_words/wake_word_audio_cache.cc"
                       INCLUDE_DIRS "include"
                       PRIV_INCLUDE_DIRS "."
                       REQUIRES esp_driver_gpio esp_driver_i2s esp_system esp_timer freertos lwip
                                espressif__esp-sr espressif__esp_audio_codec
                                espressif__esp_audio_effects
                       PRIV_REQUIRES settings heap log espressif__cjson)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_23)
