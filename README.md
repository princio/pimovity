# Getting started

## On Linux

### Requirements

Spdlog:
```
$ mkdir spdlog && cd spdlog

$ mkdir release

$ git clone https://github.com/gabime/spdlog.git

$ cd spdlog

$ mkdir build

$ cmake -H. -B build -DCMAKE_INSTALL_PREFIX=../release -DCMAKE_BUILD_TYPE=Release

$ cmake --build build --target install
```

Other requirements:

```
$ sudo apt install libpthread-stubs0-dev libsystemd-dev libboost-dev libturbojpeg0-dev libusb-1.0-0-dev

$ git clone -b ncsdk2 http://github.com/Movidius/ncsdk && cd ncsdk && make api
```


## On Windows


New Unity Project with Unity 2017.4.20f2.

Add all three Holotoolkit packages.

Apply UWP Project setting via Mixed Reality menu.

Switch in Player Setting .NET version to 4.6 Experimental.

Build settings -> Add scene WithUI -> check C# project -> Build.

Remember to check "allow unsafe code" for Unity VS project called Assmebly-C-Sharp or something like that. Otherwise 
you get "Assembly-C-Sharp ... not found.".
