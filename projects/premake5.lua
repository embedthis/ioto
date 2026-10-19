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

mbedtlsPath = _OPTIONS["mbedtls-path"] or "/usr"
tlsProvider = _OPTIONS["tls"] or "openssl"

---------------------------------------------------------------------
--  Workspace
---------------------------------------------------------------------

workspace "ioto"
    configurations { "debug", "release" }
    language       "C"
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

    --  TLS defines, configurations, platform and hardening flags: shared with every app
    dofile(ROOT .. "/projects/ioto-config.lua")

    --  ME_WEB_TEST_ACTIONS compiles webLib's /test table so the suite can link webTestInit. Only the
    --  library compiles webLib.c, so only it needs the define. Compiling it in is not the same as
    --  exposing it: src/cmds/web.c registers those routes only for an explicit --test-actions, which
    --  only test/web/setup.sh passes.
    filter "configurations:debug"
        defines { "ME_WEB_TEST_ACTIONS=1" }
    filter {}

    --[[
        gmake2/tls-<os>.rsp: the compiler response file a unit test needs to build against the TLS provider
        these makefiles select. test/testme.json5 passes it with "@", so the tests see the same ME_COM_*
        selection and link the same TLS libraries as libioto. It precedes the test source on the link line,
        so on Linux and FreeBSD the TLS libraries must be shared and are kept with --no-as-needed.
    ]]
    if _ACTION == "gmake" or _ACTION == "gmake2" then
        local tlsLibs = (tlsProvider == "openssl") and "-lssl -lcrypto" or "-lmbedtls -lmbedx509 -lmbedcrypto"
        for _, platform in ipairs({ "macosx", "linux", "freebsd" }) do
            local tlsPath = (tlsProvider == "openssl") and opensslPathFor(platform) or mbedtlsPath
            local keep = (platform == "macosx") and " " or " -Wl,--no-as-needed "
            local tlsFlags = table.concat({
                "-DME_COM_SSL=1",
                "-DME_COM_OPENSSL=" .. ((tlsProvider == "openssl") and "1" or "0"),
                "-DME_COM_MBEDTLS=" .. ((tlsProvider == "mbedtls") and "1" or "0"),
                "-I" .. tlsPath .. "/include",
                "-L" .. tlsPath .. "/lib",
                "-Wl,-rpath," .. tlsPath .. "/lib",
            }, " ")
            io.writefile(_SCRIPT_DIR .. "/gmake2/tls-" .. platform .. ".rsp", tlsFlags .. keep .. tlsLibs .. "\n")
        end
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

        -- TLS and system libraries (projects/ioto-config.lua)
        iotoLinks()
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
