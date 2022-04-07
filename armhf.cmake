set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR armhf)
set(CMAKE_LIBRARY_ARCHITECTURE arm-linux-gnueabihf)
# Set the following var to the mounted root filesystem of the Raspberry Pi OS emulation
set(CMAKE_SYSROOT /home/guest/workspace_rpios)

set(CMAKE_C_COMPILER arm-linux-gnueabihf-gcc)
set(CMAKE_CXX_COMPILER arm-linux-gnueabihf-g++)

# Set paths to different locations for libraries like EtherCat or the armhf libc6 within CMAKE_SYSROOT
set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH};/usr/local/lib;/usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}")

set(CMAKE_FIND_ROOT_PATH "${CMAKE_SYSROOT}")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

