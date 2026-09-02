--
--  projects/house-style.lua -- House build output for the generated GNU makefiles
--
--  Premake prints "Linking ioto" and a bare file name per compile. This project prints
--  "      [Link] ioto" and "        [CC] ioto.c", uses [AR] for static libraries, and drops
--  premake's directory-creation lines and per-project build banner.
--
--  Applied here as part of generation rather than afterwards by ~/bin/fixmake, so the generated
--  makefiles stay a pure function of premake5.lua and a fresh clone can reproduce them without any
--  tool from a home directory. A hand edit -- or a post-pass that someone forgets to run -- cannot
--  then vanish at the next regeneration. "make verify-projects" proves it and fails if they diverge.
--
--  dofile this from a premake5.lua after the workspace is declared. It hooks the action's onEnd, so
--  it runs once the files are on disk, and it is a no-op for the vs2022 and xcode4 actions.
--
--  Tags are padded to align content at column 13:
--      [CC]     8 spaces + 4 chars
--      [AR]     8 spaces + 4 chars
--      [Run]    7 spaces + 5 chars
--      [Link]   6 spaces + 6 chars
--      [Clean]  5 spaces + 7 chars
--

local houseStyle = {
    --  Compile: premake emits the bare file name, quoted or not depending on the rule
    { '\n\t@echo "%$%(notdir %$<%)"\n', '\n\t@echo "        [CC] $(notdir $<)"\n' },
    { '\n\t@echo %$%(notdir %$<%)\n',   '\n\t@echo "        [CC] $(notdir $<)"\n' },

    --  Name the action and the target the way every other Embedthis build does
    { '\n\t@echo Cleaning ([%w%-_%.]+)\n',       '\n\t@echo "     [Clean] %1"\n' },
    { '\n\t@echo Running postbuild commands\n',  '\n\t@echo "       [Run] postbuild"\n' },

    --  Noise: creating a directory, and the per-project build banner
    { '\n\t@echo Creating %$%(TARGETDIR%)\n', '\n' },
    { '\n\t@echo Creating %$%(OBJDIR%)\n',    '\n' },
    { '\n\t@echo "==== Building [^\n]*\n',    '\n' },
}

--
--  A static library is archived, not linked. Premake says "Linking" for both, so the tag follows
--  the link command the file itself declares.
--
local function linkRule(text)
    if text:find('LINKCMD = %$%(AR%)') then
        return { '\n\t@echo Linking ([%w%-_%.]+)\n', '\n\t@echo "        [AR] %1"\n' }
    end
    return { '\n\t@echo Linking ([%w%-_%.]+)\n', '\n\t@echo "      [Link] %1"\n' }
end

local function applyHouseStyle(dir)
    local files = os.matchfiles(dir .. "/*.make")
    table.insert(files, dir .. "/Makefile")

    local count = 0
    for _, file in ipairs(files) do
        local text = io.readfile(file)
        if text then
            local original = text
            for _, rule in ipairs(houseStyle) do
                text = text:gsub(rule[1], rule[2])
            end
            local link = linkRule(text)
            text = text:gsub(link[1], link[2])
            if text ~= original then
                io.writefile(file, text)
                count = count + 1
            end
        end
    end
    return count
end

--
--  Hook the action rather than the emitters. The message-emitting functions inside the gmake module
--  are internal and have been renamed between premake releases; a text pass over the finished output
--  is the same transformation fixmake was applying, and it survives a generator upgrade.
--
if _ACTION == "gmake" or _ACTION == "gmake2" then
    local action = premake.action.get(_ACTION)
    if action then
        local prior = action.onEnd
        action.onEnd = function(...)
            if prior then
                prior(...)
            end
            local n = applyHouseStyle("gmake2")
            print(string.format("Applied house build output to %d file(s)...", n))
        end
    end
end
