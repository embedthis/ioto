--
--  apps/blank/projects/premake5.lua -- Blank App Build
--
--  Builds ioto-blank linking against the pre-built libioto.a.
--

local ROOT = "../../.."
isVS = (_ACTION == "vs2022")
isXcode = (_ACTION == "xcode4")

newoption {
    trigger     = "tls",
    value       = "PROVIDER",
    description = "TLS provider: openssl (default) or mbedtls",
    default     = "openssl",
    allowed     = {
        { "openssl", "OpenSSL" },
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

workspace "ioto-blank"
    configurations { "debug", "release" }
    language       "C"
    warnings       "Extra"

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

    includedirs { ROOT .. "/include" }
    libdirs     { ROOT .. "/build/bin" }

    dofile(ROOT .. "/projects/ioto-config.lua")

project "ioto-blank"
    kind       "ConsoleApp"
    targetdir  (ROOT .. "/build/bin")
    objdir     "../build/obj/%{cfg.buildcfg}_%{cfg.platform}"

    files { "../src/blank.c" }
    links { "ioto" }

    -- TLS and system libraries (projects/ioto-config.lua)
    iotoLinks()
