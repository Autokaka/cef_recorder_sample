
cmake_minimum_required(VERSION 3.21)

project(pup_cef_sample)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

# Point CMake at the bundled CEF distribution.
set(CEF_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/cef")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CEF_ROOT}/cmake")

# Load CEF configuration (compiler flags, helper macros, etc).
find_package(CEF REQUIRED)

# Build the CEF C++ wrapper library.
add_subdirectory(${CEF_LIBCEF_DLL_WRAPPER_PATH} libcef_dll_wrapper)

set(CEF_APP_TARGET pup_cef_sample)
set(CEF_APP_SOURCES
		${CMAKE_CURRENT_SOURCE_DIR}/main.cc
)

add_executable(${CEF_APP_TARGET} ${CEF_APP_SOURCES})
SET_EXECUTABLE_TARGET_PROPERTIES(${CEF_APP_TARGET})

# Create an imported target for the CEF framework (macOS).
if(OS_MAC)
	ADD_LOGICAL_TARGET(
		"libcef"
		"${CEF_BINARY_DIR_DEBUG}/Chromium Embedded Framework.framework/Chromium Embedded Framework"
		"${CEF_BINARY_DIR_RELEASE}/Chromium Embedded Framework.framework/Chromium Embedded Framework"
	)
endif()

target_link_libraries(${CEF_APP_TARGET}
	libcef
	libcef_dll_wrapper
	${CEF_STANDARD_LIBS}
)

set(CEF_FRAMEWORK_SRC "${CEF_ROOT}/Release/Chromium Embedded Framework.framework")
set(CEF_FRAMEWORK_DST "${CMAKE_BINARY_DIR}/Frameworks/Chromium Embedded Framework.framework")
set(CEF_FRAMEWORK_LIBS_SRC "${CEF_FRAMEWORK_SRC}/Libraries")

# Pass the framework location to the binary so it can load CEF at runtime.
target_compile_definitions(${CEF_APP_TARGET} PRIVATE
	CEF_FRAMEWORK_PATH="${CMAKE_BINARY_DIR}/Frameworks/Chromium Embedded Framework.framework"
)

# Place build outputs under bin/ and add an rpath to find the copied framework.
set_target_properties(${CEF_APP_TARGET} PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
	RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin"
	RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
	BUILD_RPATH "${CMAKE_BINARY_DIR}/Frameworks;${CMAKE_BINARY_DIR}/Frameworks/Chromium Embedded Framework.framework/Libraries"
)

if(OS_MAC)
	add_custom_command(TARGET ${CEF_APP_TARGET} POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/Frameworks"
		COMMAND ${CMAKE_COMMAND} -E copy_directory "${CEF_FRAMEWORK_SRC}" "${CEF_FRAMEWORK_DST}"
		COMMAND ${CMAKE_COMMAND} -E copy_directory "${CEF_FRAMEWORK_LIBS_SRC}" "${CMAKE_BINARY_DIR}/bin"
		VERBATIM
	)
endif()
