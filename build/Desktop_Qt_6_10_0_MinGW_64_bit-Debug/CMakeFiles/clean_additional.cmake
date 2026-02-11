# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Debug")
  file(REMOVE_RECURSE
  "CMakeFiles\\SchedulingApp_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\SchedulingApp_autogen.dir\\ParseCache.txt"
  "SchedulingApp_autogen"
  )
endif()
