# cmake needs this line
cmake_minimum_required(VERSION 3.1)

# Enable C++11
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_BUILD_TYPE DEBUG)

SET(OpenCV_DIR /home/developer/opencvs/OpenCV-4.1.0/bin/lib/cmake/opencv4)

# Define project name
project( camera_calibration )

# Find OpenCV, you may need to set OpenCV_DIR variable
# to the absolute path to the directory containing OpenCVConfig.cmake file
# via the command line or GUI
find_package(OpenCV REQUIRED)

# If the package has been found, several variables will
# be set, you can find the full list with descriptions
# in the OpenCVConfig.cmake file.
# Print some message showing some of them
message(STATUS "OpenCV library status:")
message(STATUS "    config: ${OpenCV_DIR}")
message(STATUS "    version: ${OpenCV_VERSION}")
message(STATUS "    libraries: ${OpenCV_LIBS}")
message(STATUS "    include path: ${OpenCV_INCLUDE_DIRS}")

include_directories( ${OpenCV_INCLUDE_DIRS} )

# Declare the executable target built from your sources
add_executable(camera_calibration camera_calibration.cpp)

# Link your application with OpenCV libraries
target_link_libraries(camera_calibration ${OpenCV_LIBS})
