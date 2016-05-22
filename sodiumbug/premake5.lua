
solution "Building a Game Network Protocol"
    platforms { "x64" }
    includedirs { ".", "vectorial" }
    if not os.is "windows" then
        targetdir "bin/"
    end
    configurations { "Debug", "Release" }
    flags { "ExtraWarnings", "FloatFast" }
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
    links { "yojimbo", "sodium" }

project "yojimbo"
    language "C++"
    kind "StaticLib"
    files { "yojimbo.h", "yojimbo.cpp", "yojimbo_*.h", "yojimbo_*.cpp" }
    links { "sodium" }

project "007_packet_encryption"
    language "C++"
    kind "ConsoleApp"
    files { "007_packet_encryption.cpp", "protocol2.h", "network2.h" }
    links { "yojimbo", "sodium" }

if _ACTION == "clean" then
    os.rmdir "obj"
    if not os.is "windows" then
        os.execute "rm -rf bin"
        os.execute "rm -rf obj"
        os.execute "rm -f Makefile"
        os.execute "rm -f protocol2"
        os.execute "rm -f network2"
        os.execute "rm -f *.zip"
        os.execute "rm -f *.make"
        os.execute "rm -f test"
        os.execute "rm -f 007_packet_encryption"
        os.execute "find . -name .DS_Store -delete"
    else
        os.rmdir "ipch"
		os.rmdir "bin"
		os.rmdir ".vs"
        os.rmdir "Debug"
        os.rmdir "Release"
        os.execute "del /F /Q *.zip"
        os.execute "del /F /Q *.db"
        os.execute "del /F /Q *.opendb"
        os.execute "del /F /Q *.vcproj"
        os.execute "del /F /Q *.vcxproj"
        os.execute "del /F /Q *.sln"
    end
end

if not os.is "windows" then

    newaction
    {
        trigger     = "zip",
        description = "Zip up archive of this project",
        valid_kinds = premake.action.get("gmake").valid_kinds,
        valid_languages = premake.action.get("gmake").valid_languages,
        valid_tools = premake.action.get("gmake").valid_tools,
     
        execute = function ()
            _ACTION = "clean"
            premake.action.call( "clean" )
            os.execute "zip -9r \"Building a Game Network Protocol.zip\" *.cpp *.h vectorial premake5.lua"
        end
    }

    newaction
    {
        trigger     = "test",
        description = "Build and run all unit tests",
        valid_kinds = premake.action.get("gmake").valid_kinds,
        valid_languages = premake.action.get("gmake").valid_languages,
        valid_tools = premake.action.get("gmake").valid_tools,
     
        execute = function ()
            if os.execute "make -j4 test" == 0 then
                os.execute "./bin/test"
            end
        end
    }

    newaction
    {
        trigger     = "yojimbo",
        description = "Build yojimbo client/server protocol library",
        valid_kinds = premake.action.get("gmake").valid_kinds,
        valid_languages = premake.action.get("gmake").valid_languages,
        valid_tools = premake.action.get("gmake").valid_tools,
     
        execute = function ()
            os.execute "make -j4 yojimbo"
        end
    }

    newaction
    {
        trigger     = "007",
        description = "Build example source for packet encryption",
        valid_kinds = premake.action.get("gmake").valid_kinds,
        valid_languages = premake.action.get("gmake").valid_languages,
        valid_tools = premake.action.get("gmake").valid_tools,
     
        execute = function ()
            if os.execute "make -j4 007_packet_encryption" == 0 then
                os.execute "./bin/007_packet_encryption"
            end
        end
    }

end
