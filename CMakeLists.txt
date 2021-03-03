cmake_minimum_required(VERSION 2.8)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
  else()
    message(FATAL_ERROR "Please define VITASDK to point to your SDK path!")
  endif()
endif()

project(vmass)
include("${VITASDK}/share/vita.cmake" REQUIRED)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-q -Wall -O3 -fno-inline")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti -fno-exceptions")

add_executable(vmass
  src/main.c
  src/vmass.c
  src/vmass_sysevent.c
  src/fat.c
)

target_link_libraries(vmass
  SceCpuForDriver_stub
  SceDebugForDriver_stub
  SceDmacmgrForDriver_stub
  SceIofilemgrForDriver_stub
  SceThreadmgrForDriver_stub
  SceModulemgrForDriver_stub
  SceSysconForDriver_stub
  SceSysmemForDriver_stub
  SceSysclibForDriver_stub
  SceKernelSuspendForDriver_stub
)

set_target_properties(vmass
  PROPERTIES LINK_FLAGS "-nostdlib"
)

vita_create_self(vmass.skprx vmass
  CONFIG exports.yml
  UNSAFE
)
