--
--  ioto-config.lua -- Shared Ioto build configuration
--
--  The one definition of the TLS provider defines, the debug and release configurations, the
--  platform flags and the hardening flags. projects/premake5.lua (the library and tools) and every
--  apps/*/projects/premake5.lua dofile() it inside their workspace, so the library, the tools and
--  the apps are compiled and linked alike. Two copies of these blocks drifted apart before; do not
--  add a configuration or platform flag anywhere else. test/guard/hardening.tst.sh enforces it.
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

---------------------------------------------------------------------
--  TLS provider (overrides the include/config.h defaults)
---------------------------------------------------------------------

defines {
    "ME_COM_SSL=1",
    "ME_COM_OPENSSL=" .. ((tlsProvider == "openssl") and "1" or "0"),
    "ME_COM_MBEDTLS=" .. ((tlsProvider == "mbedtls") and "1" or "0"),
    'ME_COM_MBEDTLS_PATH="' .. mbedtlsPath .. '"',
}
if tlsProvider ~= "openssl" then
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

---------------------------------------------------------------------
--  Debug / Release
---------------------------------------------------------------------

--  The dynamic CRT (/MD), so Windows Update services CRT security fixes without a rebuild
staticruntime "Off"

filter "configurations:debug"
    symbols  "On"
    optimize "Off"
    defines  { "ME_DEBUG=1" }

--  -O2: the level the warnings guard and _FORTIFY_SOURCE are validated at
filter "configurations:release"
    symbols  "Off"
    optimize "On"
    --  Defined, not merely absent: the runtime tests it as a value, not only with #if
    defines  { "ME_DEBUG=0" }

filter {}

if not isVS then
    filter "configurations:debug"
        linkoptions { "-g" }
    --
    --  No explicit -s for release: symbols "Off" already strips where the toolset supports it, so
    --  it only ever duplicated the flag on Linux and supplied the one macOS rejects as obsolete -
    --  which the release build treats as an error. macOS strips local symbols with -Wl,-x below.
    --
    filter {}
end

---------------------------------------------------------------------
--  Platform flags (guarded by action to avoid toolset conflicts)
---------------------------------------------------------------------

if isVS then
    filter "platforms:windows"
        system  "windows"
        toolset "msc"
        disablewarnings {
            "4127",     -- conditional expression is constant
            "4152",     -- function/data pointer conversion
            "4244",     -- conversion, possible loss of data
            "4456",     -- declaration hides previous local declaration
            "4459",     -- declaration hides global declaration
        }
    filter {}
else
    filter "platforms:macosx"
        system  "macosx"
        toolset "clang"
        buildoptions {
            "-Wsign-conversion",
            "-Wshorten-64-to-32",
            "-Wall",
            "-Wno-unknown-warning-option",
            "-fstack-protector-strong",
            "-Wformat", "-Wformat-security",
            "-Wsign-compare",
        }
        linkoptions {
            "-Wl,-no_warn_duplicate_libraries",
            "-Wl,-rpath,@executable_path/",
            "-Wl,-rpath,@loader_path/",
        }

    --
    --  PIE, full RELRO and a non-executable stack are set explicitly rather than left to the
    --  compiler's built-in defaults, which differ by distribution. -pie is a link option: in
    --  buildoptions it reaches only the compile line. A static library ignores linkoptions, so the
    --  executables carry them.
    --
    filter "platforms:linux"
        system  "linux"
        toolset "gcc"
        buildoptions {
            "-Wsign-conversion",
            "-Wall",
            "-fstack-protector-strong",
            "-Wformat", "-Wformat-security",
            "-Wsign-compare",
            "-fPIE",
        }
        linkoptions {
            "-pie",
            "-Wl,-z,relro,-z,now",
            "-Wl,-z,noexecstack",
            "-Wl,--as-needed",
            "-Wl,--no-copy-dt-needed-entries",
        }

    filter "platforms:freebsd"
        system  "bsd"
        toolset "gcc"
        buildoptions {
            "-Wsign-conversion",
            "-Wall",
            "-fstack-protector-strong",
            "-Wformat", "-Wformat-security",
            "-Wsign-compare",
            "-fPIE",
        }
        linkoptions {
            "-pie",
            "-Wl,-z,relro,-z,now",
            "-Wl,-z,noexecstack",
        }

    --
    --  FORTIFY needs optimization, so release only. Undefine first: distribution compilers
    --  predefine it (Ubuntu 24.04 gcc at level 3) and a bare -D would be a redefinition warning.
    --  Level 3 falls back to level 2 where the compiler lacks __builtin_dynamic_object_size.
    --
    filter { "configurations:release", "platforms:linux or freebsd" }
        buildoptions { "-U_FORTIFY_SOURCE", "-D_FORTIFY_SOURCE=3" }

    --  Strip local symbols: symbols "Off" emits no strip flag for the clang toolset
    filter { "configurations:release", "platforms:macosx" }
        linkoptions { "-Wl,-x" }

    filter {}
end

---------------------------------------------------------------------
--  Libraries an executable links after libioto
---------------------------------------------------------------------

--
--  Call from an executable project after its links { "ioto" }. Premake emits workspace links ahead
--  of project links, and a system library listed before the static libioto is dropped by
--  --as-needed, so these cannot be workspace settings. The OpenSSL prefix differs by platform, so
--  each platform has its own filter. See projects/openssl-paths.lua.
--
--  The Windows system libraries belong to the executables and never to the library: premake passes
--  a StaticLib's links to Lib.exe, which merges the import libraries into ioto.lib; each then
--  redefines __NULL_IMPORT_DESCRIPTOR and the librarian emits LNK4006 for every one after the first.
--
local systemLibs = {
    macosx  = { "dl", "pthread", "m" },
    freebsd = { "dl", "pthread", "m" },
    linux   = { "rt", "dl", "pthread", "m" },
    windows = { "ws2_32", "advapi32", "user32", "kernel32", "oldnames", "shell32" },
}

function iotoLinks()
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
            links(systemLibs[platform])
    end
    filter {}
end
