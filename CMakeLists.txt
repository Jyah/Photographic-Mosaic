# set the project name
project(Mosaic)

#
if(COMMAND cmake_policy)
	cmake_policy(SET CMP0003 NEW)
endif(COMMAND cmake_policy)

# add the executable
add_executable(Mosaic main.cpp)


cmake_minimum_required(VERSION 3.10)
find_package(OpenCV REQUIRED)



#Add lib for fs
TARGET_LINK_LIBRARIES(Mosaic stdc++fs)

# Set C++14 support
set_property(TARGET Mosaic PROPERTY CXX_STANDARD 14)
set_property(TARGET Mosaic PROPERTY CXX_STANDARD_REQUIRED ON)

# link opencv library
target_link_libraries( Mosaic ${OpenCV_LIBS} )

# opencv library
find_package(OpenCV) 

