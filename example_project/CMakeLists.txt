# Example project using GG Flash Manager component
cmake_minimum_required(VERSION 3.16)

# Add the components directory to the component search path
set(EXTRA_COMPONENT_DIRS "../")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(gg_flash_mgr_example)
