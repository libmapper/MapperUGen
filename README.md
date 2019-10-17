# MapperUGen
A SuperCollider UGen for using libmapper

## Installation
* Build and install [libmapper](https://github.com/libmapper/libmapper)
* Unzip MapperUGen.zip from [releases](https://github.com/mathiasbredholt/MapperUGen/releases) into SuperCollider extensions folder (Platform.userExtensionDir)

## Compile from source
* Build and install [libmapper](https://github.com/libmapper/libmapper)
* Run
```
mkdir build && cd build
cmake -DSC_PATH=/path/to/sc/ ..
cmake --build . --target install
```

### CMake options
* Build supernova-plugin
* (default 'OFF')
* ```cmake -DSUPERNOVA=ON ..```