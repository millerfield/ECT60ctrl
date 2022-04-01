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
