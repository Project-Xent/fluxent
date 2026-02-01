-- FluXent: Windows Rendering Backend for Xent UI Framework
set_project("fluxent")
add_rules("mode.debug", "mode.release")
set_languages("c++20")

-- Use xent-core from xmake-repo packages
add_requires("xent-core")
add_requires("tl_expected")

-- Enable modern Windows API (Windows 10+)
add_defines("_WIN32_WINNT=0x0A00", "WINVER=0x0A00", "UNICODE", "_UNICODE")
add_defines("WIN32_LEAN_AND_MEAN", "NOMINMAX")

target("fluxent")
    set_kind("static")
    set_warnings("all", "error")
    add_includedirs("include", {public = true})
    add_packages("xent-core", {public = true})
    add_packages("tl_expected", {public = true})
    
    add_files("src/*.cpp")
    add_files("src/theme/*.cpp")
    add_files("src/controls/*.cpp")
    
    if is_plat("windows", "mingw") then
        add_syslinks("user32", "gdi32", "dcomp", "d2d1", "d3d11", "dxgi", "dwrite", "dwmapi", "ole32", "uuid", "uxtheme")
    end
target_end()

target("hello_fluxent")
    set_kind("binary")
    set_warnings("all", "error")
    add_deps("fluxent")
    add_files("examples/hello_fluxent/*.cpp")
    
    if is_plat("windows") then
        add_ldflags("/MANIFEST:EMBED", "/MANIFESTINPUT:" .. path.join(os.projectdir(), "fluxent.manifest"), {force = true})
        
        if is_mode("release") then
            add_ldflags("/DEBUG:NONE", {force = true})
            add_ldflags("/PDBALTPATH:NONE", {force = true})
            set_symbols("none")
            add_ldflags("/OPT:REF", "/OPT:ICF", {force = true})
        end
    end

    if is_mode("release") then
        set_strip("all")
    end
target_end()
