## Prerequisits:
Login to the target and install ethercat master from sources of  [IgH master](https://gitlab.com/etherlab.org/ethercat) into filesystem.\
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
