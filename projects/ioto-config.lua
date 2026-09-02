--
--  ioto-config.lua -- Shared Ioto build configuration
--
--  Included by premake5.lua files via dofile(). Provides common workspace
--  settings, defines, platform flags, and TLS configuration.
--
--  Variables expected to be set before including this file:
--    tlsProvider   -- "openssl" or "mbedtls"
--    mbedtlsPath   -- Path to MbedTLS installation
--    isVS          -- true when generating for vs2022
--
--  projects/openssl-paths.lua must be dofile()d first: it supplies platformList and
--  opensslPathFor(), which resolve the OpenSSL prefix per target platform rather than from the
--  generating host.
--

if tlsProvider == "openssl" then
    defines { "ME_COM_OPENSSL=1", "ME_COM_MBEDTLS=0", "ME_COM_SSL=1" }
    for _, platform in ipairs(platformList) do
        filter("platforms:" .. platform)
            includedirs { opensslPathFor(platform) .. "/include" }
    end
    filter {}
else
    defines { "ME_COM_OPENSSL=0", "ME_COM_MBEDTLS=1", "ME_COM_SSL=1" }
    includedirs { mbedtlsPath .. "/include" }
end

filter "configurations:debug"
    symbols "On"
    optimize "Off"
    defines { "ME_DEBUG=1" }

filter "configurations:release"
    symbols "Off"
    optimize "Speed"

filter {}

-- Platform flags (guarded by action to avoid toolset conflicts)
if isVS then
    filter "platforms:windows"
        system  "windows"
        toolset "msc"
        links   { "ws2_32", "advapi32", "user32", "kernel32", "oldnames", "shell32" }
        disablewarnings {
            "4100",     -- unreferenced formal parameter
            "4127",     -- conditional expression is constant
            "4133",     -- incompatible types (char*/LPCWSTR)
            "4152",     -- function/data pointer conversion
            "4244",     -- conversion, possible loss of data
            "4389",     -- signed/unsigned mismatch
            "4456",     -- declaration hides previous local declaration
            "4459",     -- declaration hides global declaration
        }
    filter {}
else
    filter "platforms:macosx"
        system  "macosx"
        toolset "clang"
        buildoptions {
            "-Wno-unused-parameter", "-Wno-unused-result",
            "-Wshorten-64-to-32", "-Wall",
            "-Wno-unknown-warning-option", "-fstack-protector",
            "--param=ssp-buffer-size=4",
            "-Wformat", "-Wformat-security",
            "-Wsign-compare", "-Wsign-conversion",
        }
        linkoptions {
            "-Wl,-no_warn_duplicate_libraries",
            "-Wl,-rpath,@executable_path/",
            "-Wl,-rpath,@loader_path/",
        }

    filter "platforms:linux"
        system  "linux"
        toolset "gcc"
        buildoptions {
            "-Wno-unused-parameter", "-Wno-unused-result",
            "-Wall", "-fstack-protector",
            "--param=ssp-buffer-size=4",
            "-Wformat", "-Wformat-security",
            "-Wsign-compare", "-Wsign-conversion",
            "-pie", "-fPIE",
        }
        linkoptions {
            "-Wl,-z,relro,-z,now", "-Wl,--as-needed",
            "-Wl,--no-copy-dt-needed-entries",
            "-Wl,--no-warn-execstack",
        }

    filter "platforms:freebsd"
        system  "bsd"
        toolset "gcc"
        buildoptions {
            "-Wno-unused-parameter", "-Wno-unused-result",
            "-Wall", "-fstack-protector",
            "--param=ssp-buffer-size=4",
            "-Wformat", "-Wformat-security",
            "-Wsign-compare", "-Wsign-conversion",
        }

    filter {}
end
