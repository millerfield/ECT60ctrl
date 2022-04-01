## Prerequisits:
Install ethercat from sources of IgH master into filesystem.\

Install the needed libraries in the targets arch.
```
sudo apt-get install libpigpio-dev libpigpio1 libncurses-dev 
```
## Build:
```
mkdir build\
cd build\
cmake .. -DCMAKE_PREFIX_PATH=/usr/local/lib\
```
