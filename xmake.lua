-- FluXent: Windows Rendering Backend for Xent UI Framework
set_project("fluxent")
add_rules("mode.debug", "mode.release")
set_languages("c++20")

-- Enable modern Windows API (Windows 10+)
add_defines("_WIN32_WINNT=0x0A00", "WINVER=0x0A00", "UNICODE", "_UNICODE")
add_defines("WIN32_LEAN_AND_MEAN", "NOMINMAX")

includes("../xent-core")

target("fluxent")
    set_kind("static")
    set_warnings("all", "error")
    add_includedirs("include", {public = true})
    add_deps("xent-core")
    
    add_files("src/graphics.cpp")
    add_files("src/window.cpp")
    add_files("src/engine.cpp")
    add_files("src/input.cpp")
    add_files("src/text.cpp")
    add_files("src/theme/*.cpp")
    add_files("src/controls/*.cpp")
    
    if is_plat("windows", "mingw") then
        add_syslinks(
            "user32",
            "gdi32",
            "dcomp",
            "d2d1",
            "d3d11",
            "dxgi",
            "dwrite",
            "dwmapi",
            "ole32",
            "uuid",
            "uxtheme"
        )
    end
target_end()

target("hello_fluxent")
    set_kind("binary")
    set_warnings("all", "error")
    add_deps("fluxent")
    add_files("examples/hello_fluxent/*.cpp")
    
    if is_plat("windows") then
        add_ldflags("/MANIFEST:EMBED", "/MANIFESTINPUT:" .. path.join(os.projectdir(), "fluxent.manifest"), {force = true})
    end
    if is_mode("release") then
        set_strip("all")
    end
target_end()