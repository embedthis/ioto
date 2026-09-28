--
--  projects/premake5.lua -- Ioto Library and Tools Build
--
--  Builds libioto.a and utility binaries (db, json, url, web, password).
--  Does NOT build apps -- each app has its own premake5.lua.
--
--  Cross-generation: uses explicit `platforms` so all platform variants
--  are generated in a single pass from any host machine.
--
--  Usage (run from the projects/ directory):
--      premake5 gmake                     # Generate GNU Makefiles for all platforms
--      premake5 vs2022                    # Generate VS2022 projects for Windows
--      premake5 xcode4                    # Generate Xcode projects for macOS
--      make config=debug_macosx           # Build for macOS
--      make config=release_linux          # Build for Linux
--

local ROOT = ".."

-- Read version from package.json (single source of truth)
local pakContent = io.readfile(ROOT .. "/package.json")
local VERSION = pakContent:match('"version"%s*:%s*"([^"]+)"') or "0.0.0"
isVS = (_ACTION == "vs2022")
isXcode = (_ACTION == "xcode4")

---------------------------------------------------------------------
--  Options
---------------------------------------------------------------------

newoption {
    trigger     = "tls",
    value       = "PROVIDER",
    description = "TLS provider: openssl (default) or mbedtls",
    default     = "openssl",
    allowed     = {
        { "openssl", "OpenSSL (default)" },
        { "mbedtls", "MbedTLS" },
    }
}

newoption {
    trigger     = "openssl-path",
    value       = "PATH",
    description = "Path to OpenSSL for every platform, overriding the per-platform defaults " ..
                  "(/opt/homebrew on macOS, /usr on Linux/FreeBSD, C:/Program Files/OpenSSL on Windows)",
}

newoption {
    trigger     = "mbedtls-path",
    value       = "PATH",
    description = "Path to MbedTLS installation",
}

dofile(ROOT .. "/projects/openssl-paths.lua")
dofile(ROOT .. "/projects/house-style.lua")

local mbedtlsPath = _OPTIONS["mbedtls-path"] or "/usr"
local tlsProvider = _OPTIONS["tls"] or "openssl"

---------------------------------------------------------------------
--  Workspace
---------------------------------------------------------------------

workspace "ioto"
    configurations { "debug", "release" }
    language       "C"
    staticruntime  "On"
    warnings       "Extra"
    objdir         (ROOT .. "/build/obj/%{prj.name}-%{cfg.platform}-%{cfg.buildcfg}")
    targetdir      (ROOT .. "/build/bin")

    if isVS then
        platforms { "windows" }
        location  "vs2022"
        cdialect  "C11"
        architecture "x86_64"
        characterset "MBCS"
    elseif isXcode then
        platforms { "macosx" }
        location  "xcode"
        cdialect  "gnu11"
    else
        platforms { "macosx", "linux", "freebsd" }
        location  "gmake2"
        cdialect  "gnu11"
    end

    -- Common include paths
    includedirs { ROOT .. "/include" }

    -- Dynamic defines (static defaults are in include/config.h)
    defines {
        'ME_VERSION="' .. VERSION .. '"',
    }

    -- TLS provider defines (override config.h defaults based on --tls option)
    if tlsProvider == "openssl" then
        defines {
            "ME_COM_OPENSSL=1",
            "ME_COM_MBEDTLS=0",
            'ME_COM_MBEDTLS_PATH="' .. mbedtlsPath .. '"',
        }
    else
        defines {
            "ME_COM_OPENSSL=0",
            "ME_COM_MBEDTLS=1",
            'ME_COM_MBEDTLS_PATH="' .. mbedtlsPath .. '"',
        }
        includedirs { mbedtlsPath .. "/include" }
    end

    --
    --  ME_COM_OPENSSL_PATH is emitted whichever provider is selected, and must follow the target
    --  platform rather than the generating host. See projects/openssl-paths.lua.
    --
    for _, platform in ipairs(platformList) do
        filter("platforms:" .. platform)
            defines { 'ME_COM_OPENSSL_PATH="' .. opensslPathFor(platform) .. '"' }
            if tlsProvider == "openssl" then
                includedirs { opensslPathFor(platform) .. "/include" }
            end
    end
    filter {}

    -----------------------------------------------------------------
    --  Debug / Release
    -----------------------------------------------------------------
    filter "configurations:debug"
        symbols  "On"
        optimize "Off"
        defines  { "ME_DEBUG=1" }

    filter "configurations:release"
        symbols  "Off"
        optimize "On"

    filter {}

    if not isVS then
        filter "configurations:debug"
            linkoptions { "-g" }
        filter "configurations:release"
            linkoptions { "-s" }
        filter {}
    end

    -----------------------------------------------------------------
    --  Platform-specific flags (guarded by action to avoid toolset conflicts)
    -----------------------------------------------------------------
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
                "-Wno-sign-conversion",
                "-Wno-unused-parameter",
                "-Wno-unused-result",
                "-Wshorten-64-to-32",
                "-Wall",
                "-Wno-unknown-warning-option",
                "-fstack-protector",
                "--param=ssp-buffer-size=4",
                "-Wformat", "-Wformat-security",
                "-Wsign-compare",
            }
            linkoptions {
                "-Wl,-no_warn_duplicate_libraries",
                "-Wl,-rpath,@executable_path/",
                "-Wl,-rpath,@loader_path/",
            }
            links { "dl", "pthread", "m" }

        filter "platforms:linux"
            system  "linux"
            toolset "gcc"
            buildoptions {
                "-Wno-unused-parameter", "-Wno-unused-result",
                "-Wall",
                "-fstack-protector",
                "--param=ssp-buffer-size=4",
                "-Wformat", "-Wformat-security",
                "-Wsign-compare",
                "-pie", "-fPIE",
            }
            linkoptions {
                "-Wl,-z,relro,-z,now",
                "-Wl,--as-needed",
                "-Wl,--no-copy-dt-needed-entries",
                "-Wl,--no-warn-execstack",
            }
            links { "rt", "dl", "pthread", "m" }

        filter "platforms:freebsd"
            system  "bsd"
            toolset "gcc"
            buildoptions {
                "-Wno-unused-parameter", "-Wno-unused-result",
                "-Wall",
                "-fstack-protector",
                "--param=ssp-buffer-size=4",
                "-Wformat", "-Wformat-security",
                "-Wsign-compare",
            }
            links { "dl", "pthread", "m" }

        filter {}
    end


