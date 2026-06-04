# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Release")
  file(REMOVE_RECURSE
  "Nuummite\\CMakeFiles\\voice_client_autogen.dir\\AutogenUsed.txt"
  "Nuummite\\CMakeFiles\\voice_client_autogen.dir\\ParseCache.txt"
  "Nuummite\\voice_client_autogen"
  )
endif()
