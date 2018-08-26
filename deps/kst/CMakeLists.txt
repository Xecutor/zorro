cmake_minimum_required(VERSION 3.0)

add_library( kst STATIC src/Logger.cpp src/Format.cpp)

if(MSVC)
  target_compile_definitions(kst PRIVATE -D_CRT_NONSTDC_NO_DEPRECATE -D_CRT_SECURE_NO_WARNINGS)
  target_compile_options(kst PRIVATE /wd4710 /wd4514 /wd4820)
  target_link_libraries(kst ws2_32)
endif()


target_include_directories(kst PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include/kst>
)
