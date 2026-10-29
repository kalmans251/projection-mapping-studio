cmake_minimum_required(VERSION 3.24)
project(ProjectionMappingStudio VERSION 0.2.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets Multimedia)
qt_standard_project_setup()
qt_add_executable(ProjectionMappingStudio WIN32
    src/main.cpp
    src/OutputController.h
)
target_link_libraries(ProjectionMappingStudio PRIVATE Qt6::Widgets Qt6::Multimedia)
set_target_properties(ProjectionMappingStudio PROPERTIES WIN32_EXECUTABLE TRUE)
install(TARGETS ProjectionMappingStudio RUNTIME DESTINATION bin)
