idf_component_register(
    SRCS "display.cpp"
    INCLUDE_DIRS "include"
    REQUIRES board esp_lcd esp_lcd_axs15231b
    PRIV_REQUIRES esp_driver_spi heap freertos
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
