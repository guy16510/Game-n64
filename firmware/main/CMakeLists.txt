idf_component_register(
    SRCS "main.cpp"
    INCLUDE_DIRS "."
    REQUIRES game_core
    PRIV_REQUIRES spi_flash heap esp_system
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
