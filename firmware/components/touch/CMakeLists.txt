idf_component_register(
    SRCS "touch.cpp"
    INCLUDE_DIRS "include"
    REQUIRES board esp_driver_i2c
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
