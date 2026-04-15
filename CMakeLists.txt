cmake_minimum_required(VERSION 4.0)
project(bigint)

set(CMAKE_CXX_STANDARD 17)

add_executable(${PROJECT_NAME} main.cpp)

#include(CheckCXXCompilerFlag)
#
#check_cxx_compiler_flag("-madx" HAS_MADX)
#check_cxx_compiler_flag("-mbmi2" HAS_BMI2)
#
#message(STATUS "HAS_MADX = ${HAS_MADX}")
#message(STATUS "HAS_BMI2 = ${HAS_BMI2}")
#
#if(HAS_MADX AND HAS_BMI2)
#    message(STATUS "Enabling -madx -mbmi2")
#    target_compile_options(${PROJECT_NAME} PRIVATE -madx -mbmi2)
#else()
#    message(STATUS "NOT enabling ADX/BMI2 flags")
#endif()