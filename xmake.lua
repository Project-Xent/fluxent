set_project("fluxent")
set_version("0.1.0")
set_languages("c17")
add_rules("mode.debug", "mode.release")
set_warnings("all", "error")
set_runtimes("MT")

if is_plat("mingw") then
    set_toolchains("mingw", { mingw = "C:\\Users\\Wynn\\llvm-mingw-20260324-ucrt-x86_64" })
end

includes("../xent-core")

add_defines("COBJMACROS")
add_defines("_WIN32_WINNT=0x0A00", "WINVER=0x0A00", "UNICODE", "_UNICODE")
add_defines("WIN32_LEAN_AND_MEAN", "NOMINMAX")

target("fluxent")
    set_kind("static")
    add_deps("xent_core")
    add_includedirs("include", { public = true })
    add_headerfiles("include/fluxent/*.h")
    add_files("src/*.c", "src/controls/*.c", "src/theme/*.c")
    add_cflags("-ffunction-sections", "-fdata-sections", { force = true })
    if is_plat("windows", "mingw") then
        add_syslinks("user32", "gdi32", "dcomp", "d2d1", "d3d11",
                     "dxgi", "dwrite", "dwmapi", "ole32", "uuid", "uxtheme", "imm32")
    end
target_end()

target("hello_fluxent")
    set_kind("binary")
    add_deps("fluxent")
    add_files("examples/hello_fluxent.c")
    add_includedirs("include")
    add_cflags("-ffunction-sections", "-fdata-sections", { force = true })
    if is_plat("windows", "mingw") then
        add_ldflags("-Wl,--subsystem,windows", "-Wl,--gc-sections", { force = true })
    end
target_end()
