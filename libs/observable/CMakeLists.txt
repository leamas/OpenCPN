cmake_minimum_required(VERSION 3.10.0)

if (TARGET observable::observable)
  return ()
endif ()

set(SRC
  include/observe/eventvar.h
  include/observe/globalvar.h
  include/observe/configvar.h
  include/observe/event.h
  include/observe/observable.h
  src/configvar.cpp
  src/observable.cpp
)
add_library(_OBSERVABLE STATIC ${SRC})
add_library(observable::observable ALIAS _OBSERVABLE)

if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang|GNU")  # Apple is AppleClang
  target_compile_options(_OBSERVABLE PRIVATE -fvisibility=default -fPIC)
endif ()

target_include_directories(
  _OBSERVABLE PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)
