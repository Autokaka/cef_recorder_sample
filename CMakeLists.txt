cmake_minimum_required(VERSION 3.21)
project(pup VERSION 1.0.0 LANGUAGES CXX)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(USE_SANDBOX OFF)
set(CEF_ROOT "${PROJECT_SOURCE_DIR}/vendor/cef")
list(APPEND CMAKE_MODULE_PATH "${CEF_ROOT}/cmake")

find_package(CEF REQUIRED)
add_subdirectory(${CEF_LIBCEF_DLL_WRAPPER_PATH} libcef_dll_wrapper)
SET_CEF_TARGET_OUT_DIR()

file(GLOB PUP_SHARED_SOURCES CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/src/shared/*")
file(GLOB PUP_APP_SOURCES CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/src/app/*")
file(GLOB PUP_HELPER_SOURCES CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/src/helper/*")

set(PUP_SOURCES ${PUP_APP_SOURCES} ${PUP_SHARED_SOURCES})
set(PUP_HELPER_SRCS ${PUP_HELPER_SOURCES} ${PUP_SHARED_SOURCES})

if(OS_MAC)
  set(PUP_OUTPUT_NAME "pup")
  set(PUP_BUNDLE_ID "com.pup.recorder")
  set(CEF_HELPER_APP_SUFFIXES
    "::" " (Alerts):_alerts:.alerts" " (GPU):_gpu:.gpu"
    " (Plugin):_plugin:.plugin" " (Renderer):_renderer:.renderer")

  add_executable(${PUP_OUTPUT_NAME} MACOSX_BUNDLE ${PUP_SOURCES})
  SET_EXECUTABLE_TARGET_PROPERTIES(${PUP_OUTPUT_NAME})
  target_link_libraries(${PUP_OUTPUT_NAME} PRIVATE libcef_dll_wrapper ${CEF_STANDARD_LIBS})
  target_include_directories(${PUP_OUTPUT_NAME} PRIVATE "${PROJECT_SOURCE_DIR}/src")
  set_target_properties(${PUP_OUTPUT_NAME} PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST "${PROJECT_SOURCE_DIR}/res/mac/Info.plist"
    MACOSX_BUNDLE_BUNDLE_NAME "${PUP_OUTPUT_NAME}"
    MACOSX_BUNDLE_GUI_IDENTIFIER "${PUP_BUNDLE_ID}"
    OUTPUT_NAME "${PUP_OUTPUT_NAME}")

  set(CEF_APP "${CEF_TARGET_OUT_DIR}/${PUP_OUTPUT_NAME}.app")
  COPY_MAC_FRAMEWORK(${PUP_OUTPUT_NAME} "${CEF_BINARY_DIR}" "${CEF_APP}")

  foreach(_suffix_list ${CEF_HELPER_APP_SUFFIXES})
    string(REPLACE ":" ";" _suffix_list ${_suffix_list})
    list(GET _suffix_list 0 _name_suffix)
    list(GET _suffix_list 1 _target_suffix)
    list(GET _suffix_list 2 _plist_suffix)

    set(_helper_target "${PUP_OUTPUT_NAME}_helper${_target_suffix}")
    set(_helper_output_name "${PUP_OUTPUT_NAME} Helper${_name_suffix}")
    set(_helper_bundle_id "${PUP_BUNDLE_ID}.helper${_plist_suffix}")

    add_executable(${_helper_target} MACOSX_BUNDLE ${PUP_HELPER_SRCS})
    SET_EXECUTABLE_TARGET_PROPERTIES(${_helper_target})
    target_link_libraries(${_helper_target} PRIVATE libcef_dll_wrapper ${CEF_STANDARD_LIBS})
    target_include_directories(${_helper_target} PRIVATE "${PROJECT_SOURCE_DIR}/src")
    set_target_properties(${_helper_target} PROPERTIES
      MACOSX_BUNDLE_INFO_PLIST "${PROJECT_SOURCE_DIR}/res/mac/Info-helper${_plist_suffix}.plist"
      MACOSX_BUNDLE_BUNDLE_NAME "${_helper_output_name}"
      MACOSX_BUNDLE_GUI_IDENTIFIER "${_helper_bundle_id}"
      OUTPUT_NAME "${_helper_output_name}")

    set(_helper_app "${CEF_TARGET_OUT_DIR}/${_helper_output_name}.app")
    add_custom_command(TARGET ${PUP_OUTPUT_NAME} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_directory "${_helper_app}"
        "${CEF_APP}/Contents/Frameworks/${_helper_output_name}.app"
      VERBATIM)
    add_dependencies(${PUP_OUTPUT_NAME} ${_helper_target})
  endforeach()
endif()
