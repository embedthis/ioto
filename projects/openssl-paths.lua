--
--  openssl-paths.lua -- Per-platform OpenSSL location
--
--  dofile() this before ioto-config.lua and before any project block that needs an OpenSSL
--  include or library path.
--
--  OpenSSL sits in a different place on each target, and the generated project files are
--  committed and shipped. Resolving a single path from os.host() at generation time bakes the
--  generating machine's layout into every configuration, so a Linux build from the distribution
--  is handed the macOS Homebrew prefix and the /usr default for Linux never reaches a user.
--  Resolve per target platform instead, inside a filter, so each configuration carries its own.
--
--  Variables expected to be set before including this file:
--    isVS          -- true when generating for vs2022
--    isXcode       -- true when generating for xcode4
--
--  Provides:
--    opensslPaths     -- table keyed by platform name
--    platformList     -- ordered list of the platforms this generator emits
--    opensslPathFor() -- accessor, for use inside a per-platform filter
--
--  An explicit --openssl-path overrides every platform, for an installation that is not in the
--  usual place.
--

local opensslOverride = _OPTIONS["openssl-path"]

opensslPaths = {
    macosx  = opensslOverride or "/opt/homebrew",
    linux   = opensslOverride or "/usr",
    freebsd = opensslOverride or "/usr",
    windows = opensslOverride or "C:/Program Files/OpenSSL",
}

--
--  Iterate this ordered list, never pairs() over the table above: the generated files are
--  committed and diffed, so the emission order has to be stable from run to run. Keep it in step
--  with the platforms{} blocks in each workspace.
--
if isVS then
    platformList = { "windows" }
elseif isXcode then
    platformList = { "macosx" }
else
    platformList = { "macosx", "linux", "freebsd" }
end

function opensslPathFor(platform)
    return opensslPaths[platform]
end
