cmake_minimum_required(VERSION 3.21)

project(pup VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CEF_ROOT "${PROJECT_SOURCE_DIR}/vendor/cef")
list(APPEND CMAKE_MODULE_PATH "${CEF_ROOT}/cmake")
find_package(CEF REQUIRED)

add_subdirectory(${CEF_LIBCEF_DLL_WRAPPER_PATH} libcef_dll_wrapper)

add_executable(pup
  src/main.cc
  src/cef_app.cc
  src/devtools_observer.cc
  src/offscreen_client.cc
  src/recorder.cc
)

target_include_directories(pup
  PRIVATE
    src
    ${CEF_ROOT}
)

SET_EXECUTABLE_TARGET_PROPERTIES(pup)

if(OS_MAC)
  set(CEF_FRAMEWORK_SRC "${CEF_ROOT}/Release/Chromium Embedded Framework.framework")

  ADD_LOGICAL_TARGET(
    "libcef"
    "${CEF_FRAMEWORK_SRC}/Chromium Embedded Framework"
    "${CEF_FRAMEWORK_SRC}/Chromium Embedded Framework"
  )
endif()

target_link_libraries(pup
  PRIVATE
    libcef
    libcef_dll_wrapper
    ${CEF_STANDARD_LIBS}
)

target_compile_definitions(pup
  PRIVATE
    CEF_FRAMEWORK_PATH="${CMAKE_BINARY_DIR}/Frameworks/Chromium Embedded Framework.framework"
)

set_target_properties(pup PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
  BUILD_RPATH "${CMAKE_BINARY_DIR}/Frameworks;${CMAKE_BINARY_DIR}/Frameworks/Chromium Embedded Framework.framework/Libraries"
)

if(OS_MAC)
  set(CEF_FRAMEWORK_DST "${CMAKE_BINARY_DIR}/Frameworks/Chromium Embedded Framework.framework")

  add_custom_command(TARGET pup POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/Frameworks"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      "${CEF_FRAMEWORK_SRC}"
      "${CEF_FRAMEWORK_DST}"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      "${CEF_FRAMEWORK_SRC}/Libraries"
      "${CMAKE_BINARY_DIR}/bin"
    COMMENT "Copying CEF framework and libraries"
    VERBATIM
  )
endif()
