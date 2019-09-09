# Configurations

## WINDOWS

New Unity Project with Unity 2017.4.20f2.

Add all three Holotoolkit packages.

Apply UWP Project setting via Mixed Reality menu.

Switch in Player Setting .NET version to 4.6 Experimental.

Build settings -> Add scene WithUI -> check C# project -> Build.

Remember to check "allow unsafe code" for Unity VS project called Assmebly-C-Sharp or something like that. Otherwise 
you get "Assembly-C-Sharp ... not found.".

Bye

 ## LINUX

~/pimovity-holo/build

 ./gengi -v 0 --port 56789 --iface enp0s8 --ale fe