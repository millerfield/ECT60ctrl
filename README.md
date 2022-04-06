## Prerequisits:
Login to the target and install ethercat master from sources of  [IgH master](https://gitlab.com/etherlab.org/ethercat) into filesystem.\
Also install additional needed libraries to the target:
```
sudo apt-get install libpigpio-dev libpigpio1 libncurses-dev 
```
## Build:
```
mkdir build
cd build
cmake .. -DCMAKE_PREFIX_PATH=/usr/local/lib
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
