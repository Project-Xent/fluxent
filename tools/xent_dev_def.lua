import("core.tool.toolchain")
import("lib.detect.find_tool")

local exported = { fluxent = true, xtk = true, xent_core = true }

local function collect(dumpbin, lib, out, seen)
    local text = os.iorunv(dumpbin, { "/symbols", "/nologo", lib })
    for _, line in ipairs(text:split("\n", { plain = true })) do
        if line:find("External", 1, true) and not line:find("UNDEF", 1, true) then
            local name = line:match("External%s+|%s+(%S+)")
            if name and not seen[name] and not name:startswith("__") and not name:startswith("?") then
                seen[name] = true
                table.insert(out, line:find("()", 1, true) and name or (name .. " DATA"))
            end
        end
    end
end

function main(target, deffile)
    local msvc = toolchain.load("msvc", { plat = target:plat(), arch = target:arch() })
    local dumpbin = assert(find_tool("dumpbin", { envs = msvc:runenvs() }), "dumpbin not found")
    local out, seen = { "EXPORTS" }, {}
    for _, obj in ipairs(target:objectfiles()) do
        collect(dumpbin.program, obj, out, seen)
    end
    for _, dep in ipairs(target:orderdeps()) do
        if exported[dep:name()] then
            collect(dumpbin.program, dep:targetfile(), out, seen)
        end
    end
    io.writefile(deffile, table.concat(out, "\n") .. "\n")
end
