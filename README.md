Example Source Code
===================

This is the example source code for [Building a Game Network Protocol](http://gafferongames.com/building-a-game-network-protocol/).

My goal for this source code is to provide you with self contained implementations of key concepts that you can study, rip apart and learn from. There is a lot to say for having a working implementation or testbed demonstrating just one concept at a time, vs. a large lump of library code that requires you to take the whole library or leave it, so I have done my best to keep each example simple and self-contained vs. relying on a lot of external pieces.

You will still find some extremely lightweight library-like components in protocol2.h and network2.h for functionality which is common across all samples, but I did my best to keep this down to the bare minimum. My hope is that if you look at each example you can understand the concepts for that article, and potentially rip that code and modify it easily to do what you want to do.

Although I have done my best to make the code as correct as possible, please be aware that this example source code is **not** production ready. This example source code is really my personal R&D testbeds, experiments and things I tried along the way while writing this article series. Ultimately, all this work is funneled into creating [libyojimbo](http://gafferongames.com/2016/06/17/introducing-libyojimbo/), which is a hardened, library version of the network protocol described in this series.

If you would like production ready code to use in your game, please consider using source from libyojimbo instead.

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

    premake5 003            // build and run packet fragmentation and reassembly example

... and so on. 

    ls -al *.cpp
    
To see the full set of example source that you can build and run.

Each example source code corresponds to an article in the series with the exception of "Packet Aggregation". 

I intended to write an article on this subject for quite some time, but eventually I realized it would be the most boring article on game networking ever written (I really don't think I can fill 3-4 pages on "Packet Aggregation") so it was cut. The example source code is still good though, so I left it in.

cheers 

- Glenn
