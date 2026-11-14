import("core.base.option")
import("lib.detect.find_program")

local libs = {
    "fluxent", "xtk", "xent_core", "cwinrt-rt",
    "cwinrt-bindings-foundation",
    "cwinrt-bindings-ui-composition",
    "cwinrt-bindings-ui-composition-interactions",
    "cwinrt-bindings-ui-viewmanagement",
    "cwinrt-bindings-globalization",
    "cwinrt-bindings-globalization-datetimeformatting",
    "cwinrt-bindings-globalization-numberformatting",
    "cwinrt-bindings-ui-input",
}

local syslinks = {
    "user32", "gdi32", "dcomp", "d2d1", "d3d11", "dxgi", "dwrite", "dwmapi", "ole32", "oleaut32", "uuid",
    "uxtheme", "imm32", "advapi32", "shell32", "coremessaging", "uiautomationcore", "runtimeobject",
    "windowscodecs",
}

local import_libs = {"coremessaging", "uiautomationcore", "runtimeobject"}

local triples = {x86_64 = "x86_64-w64-mingw32", arm64 = "aarch64-w64-mingw32"}

local function mingw_root()
    local clang = assert(find_program("x86_64-w64-mingw32-clang"), "llvm-mingw not found on PATH")
    return path.directory(path.directory(clang))
end

local function build(root, arch, mingw, version)
    local work = path.join(root, "build", "dist", arch)
    local envs = {XMAKE_CONFIGDIR = path.join(work, "config"), XENT_VERSION = version}
    os.execv("xmake", {"f", "-P", root, "-p", "mingw", "-a", arch, "-m", "release", "--mingw=" .. mingw,
        "-o", path.join(work, "build"), "-y"}, {envs = envs, curdir = root})
    os.execv("xmake", {"build", "-P", root, "fluxent"}, {envs = envs, curdir = root})
    return path.join(work, "build", "mingw", arch, "release")
end

local function merge(libdir, out, ar)
    local script = {"create " .. out}
    for _, lib in ipairs(libs) do
        table.insert(script, "addlib " .. path.join(libdir, "lib" .. lib .. ".a"))
    end
    table.insert(script, "save")
    table.insert(script, "end")
    local mri = out .. ".mri"
    io.writefile(mri, table.concat(script, "\n") .. "\n")
    os.execv(ar, {"-M"}, {stdin = mri})
    os.rm(mri)
end

local function pkgconfig(version)
    local ls = {}
    for _, lib in ipairs(syslinks) do table.insert(ls, "-l" .. lib) end
    return table.concat({
        "prefix=${pcfiledir}/../..",
        "includedir=${prefix}/include",
        "libdir=${prefix}/lib",
        "",
        "Name: xent",
        "Description: Xent native UI toolkit (xent-core, xent-kit, fluxent)",
        "Version: " .. version,
        "Cflags: -I${includedir}",
        "Libs: -L${libdir} -lxent",
        "Libs.private: " .. table.concat(ls, " "),
        "",
    }, "\n")
end

local function stage(root, arch, libdir, mingw, version)
    local name = string.format("xent-%s-%s-windows-gnu", version, arch)
    local out = path.join(root, "build", "dist", name)
    os.tryrm(out)
    local xent = path.directory(root)
    os.cp(path.join(xent, "xent-core", "include", "xent"), path.join(out, "include", "xent"))
    os.cp(path.join(xent, "xent-kit", "include", "xtk"), path.join(out, "include", "xtk"))
    os.cp(path.join(root, "include", "fluxent"), path.join(out, "include", "fluxent"))
    os.mkdir(path.join(out, "lib", "pkgconfig"))
    merge(libdir, path.join(out, "lib", "libxent.a"), path.join(mingw, "bin", "llvm-ar.exe"))
    for _, lib in ipairs(import_libs) do
        os.cp(path.join(mingw, triples[arch], "lib", "lib" .. lib .. ".a"), path.join(out, "lib"))
    end
    io.writefile(path.join(out, "lib", "pkgconfig", "xent.pc"), pkgconfig(version))
    os.cp(path.join(root, "LICENSE"), out)
    local zip = out .. ".zip"
    os.tryrm(zip)
    os.execv("tar", {"-a", "-c", "-f", zip, "-C", path.directory(out), name})
    return zip
end

function main(...)
    local args = {...}
    local root = os.projectdir()
    local version = os.getenv("XENT_VERSION") or "0.3.2"
    local mingw = mingw_root()
    local archs = #args > 0 and args or {"x86_64", "arm64"}
    for _, arch in ipairs(archs) do
        assert(triples[arch], "unknown arch: " .. arch)
        local zip = stage(root, arch, build(root, arch, mingw, version), mingw, version)
        print(string.format("%s  %d bytes", zip, os.filesize(zip)))
    end
end
