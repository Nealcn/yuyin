# The top-level CMakeLists.txt for the M5AtomS3 Voice Stick firmware
cmake_minimum_required(VERSION 3.16)

# Set project name and extra components
set(COMPONENTS main opus)

# Include the ESP-IDF project configuration
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(atoms3_voicestick)
