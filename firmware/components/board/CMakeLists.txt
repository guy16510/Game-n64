idf_component_register(
    SRCS "board.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_i2c
    PRIV_REQUIRES esp_driver_gpio esp_io_expander_tca9554 freertos
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
