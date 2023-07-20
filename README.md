# libmav example C++

This is an example on how to use libmav in a simple C++ program

## Getting started

#### Clone this repo & submodules
```
git clone https://github.com/Auterion/libmav-example.git
git submodule update --init --recursive
```

#### Build
```
mkdir build && cd build
cmake ..
make
```

#### Run
The example sets up a MAVLink server at port `14550`.
It will only work if you have a MAVLink stream reaching your computer on that port. 
The easiest way to do that, is to launch PX4 SITL on your computer. 
```
./libmav-example
```

**Look into [main.cpp](main.cpp) for instructions and example code**
