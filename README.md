# MapperUGen
A SuperCollider UGen for using libmapper

## Installation
* Install [SuperCollider](https://supercollider.github.io/)
* Build and install [libmapper](https://github.com/libmapper/libmapper)
* Unzip MapperUGen.zip from [releases](https://github.com/mathiasbredholt/MapperUGen/releases) into SuperCollider extensions folder (Platform.userExtensionDir)

## Compile from source
* Build and install [libmapper](https://github.com/libmapper/libmapper)
### GNU/Linux
```
git clone https://github.com/libmapper/MapperUGen.git
cd MapperUGen
mkdir build && cd build
cmake -DSUPERNOVA=ON ..
cmake --build . --target install
```
### macOS
```
git clone --recursive https://github.com/libmapper/MapperUGen.git
cd MapperUGen
mkdir build && cd build
cmake -DSUPERNOVA=ON ..
cmake --build . --target install
```

### Windows
```
git clone https://github.com/libmapper/MapperUGen.git
git config --global url."https://".insteadOf git://
cd MapperUGen
mkdir build
cd build
cmake -DSUPERNOVA=ON ..
cmake --build .
```

#### Windows manual installation after compiling

1. Evaluate `Platform.userExtensionDir` in supercollider and create a "Mapper" folder there
2. Copy everything inside the ./sc folder to the Mapper folder
3. Create a "plugins" folder in the Mapper directory
4. Copy the build outputs from ./build/Debug and your libmapper, liblo and zlib .dlls to the plugins folder
5. Reboot the supercollider interpreter