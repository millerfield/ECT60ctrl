## What is the ECT60ctrl ?
The ECT60ctrl can be used to control a Rtelligent ECT60 EtherCAT servo motor driver connected to an EtherCAT master on Linux.
It's an ncurses based application running on top of the IgH EtherLab EtherCAT master implementation on a Debian Linux like Raspberry Pi OS with a realtime kernel.
The servo motor is driven in the CiA402 operating mode PV (profile velocity) which is used to simply apply different speeds to the servo motor. Additionally some parameters could be applied to optimize the performance of the drive.

## Prerequisits:
Login to the target and install ethercat master from sources of [IgH master](https://gitlab.com/etherlab.org/ethercat) into filesystem.\
Also install additional needed libraries to the target:
```
sudo apt-get install libpigpio-dev libpigpio1 libncurses-dev 
```
Check if user who is running the application is in group root.

## Build:
The application can be build natively on the target or by the help of a cross build environemnt.
For cross building the application, the root filesystem of the target must be mounted to the user directory.
In any case adjust the variable CMAKE_SYSROOT in the toolchain file armhf.cmake to the root filesystem. A native target build requires '/'.
The cmake call needs two cache variables for the build type (Release or Debug) and for the toolchain file as argument. 
```
mkdir _build/Debug
cd _build/Debug
cmake .. -DCMAKE_BUILD_TYPE:STRING=Debug -DCMAKE_TOOLCHAIN_FILE:STRING=armhf.cmake
make
```

## Deploy
Copy over the executable to the target:
```

```

## Execute
To let ECT60ctrl raise realtime scheduling priority as non-root user, set the CAP_SYS_NICE capability of the executable:
```
sudo setcap cap_sys_nice+ep ./ECT60ctrl
./ECT60ctrl
```
