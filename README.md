Building a Game Network Protocol Example Source Code
====================================================

Thanks for your support!

Here is the example source code for [Building a Game Network Protocol](http://gafferongames.com/building-a-game-network-protocol/).

My goal for this source code is to provide you with self contained implementations of key concepts from each article that you can study, rip apart and learn from. 

I have done my best to keep each example lightweight self contained, and resisted the temptation to turn too much of it into library like code.

You will still find some extremely lightweight library like components in protocol2.h and network2.h, for functionality which is common across all samples, 
but I did my best to keep this to the bare minimum so each sample stands on its own.

Although I have done my best to make the code as correct as possible, this source code is not production ready.

If you would like production ready code to use in your game, please consider [libyojimbo](http://gafferongames.com/2016/06/17/introducing-libyojimbo/) instead.

cheers

- Glenn

## Building on Windows

Download [premake 5](https://premake.github.io/download.html) and copy the **premake5** executable somewhere in your path.

If you don't have Visual Studio 2015 you can [download the community edition for free](https://www.visualstudio.com/en-us/downloads/download-visual-studio-vs.aspx).

Go to the command line under the libyojimbo directory and type:

    premake5 solution

This creates Yojimbo.sln and a bunch of project files then opens them in Visual Studio for you.

Now you can build the library and run individual test programs as you would for any other visual studio solution.

## Building on MacOSX and Linux

Download [premake 5](https://premake.github.io/download.html) then build and install from source.

Next install libsodium.

On MacOS X, this can be done "brew install libsodium". If you don't have Brew, you can install it from <http://brew.sh>.

On Linux, depending on your particular distribution there may be prebuilt packages for libsodium, or you may have to build from source from here [libsodium](https://github.com/jedisct1/libsodium/releases).

Next go to the command line under the libyojimbo directory and enter:

    premake5 gmake

This creates makefiles which you can use to build the source via "make all", or if you prefer, via the following shortcuts:

    premake5 test           // build and run unit tests

    premake5 001            // build and run reading and writing packets example

    premake5 002            // build and run serialization strategies example

    premake5 002            // build and run packet fragmentation and reassembly example

... and so on.

## Run a yojimbo server inside Docker
