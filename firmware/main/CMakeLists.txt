idf_component_register(
    SRCS "main.cpp"
    INCLUDE_DIRS "."
    REQUIRES game_core
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
