# MapperUGen
A SuperCollider UGen for using libmapper

## Installation
* Install [SuperCollider](https://supercollider.github.io/)
* Build and install [libmapper](https://github.com/libmapper/libmapper)
* Unzip MapperUGen.zip from [releases](https://github.com/mathiasbredholt/MapperUGen/releases) into SuperCollider extensions folder (Platform.userExtensionDir)

## Compile from source
* Build and install [libmapper](https://github.com/libmapper/libmapper)
* Run
```
git clone --recursive https://github.com/mathiasbredholt/MapperUGen.git
cd MapperUGen
mkdir build && cd build
cmake -DSUPERNOVA=ON ..
cmake --build . --target install
```