---------------------------------------------------------------------
--  libioto (static library)
---------------------------------------------------------------------

project "ioto"
    kind       "StaticLib"
    targetname "ioto"

    -- Library module sources (concatenated single-file libs)
    files {
        ROOT .. "/lib/cryptLib.c",
        ROOT .. "/lib/dbLib.c",
        ROOT .. "/lib/jsonLib.c",
        ROOT .. "/lib/mqttLib.c",
        ROOT .. "/lib/openaiLib.c",
        ROOT .. "/lib/rLib.c",
        ROOT .. "/lib/uctxLib.c",
        ROOT .. "/lib/urlLib.c",
        ROOT .. "/lib/webLib.c",
        ROOT .. "/lib/websockLib.c",
    }

    -- Assembly fiber context switch (all gmake2 platforms)
    filter { "platforms:macosx or linux or freebsd" }
        files { ROOT .. "/lib/uctxAssembly.S" }
    filter {}

    -- Agent core
    files {
        ROOT .. "/src/agent.c",
        ROOT .. "/src/ai.c",
        ROOT .. "/src/config.c",
        ROOT .. "/src/cron.c",
        ROOT .. "/src/database.c",
        ROOT .. "/src/esp32.c",
        ROOT .. "/src/setup.c",
        ROOT .. "/src/webserver.c",
        ROOT .. "/src/cmds/ioto.c",
    }

    -- Cloud subsystem (optional, installed via cloud pak)
    if os.isfile(ROOT .. "/lib/cloudLib.c") then
        files {
            ROOT .. "/lib/cloudLib.c",
        }
    end


---------------------------------------------------------------------
--  Helper: create a utility exe linked against libioto
---------------------------------------------------------------------

local function ioto_tool(name, srcs)
    project(name)
        kind       "ConsoleApp"
        links      { "ioto" }
        dependson  { "ioto" }
        libdirs    { ROOT .. "/build/bin" }

        -- Source paths are relative to ROOT (project root)
        files(srcs)

        -- TLS libraries (per-platform: the library prefix must follow the target, not the host)
        for _, platform in ipairs(platformList) do
            filter("platforms:" .. platform)
                if tlsProvider ~= "openssl" then
                    libdirs { mbedtlsPath .. "/lib", mbedtlsPath .. "/library" }
                    links   { "mbedtls", "mbedcrypto", "mbedx509" }
                elseif platform == "windows" then
                    local p = opensslPathFor("windows")
                    libdirs {
                        p .. "/lib",
                        p .. "/lib/VC/x64/MTd",
                        p .. "/lib/VC/x64/MT",
                        p .. "/lib/VC/x64/MDd",
                        p .. "/lib/VC/x64/MD",
                    }
                    links { "libssl", "libcrypto" }
                else
                    libdirs { opensslPathFor(platform) .. "/lib" }
                    links   { "ssl", "crypto" }
                end
        end
        filter {}
end


---------------------------------------------------------------------
--  Utility tools
---------------------------------------------------------------------

ioto_tool("password", { ROOT .. "/src/cmds/password.c" })
ioto_tool("db",       { ROOT .. "/src/cmds/db.c" })
ioto_tool("json",     { ROOT .. "/src/cmds/json.c" })
ioto_tool("url",      { ROOT .. "/src/cmds/url.c" })
ioto_tool("web",      { ROOT .. "/src/cmds/web.c" })
ioto_tool("gen-config", { ROOT .. "/bin/gen-config.c" })

-- Add post-build step to run gen-config and produce build/config.sh
project "gen-config"
    postbuildcommands {
        '"%{cfg.buildtarget.abspath}" > "%{cfg.buildtarget.directory}/../config.sh"'
    }
