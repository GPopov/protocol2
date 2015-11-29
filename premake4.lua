solution "protocol2"
    platforms { "x64" }
    configurations { "Release", "Debug" }
    flags { "Symbols", "ExtraWarnings", "EnableSSE2", "FloatFast" , "NoRTTI", "NoExceptions" }
    configuration "Release"
        flags { "OptimizeSpeed" }
        defines { "NDEBUG" }

project "test"
    language "C++"
    kind "ConsoleApp"
    files { "test.cpp" }

project "001_reading_and_writing_packets"
    language "C++"
    kind "ConsoleApp"
    files { "001_reading_and_writing_packets.cpp" }

if _ACTION == "clean" then
    if not os.is "windows" then
        os.execute "rm -f protocol2.zip"
    else
        os.rmdir "ipch"
        os.execute "del /F /Q protocol2.zip"
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
            os.execute "zip -9r protocol2.zip *.cpp *.h premake4.lua"
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
                os.execute "./test"
            end
        end
    }

end
