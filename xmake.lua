set_project("fluxent")
set_version(os.getenv("XENT_VERSION") or "0.3.2")
set_languages("c23")
add_rules("mode.debug", "mode.release")
if is_plat("mingw") and is_mode("release") then
    set_symbols("none")
end
set_warnings("all")
set_runtimes("MT")

if is_mode("release") then
    set_optimize("faster")
end

if is_plat("windows") then
    add_defines("_CRT_SECURE_NO_WARNINGS")
end

add_repositories("xent-repo https://github.com/Project-Xent/xent-repo.git")
local dev = os.isdir(path.absolute(path.join(os.scriptdir(), "..", "xent-kit")))
if dev then
    includes(path.join(os.scriptdir(), "..", "xent-kit"))
    includes(path.join(os.scriptdir(), "..", "cwinrt"))
else
    add_requires("xent-core", "xent-kit", "cwinrt")
end

add_defines("COBJMACROS")
add_defines("_WIN32_WINNT=0x0A00", "WINVER=0x0A00", "UNICODE", "_UNICODE")
add_defines("WIN32_LEAN_AND_MEAN", "NOMINMAX")

target("fluxent")
    set_kind("static")
    if dev then
        add_deps("xent_core", "xtk", "cwinrt-rt",
                 "cwinrt-bindings-foundation",
                 "cwinrt-bindings-ui-composition",
                 "cwinrt-bindings-ui-composition-interactions",
                 "cwinrt-bindings-ui-viewmanagement",
                 "cwinrt-bindings-globalization",
                 "cwinrt-bindings-globalization-datetimeformatting",
                 "cwinrt-bindings-globalization-numberformatting",
                 "cwinrt-bindings-ui-input")
    else
        add_packages("xent-core", "xent-kit", "cwinrt", { public = true })
    end
    add_includedirs("include", { public = true })
    add_includedirs("src", { private = true })
    add_includedirs("thirdparty/c_d2d_dwrite", { public = true })
    if dev then
        add_includedirs("../cwinrt/include", { public = true })
    end
    add_headerfiles("include/(fluxent/*.h)")
    add_files(
        "src/a11y/*.c",
        "src/app/*.c",
        "src/compose/*.c",
        "src/controls/behavior/*.c",
        "src/controls/draw/*.c",
        "src/controls/factory/*.c",
        "src/controls/textbox/*.c",
        "src/bridge/*.c",
        "src/host/*.c",
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
                     "dxgi", "dwrite", "dwmapi", "ole32", "oleaut32", "uuid", "uxtheme", "imm32",
                     "advapi32", "shell32", "coremessaging", "uiautomationcore", "runtimeobject", "windowscodecs")
    end
target_end()

local function gui_binary(name, dirs)
    target(name)
        set_kind("binary")
        add_deps("fluxent")
        add_includedirs("include")
        for _, dir in ipairs(dirs) do
            add_files(dir .. "/**.c")
            add_includedirs(dir)
        end
        if is_plat("mingw") then
            add_cflags("-ffunction-sections", "-fdata-sections", { force = true })
            add_ldflags("-Wl,--subsystem,windows", "-Wl,--gc-sections", "-Wl,--icf=safe", { force = true })
        end
        if is_plat("windows") then
            add_ldflags("/SUBSYSTEM:WINDOWS", "/OPT:REF", "/OPT:ICF", { force = true })
        end
    target_end()
end

gui_binary("fluxent_gallery_decl", { "examples/gallery_decl", "examples/gallery_shared" })
gui_binary("fluxent_gallery_imp", { "examples/gallery_imp", "examples/gallery_shared" })
gui_binary("titlebar_demo", { "examples/titlebar_demo" })
gui_binary("installer_wizard", { "examples/installer_wizard" })

target("bench_fx_release")
    set_default(false)
    set_kind("binary")
    add_deps("fluxent")
    add_files("bench/bench_fx_release.c")
    add_includedirs("include", "src")
    if dev then
        add_includedirs("../xent-kit/src")
    end
target_end()

target("test_fx_txn_errors")
    set_default(false)
    set_kind("binary")
    add_deps("fluxent")
    add_files("examples/tests/test_fx_txn_errors.c")
target_end()

task("dist")
    set_menu {
        usage = "xmake dist [arch ...]",
        description = "Build the release bundles (headers, merged libxent.a, import libraries, xent.pc).",
        options = {{nil, "archs", "vs", nil, "Architectures to build: x86_64, arm64 (default: both)."}},
    }
    on_run(function ()
        import("core.base.option")
        local argv = {"lua", path.join(os.scriptdir(), "tools", "dist.lua")}
        for _, arch in ipairs(option.get("archs") or {}) do table.insert(argv, arch) end
        os.execv("xmake", argv)
    end)
task_end()
