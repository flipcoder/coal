solution("coal")
    targetdir("bin")
    
    configurations {"Debug", "Release"}

        defines { }
        
        -- Debug Config
        configuration "Debug"
            defines { "DEBUG" }
            flags { "Symbols" }
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
            links {
                "portaudio_x86",
                "sndfile",
            }

            includedirs {
                "c:/local/boost_1_61_0",
                "c:/msvc/include",
            }
            configuration { "windows", "Debug" }
                libdirs {
                    "c:/msvc/lib32/debug"
                }
            configuration { "windows" }
            libdirs {
                "c:/msvc/lib32",
                "c:/local/boost_1_61_0/lib32-msvc-14.0",
            }
            -- buildoptions {
                -- "/MP",
                -- "/Gm-",
            -- }
            
            configuration { "windows", "Debug" }
                links {
                    "libboost_filesystem-vc140-mt-gd-1_61"
                }
            configuration {}
            configuration { "windows", "Release" }
                links {
                    "libboost_filesystem-vc140-mt-1_61"
                }

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

