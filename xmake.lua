set_project("fluxent")
set_version("0.1.0")
set_languages("c17")
add_rules("mode.debug", "mode.release")
set_warnings("all")
set_runtimes("MT")


if is_plat("windows") then
    add_defines("_CRT_SECURE_NO_WARNINGS")
end

includes("../xent-core")

add_defines("COBJMACROS")
add_defines("_WIN32_WINNT=0x0A00", "WINVER=0x0A00", "UNICODE", "_UNICODE")
add_defines("WIN32_LEAN_AND_MEAN", "NOMINMAX")

target("fluxent")
    set_kind("static")
    add_deps("xent_core")
    add_includedirs("include", { public = true })
    add_includedirs("src", { private = true })
    add_includedirs("thirdparty/c_d2d_dwrite", { public = true })
    add_headerfiles("include/fluxent/*.h")
    add_files(
        "src/*.c",
        "src/app/*.c",
        "src/controls/behavior/*.c",
        "src/controls/draw/*.c",
        "src/controls/factory/*.c",
        "src/controls/textbox/*.c",
        "src/graphics/*.c",
        "src/input/*.c",
        "src/popup/*.c",
        "src/render/*.c",
        "src/runtime/*.c",
        "src/store/*.c",
        "src/text/*.c",
        "src/theme/*.c",
        "src/window/*.c"
    )
    if is_plat("mingw") then
        add_cflags("-ffunction-sections", "-fdata-sections", { force = true })
    end
    if is_plat("windows") then
        add_cflags("/Gy", "/Gw", "/wd4201", { force = true })
        add_ldflags("/OPT:REF", "/OPT:ICF", { force = true })
    end
    if is_plat("windows", "mingw") then
        add_syslinks("user32", "gdi32", "dcomp", "d2d1", "d3d11",
                     "dxgi", "dwrite", "dwmapi", "ole32", "uuid", "uxtheme", "imm32",
                     "advapi32", "shell32")
    end
target_end()

target("hello_fluxent")
    set_kind("binary")
    add_deps("fluxent")
    add_files("examples/hello_fluxent.c")
    add_includedirs("include")
    if is_plat("mingw") then
        add_cflags("-ffunction-sections", "-fdata-sections", { force = true })
        add_ldflags("-Wl,--subsystem,windows", "-Wl,--gc-sections", { force = true })
    end
    if is_plat("windows") then
        add_ldflags("/SUBSYSTEM:WINDOWS", "/OPT:REF", "/OPT:ICF", { force = true })
    end
target_end()
