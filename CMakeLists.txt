cmake_minimum_required(VERSION 3.24)
project(ProjectionMappingStudio VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets)
qt_standard_project_setup()
qt_add_executable(ProjectionMappingStudio WIN32
    src/main.cpp
)
target_link_libraries(ProjectionMappingStudio PRIVATE Qt6::Widgets)
set_target_properties(ProjectionMappingStudio PROPERTIES
    WIN32_EXECUTABLE TRUE
)
install(TARGETS ProjectionMappingStudio RUNTIME DESTINATION bin)
