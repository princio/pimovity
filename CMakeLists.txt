# Set the minimum version of CMake that can be used
# To find the cmake version run
# $ cmake --version
cmake_minimum_required(VERSION 3.5.1)


#---------------------------------------------------------------------------------------
# Compiler config
#---------------------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


# Set the project name
project (gengi)

# Create a sources variable with a link to all cpp files to compile
file(GLOB SOURCES "${PROJECT_SOURCE_DIR}/src/*.cpp")

# Add an executable with the above sources
add_executable(gengi ${SOURCES})

set(spdlog_DIR /home/pi/spdlog/release/lib/spdlog/cmake/)
#set(OpenCV_DIR /home/developer/opencvs/OpenCV-4.1.0/bin/lib/cmake/opencv4/)
find_package(spdlog REQUIRED)
#find_package(OpenCV REQUIRED)

target_link_libraries(gengi PRIVATE spdlog::spdlog mvnc m jpeg turbojpeg ${OpenCV_LIBS})   # -l flags for linking prog target


# Set the directories that should be included in the build command for this target
# when running g++ these will be included as -I/directory/path/
target_include_directories(gengi
    PUBLIC 
        ${PROJECT_SOURCE_DIR}/include
)
