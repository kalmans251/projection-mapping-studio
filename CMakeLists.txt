cmake_minimum_required(VERSION 3.24)
project(ProjectionMappingStudio VERSION 0.5.1 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets Multimedia)
qt_standard_project_setup()
qt_add_executable(ProjectionMappingStudio WIN32
    src/main.cpp
    src/OutputController.h
    src/rendering/D3D11Engine.h
    src/rendering/D3D11Engine.cpp
)
# Prevent Windows headers from defining min/max function-like macros.
target_compile_definitions(ProjectionMappingStudio PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(ProjectionMappingStudio PRIVATE Qt6::Widgets Qt6::Multimedia d3d11 dxgi)
set_target_properties(ProjectionMappingStudio PROPERTIES WIN32_EXECUTABLE TRUE)
install(TARGETS ProjectionMappingStudio RUNTIME DESTINATION bin)
