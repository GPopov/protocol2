
if os.is "windows" then
    debug_libs = { "sodium-debug" }
    release_libs = { "sodium-release" }
else
    debug_libs = { "sodium" }
    release_libs = debug_libs
end

solution "NetworkProtocol"
    platforms { "x64" }
    includedirs { ".", "vectorial" }
    if not os.is "windows" then
        targetdir "bin/"
    end
    configurations { "Debug", "Release" }
    flags { "ExtraWarnings", "FatalWarnings", "StaticRuntime", "FloatFast" }
    rtti "Off"
    configuration "Debug"
        flags { "Symbols" }
        defines { "DEBUG" }
    configuration "Release"
        optimize "Speed"
        defines { "NDEBUG" }

project "test"
    language "C++"
    kind "ConsoleApp"
    files { "test.cpp", "protocol2.h", "network2.h" }
    configuration "Debug"
        links { debug_libs }
    configuration "Release"
        links { release_libs }

project "001_reading_and_writing_packets"
    language "C++"
    kind "ConsoleApp"
    files { "001_reading_and_writing_packets.cpp", "protocol2.h" }

project "002_serialization_strategies"
    language "C++"
    kind "ConsoleApp"
    files { "002_serialization_strategies.cpp", "protocol2.h" }

project "003_packet_fragmentation_and_reassembly"
    language "C++"
    kind "ConsoleApp"
    files { "003_packet_fragmentation_and_reassembly.cpp", "protocol2.h" }

project "004_sending_large_blocks_of_data"
    language "C++"
    kind "ConsoleApp"
    files { "004_sending_large_blocks_of_data.cpp", "protocol2.h", "network2.h" }

project "005_packet_aggregation"
    language "C++"
    kind "ConsoleApp"
    files { "005_packet_aggregation.cpp", "protocol2.h", "network2.h" }

project "006_reliable_ordered_messages"
    language "C++"
    kind "ConsoleApp"
    files { "006_reliable_ordered_messages.cpp", "protocol2.h", "network2.h" }

project "007_messages_and_blocks"
    language "C++"
    kind "ConsoleApp"
    files { "007_messages_and_blocks.cpp", "protocol2.h", "network2.h" }

project "008_packet_encryption"
    language "C++"
    kind "ConsoleApp"
    files { "008_packet_encryption.cpp", "protocol2.h", "network2.h" }
    configuration "Debug"
        links { debug_libs }
    configuration "Release"
        links { release_libs }

project "009_client_server"
    language "C++"
    kind "ConsoleApp"
    files { "009_client_server.cpp", "protocol2.h", "network2.h" }
    configuration "Debug"
        links { debug_libs }
    configuration "Release"
        links { release_libs }

project "010_connect_tokens"
    language "C++"
    kind "ConsoleApp"
    files { "010_connect_tokens.cpp", "protocol2.h", "network2.h" }
    configuration "Debug"
        links { debug_libs }
    configuration "Release"
        links { release_libs }

if not os.is "windows" then

    newaction
    {
        trigger     = "zip",
        description = "Create a zip of this project",
        execute = function ()
            _ACTION = "clean"
            premake.action.call( "clean" )
            files_to_zip = "*.cpp *.h *.lib premake5.lua sodium vectorial"
            os.execute( "rm -rf *.7z" );
            os.execute( "7z a -y -mx=9 -p\"wintermute\" \"Building a Game Network Protocol.7z\" " .. files_to_zip )
            os.execute( "echo" );
        end
    }

    newaction
    {
        trigger     = "test",
        description = "Build and run all unit tests",
        execute = function ()
            if os.execute "make -j32 test" == 0 then
                os.execute "./bin/test"
            end
        end
    }

    newaction
    {
        trigger     = "001",
        description = "Build example source for reading and writing packets",
        execute = function ()
            if os.execute "make -j32 001_reading_and_writing_packets" == 0 then
                os.execute "./bin/001_reading_and_writing_packets"
            end
        end
    }

    newaction
    {
        trigger     = "002",
        description = "Build example source for serialization strategies",
        execute = function ()
            if os.execute "make -j32 002_serialization_strategies" == 0 then
                os.execute "./bin/002_serialization_strategies"
            end
        end
    }

    newaction
    {
        trigger     = "003",
        description = "Build example source for packet fragmentation and reassembly",
        execute = function ()
            if os.execute "make -j32 003_packet_fragmentation_and_reassembly" == 0 then
                os.execute "./bin/003_packet_fragmentation_and_reassembly"
            end
        end
    }

    newaction
    {
        trigger     = "004",
        description = "Build example source for sending large blocks of data",
        execute = function ()
            if os.execute "make -j32 004_sending_large_blocks_of_data" == 0 then
                os.execute "./bin/004_sending_large_blocks_of_data"
            end
        end
    }

    newaction
    {
        trigger     = "005",
        description = "Build example source for packet aggregation",
        execute = function ()
            if os.execute "make -j32 005_packet_aggregation" == 0 then
                os.execute "./bin/005_packet_aggregation"
            end
        end
    }

    newaction
    {
        trigger     = "006",
        description = "Build example source for reliable ordered messages",
        execute = function ()
            if os.execute "make -j32 006_reliable_ordered_messages" == 0 then
                os.execute "./bin/006_reliable_ordered_messages"
            end
        end
    }

    newaction
    {
        trigger     = "007",
        description = "Build example source for messages and blocks",
        execute = function ()
            if os.execute "make -j32 007_messages_and_blocks" == 0 then
                os.execute "./bin/007_messages_and_blocks"
            end
        end
    }

    newaction
    {
        trigger     = "008",
        description = "Build example source for packet encryption",
        execute = function ()
            if os.execute "make -j32 008_packet_encryption" == 0 then
                os.execute "./bin/008_packet_encryption"
            end
        end
    }

    newaction
    {
        trigger     = "009",
        description = "Build example source for client/server",
        execute = function ()
            if os.execute "make -j32 009_client_server" == 0 then
                os.execute "./bin/009_client_server"
            end
        end
    }

    newaction
    {
        trigger     = "010",
        description = "Build example source for connect tokens",
        execute = function ()
            if os.execute "make -j32 010_connect_tokens" == 0 then
                os.execute "./bin/010_connect_tokens"
            end
        end
    }

else

    newaction
    {
        trigger     = "solution",
        description = "Build the solution and open it in Visual Studio",
        execute = function ()
            os.execute "premake5 vs2015"
            os.execute "start NetworkProtocol.sln"
        end
    }

end

newaction
{
    trigger     = "clean",

    description = "Clean all build files and output",

    execute = function ()

        files_to_delete = 
        {
            "Makefile",
            "*.make",
            "*.txt",
            "*.7z",
            "*.zip",
            "*.tar.gz",
            "*.db",
            "*.opendb",
            "*.vcproj",
            "*.vcxproj",
            "*.vcxproj.user",
            "*.sln"
        }

        directories_to_delete = 
        {
            "obj",
            "ipch",
            "bin",
            ".vs",
            "Debug",
            "Release",
        }

        for i,v in ipairs( directories_to_delete ) do
          os.rmdir( v )
        end

        if not os.is "windows" then
            os.execute "find . -name .DS_Store -delete"
            for i,v in ipairs( files_to_delete ) do
              os.execute( "rm -f " .. v )
            end
        else
            for i,v in ipairs( files_to_delete ) do
              os.execute( "del /F /Q  " .. v )
            end
        end

    end
}
