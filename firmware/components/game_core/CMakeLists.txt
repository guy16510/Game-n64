set(AP_CORE_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../core")

idf_component_register(
    SRCS "${AP_CORE_DIR}/src/game_state.cpp"
    INCLUDE_DIRS "${AP_CORE_DIR}/include"
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
