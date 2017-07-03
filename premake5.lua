workspace("coal")
    targetdir("bin")
    
    configurations {"Debug", "Release"}

        defines { "DO_NOT_USE_WMAIN", "NOMINMAX" }
        
        -- Debug Config
        configuration "Debug"
            defines { "DEBUG" }
            symbols "On"
            linkoptions { }

            configuration "gmake"
                links {
                    "z",
                    "bfd",
                    "iberty"
                }
        
        -- Release Config
        configuration "Release"
            defines { "NDEBUG" }
            flags { "OptimizeSpeed" }
            targetname("test_dist")

        -- gmake Config
        configuration "gmake"
            buildoptions { "-std=c++11" }
            -- buildoptions { "-std=c++11", "-pedantic", "-Wall", "-Wextra", '-v', '-fsyntax-only'}
            links {
                "pthread",
                "portaudio",
                "sndfile",
            }
            includedirs {
                "/usr/local/include/",
            }

            libdirs {
                "/usr/local/lib",
                "/usr/local/lib64/",
            }
            
            buildoptions {
                "`python2-config --includes`",
                "`pkg-config --cflags cairomm-1.0 pangomm-1.4`"
            }

            linkoptions {
                "`python2-config --libs`",
                "`pkg-config --libs cairomm-1.0 pangomm-1.4`"
            }
        -- OS X Config
        configuration "macosx"
            buildoptions { "-U__STRICT_ANSI__", "-stdlib=libc++" }
            linkoptions { "-stdlib=libc++" }

        configuration "windows"
            toolset "v141"
            flags { "MultiProcessorCompile" }

            links {
                "portaudio_x86",
                "libsndfile-1",
            }

            includedirs {
                "c:/local/boost_1_64_0",
                "c:/msvc/include",
                "C:/Program Files (x86)/Mega-Nerd/libsndfile/include",
            }
            configuration { "windows", "Debug" }
                libdirs {
                    "c:/msvc/lib32/debug"
                }
            configuration { "windows" }
            libdirs {
                "c:/msvc/lib32",
                "c:/local/boost_1_64_0/lib32-msvc-14.1",
                "C:/Program Files (x86)/Mega-Nerd/libsndfile/lib"
            }
            -- buildoptions {
                -- "/MP",
                -- "/Gm-",
            -- }
            
            --configuration { "windows", "Debug" }
            --    links {
            --    }
            --configuration {}
            --configuration { "windows", "Release" }
            --    links {
                --}

    project "test"
        kind "WindowedApp"
        language "C++"

        -- Project Files
        files {
            "coal/**.cpp",
            "tests/**.cpp",
        }

        -- Exluding Files
        excludes {
        }
        
        includedirs {
            ".",
            "/usr/local/include/",
        }

