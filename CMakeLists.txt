cmake_minimum_required(VERSION 3.10)

add_subdirectory(deps)

add_library(zorro STATIC
  ZVMOps.cpp
  ZorroParser.cpp
  FileReader.cpp
  CodeGenerator.cpp
  ZorroVM.cpp
  Debug.cpp
  ZMap.cpp
  ZSet.cpp
  ZMemory.cpp
  ZorroLexer.cpp
  ZVMStd.cpp
  ZString.cpp
  Symbolic.cpp
  SynTree.cpp
  MacroExpander.cpp
  Debugger.cpp
  #HeapTracer.cpp
)

target_link_libraries( zorro kst)

add_executable( zorro-bin zorro.cpp )

set_target_properties(zorro-bin PROPERTIES OUTPUT_NAME zorro )
target_link_libraries( zorro-bin zorro )

if(MSVC)
  target_compile_definitions(zorro PRIVATE -D_CRT_SECURE_NO_WARNINGS -D_CRT_SECURE_NO_DEPRECATE)
  target_compile_definitions(zorro-bin PRIVATE -D_CRT_SECURE_NO_WARNINGS -D_CRT_SECURE_NO_DEPRECATE)
  target_compile_options(zorro PRIVATE /wd4710 /wd4514 /wd4820 /wd4201 /wd4061 /wd4355 /wd4571)
endif()

if(NOT ZVM_STATIC_MATRIX)
  target_compile_definitions(zorro PUBLIC -DZVM_STATIC_MATRIX)
endif()


if(CMAKE_BUILD_TYPE MATCHES Debug)
  target_compile_definitions(zorro PRIVATE -DDEBUG)
  target_compile_definitions(zorro-bin PRIVATE -DDEBUG)
  target_compile_options(zorro PRIVATE -Wall)
endif()


target_include_directories(zorro PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/>
)

