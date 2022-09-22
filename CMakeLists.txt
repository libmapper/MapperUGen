set(FILENAME "Mapper.cpp") #specify the .cpp file here
cmake_minimum_required (VERSION 2.8)
get_filename_component(PROJECT ${FILENAME} NAME_WE) #automatically sets project name from the filename
# set(PROJECT "my_name") #alternatively set project name manually
message(STATUS "Project name is ${PROJECT}")
project (${PROJECT})

if (WIN32)
# EDIT LIBMAPPER_DIR PATH BELOW BEFORE COMPILING FOR WINDOWS
set(LIBMAPPER_DIR "C:/Users/brady/Documents/Github/libmapper")
set(LIBMAPPER_BUILD_DIR "${LIBMAPPER_DIR}/build/Release")
set(LIBLO_DIR "${LIBMAPPER_DIR}/build/liblo/liblo-master")
set(LIBLO_BUILD_DIR "${LIBLO_DIR}/cmake/build/Release")
set(LIBLO_INCLUDES "${LIBLO_DIR}/cmake/build;${LIBLO_DIR}")

set(Liblo_LIB ${LIBLO_BUILD_DIR}/liblo.lib)
set(Libmapper_LIB ${LIBMAPPER_BUILD_DIR}/libmapper.lib)
mark_as_advanced(Liblo_LIB)
mark_as_advanced(Libmapper_LIB)
add_definitions(
    -D_WINSOCK_DEPRECATED_NO_WARNINGS
    -DHAVE_WINSOCK2_H
    -DNODEFAULTLIB
)
include_directories(
  "${LIBLO_INCLUDES}"
  "${LIBMAPPER_DIR}/include"
)
set(MAPPER_LIB ${Liblo_LIB} ${Libmapper_LIB})
else()
  find_library(MAPPER_LIB mapper)
  if(NOT MAPPER_LIB)
    message(FATAL_ERROR "libmapper library not found")
  endif(NOT MAPPER_LIB)
  include_directories(/usr/local/include)
endif(WIN32)

if (APPLE)
  set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}")
endif()

find_path(SC_PATH NAMES plugin_interface/SC_PlugIn.h PATH_SUFFIXES SuperCollider)
if(NOT SC_PATH)
    set(SC_PATH supercollider)
    message("Using submodule, make sure that this repository is cloned with the --recursive option")
else()
    message("Found SuperCollider headers: ${SC_PATH}")
endif()

include_directories(${SC_PATH}/plugin_interface)
include_directories(${SC_PATH}/include/plugin_interface)
include_directories(${SC_PATH}/include/common)
include_directories(${SC_PATH}/common)

if (CMAKE_SYSTEM_NAME MATCHES "Linux|.*BSD|DragonFly")
    set(INSTALL_DESTINATION "lib/SuperCollider/plugins")
    if (QUARKS)
        set(INSTALL_DESTINATION_DISTRO
                "share/SuperCollider/MapperUGen")
    else()
        set(INSTALL_DESTINATION_DISTRO
                "share/SuperCollider/Extensions/MapperUGen")
    endif(QUARKS)
else()
    set(INSTALL_DESTINATION "MapperUGen")
    set(INSTALL_DESTINATION_DISTRO "MapperUGen")
endif()

set(PLUGIN_DIR "plugins")

set(CMAKE_SHARED_MODULE_PREFIX "")
if(APPLE OR WIN32)
set(CMAKE_SHARED_MODULE_SUFFIX ".scx")
endif()

option(SUPERNOVA "Build plugins for supernova" OFF)

option(CPP11 "Build with c++11." ON)

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_COMPILER_IS_CLANG 1)
endif()

# The below monstrosity sets compiler flags for SSE (Streaming SIMD Extensions). Among many other things, SSE protects
# against denormal numbers, which are known to cause glitchy audio in some cases.

# Some processors do not support SSE and will produce errors if you try to add SSE flags, so we also have to introduce
# conditional logic to guard against those errors.

# It's a real mess, and generally there's no need to touch it :)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_COMPILER_IS_CLANG)
    add_definitions(-fvisibility=hidden)

    include (CheckCCompilerFlag)
    include (CheckCXXCompilerFlag)

    CHECK_C_COMPILER_FLAG(-msse HAS_SSE)
    CHECK_CXX_COMPILER_FLAG(-msse HAS_CXX_SSE)

    if (HAS_SSE)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -msse")
    endif()
    if (HAS_CXX_SSE)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse")
    endif()

    CHECK_C_COMPILER_FLAG(-msse2 HAS_SSE2)
    CHECK_CXX_COMPILER_FLAG(-msse2 HAS_CXX_SSE2)

    if (HAS_SSE2)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -msse2")
    endif()
    if (HAS_CXX_SSE2)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse2")
    endif()

    CHECK_C_COMPILER_FLAG(-mfpmath=sse HAS_FPMATH_SSE)
    CHECK_CXX_COMPILER_FLAG(-mfpmath=sse HAS_CXX_FPMATH_SSE)

    if (HAS_FPMATH_SSE)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mfpmath=sse")
    endif()
    if (HAS_CXX_FPMATH_SSE)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mfpmath=sse")
    endif()

    if(NATIVE)
        add_definitions(-march=native)
    endif()

    # Plugin headers require C++11, which must be explicitly enabled for gcc and clang.
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
    if(CMAKE_COMPILER_IS_CLANG)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
    endif()
endif()
if(MINGW)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mstackrealign")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mstackrealign")
endif()

add_library(${PROJECT} MODULE ${FILENAME})
target_link_libraries(${PROJECT} ${MAPPER_LIB} ${Boost_libraries})

if(WIN32)
  set_target_properties(${PROJECT} PROPERTIES LINK_FLAGS "/NODEFAULTLIB:MSVCRTD")
  set_target_properties(${PROJECT} PROPERTIES LINK_FLAGS "/INCREMENTAL:NO")
endif(WIN32)

if(SUPERNOVA)
    add_library(${PROJECT}_supernova MODULE ${FILENAME})
    target_link_libraries(${PROJECT}_supernova ${MAPPER_LIB})
    set_property(TARGET ${PROJECT}_supernova
                 PROPERTY COMPILE_DEFINITIONS SUPERNOVA)
    if(NOT WIN32)
    install(TARGETS ${PROJECT}_supernova
            DESTINATION ${INSTALL_DESTINATION}/${PLUGIN_DIR})
    endif(NOT WIN32)
endif()

if(NOT WIN32)
install(TARGETS ${PROJECT}
        DESTINATION ${INSTALL_DESTINATION}/${PLUGIN_DIR})
install(DIRECTORY "sc/" DESTINATION "${INSTALL_DESTINATION_DISTRO}"
        PATTERN "*")
endif(NOT WIN32)