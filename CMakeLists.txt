cmake_minimum_required(VERSION 2.8)

project(psvimgtools C)

option(USE_MINIZ "Use miniz as a replacement of zlib" OFF)

if(NOT MSVC)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99")
  if(WIN32)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -static -static-libgcc")
  endif(WIN32)
endif(NOT MSVC)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")
find_package(Threads)

set(common_src utils.c aes.c aes_x86.c sha256.c)

if(USE_MINIZ)
  set(common_src ${common_src} miniz.c)
  add_definitions(-DUSE_MINIZ)
else(USE_MINIZ)
  find_package(zlib REQUIRED)
  include_directories(${zlib_INCLUDE_DIRS})
endif(USE_MINIZ)

if(NOT MSVC)
  add_definitions(-maes -mssse3)
endif(NOT MSVC)

add_executable(psvimg-extract psvimg-extract.c restore.c ${common_src})
add_executable(psvmd-decrypt psvmd-decrypt.c restore.c ${common_src})
add_executable(psvimg-create psvimg-create.c backup.c ${common_src})

target_link_libraries(psvimg-extract ${zlib_LIBRARIES} Threads::Threads)
target_link_libraries(psvmd-decrypt ${zlib_LIBRARIES} Threads::Threads)
target_link_libraries(psvimg-create ${zlib_LIBRARIES} Threads::Threads)

install(TARGETS psvimg-extract DESTINATION bin)
install(TARGETS psvmd-decrypt DESTINATION bin)
install(TARGETS psvimg-create DESTINATION bin)

if(NOT WIN32 OR CYGWIN)
  if(${LIBGCRYPT_FOUND})
    add_executable(psvimg-keyfind psvimg-keyfind.c ${common_src})
    set_target_properties(psvimg-keyfind PROPERTIES COMPILE_FLAGS "-O3")
    install(TARGETS psvimg-keyfind DESTINATION bin)
  endif(${LIBGCRYPT_FOUND})
endif(NOT WIN32 OR CYGWIN)
