set_project("fluxent")
set_version("0.1.0")
set_languages("c17")
add_rules("mode.debug", "mode.release")
set_warnings("all")
set_runtimes("MT")

-- Release builds at -O2 (xmake's "faster") rather than mode.release's default -O3.
-- O3's extra inlining/unrolling/vectorization buys nothing for event-driven UI code
-- and only adds size; O2 is a few KB smaller at equal (often better) runtime perf.
if is_mode("release") then
    set_optimize("faster")
end


if is_plat("windows") then
    add_defines("_CRT_SECURE_NO_WARNINGS")
end

-- Pinned packages by default; sibling source when present (local dev).
-- xent-core comes transitively via xent-kit.
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
    -- Granular cwinrt targets (dev) vs the umbrella package (343 binding TUs,
    -- unused ones stripped by /OPT:REF); fluxent uses Foundation/Composition/Interactions.
    if dev then
        add_deps("xent_core", "xtk", "cwinrt-rt",
                 "cwinrt-bindings-foundation",
                 "cwinrt-bindings-composition",
                 "cwinrt-bindings-interactions",
                 "cwinrt-bindings-viewmanagement",
                 "cwinrt-bindings-numberformatting",
                 "cwinrt-bindings-input")
    else
        -- public: fluxent's public headers expose xtk/xent/cwinrt types.
        add_packages("xent-core", "xent-kit", "cwinrt", { public = true })
    end
    add_includedirs("include", { public = true })
    add_includedirs("src", { private = true })
    add_includedirs("thirdparty/c_d2d_dwrite", { public = true })
    if dev then
        -- granular binding targets don't export their include dir; point at siblings.
        add_includedirs("../cwinrt/include", { public = true })
    end
    add_headerfiles("include/fluxent/*.h")
    add_files(
        "src/*.c",
        "src/app/*.c",
        "src/compose/*.c",
        "src/controls/behavior/*.c",
        "src/controls/draw/*.c",
        "src/controls/factory/*.c",
        "src/controls/textbox/*.c",
        "src/bridge/*.c",
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

target("fluxent_gallery")
    set_kind("binary")
    add_deps("fluxent")
    add_files("examples/gallery/*.c")
    add_includedirs("include", "examples/gallery")
    if is_plat("mingw") then
        add_cflags("-ffunction-sections", "-fdata-sections", { force = true })
        -- --icf=safe folds byte-identical functions (the MSVC /OPT:ICF equivalent);
        -- "safe" leaves address-taken functions alone so fn-pointer identity holds.
        add_ldflags("-Wl,--subsystem,windows", "-Wl,--gc-sections", "-Wl,--icf=safe", { force = true })
    end
    if is_plat("windows") then
        add_ldflags("/SUBSYSTEM:WINDOWS", "/OPT:REF", "/OPT:ICF", { force = true })
    end
target_end()

target("test_subtree")
    set_kind("binary")
    add_deps("fluxent")
    add_files("examples/tests/test_subtree.c")
    add_includedirs("include")
target_end()

target("test_fx_el")
    set_kind("binary")
    add_deps("fluxent")
    add_files("examples/tests/test_fx_el.c")
    add_includedirs("include")
target_end()

target("test_fx_reconcile")
    set_kind("binary")
    add_deps("fluxent")
    add_files("examples/tests/test_fx_reconcile.c")
    add_includedirs("include", "src", "src/bridge")
target_end()

target("test_fx_rows")
    set_kind("binary")
    add_deps("fluxent")
    add_files("examples/tests/test_fx_rows.c")
    add_includedirs("include", "src", "src/bridge")
target_end()

target("test_fx_probe")
    set_kind("binary")
    add_deps("fluxent")
    add_files("examples/tests/test_fx_probe.c")
    add_includedirs("include", "src", "src/bridge")
target_end()

target("test_fx_crossaxis")
    set_kind("binary")
    add_deps("fluxent")
    add_files("examples/tests/test_fx_crossaxis.c")
    add_includedirs("include", "src", "src/bridge")
target_end()

target("test_fx_grow_wrap")
    set_kind("binary")
    add_deps("fluxent")
    add_files("examples/tests/test_fx_grow_wrap.c")
    add_includedirs("include", "src", "src/bridge")
target_end()

target("test_fx_list")
    set_kind("binary")
    add_deps("fluxent")
    add_files("examples/tests/test_fx_list.c")
    add_includedirs("include", "src", "src/bridge")
target_end()

target("test_fx_new_controls")
    set_kind("binary")
    add_deps("fluxent")
    add_files("examples/tests/test_fx_new_controls.c")
    add_includedirs("include", "src", "src/bridge")
target_end()

target("hello_fluxent")
    set_kind("binary")
    add_deps("fluxent")
    add_files("examples/hello_fluxent/*.c")
    add_includedirs("include", "examples/hello_fluxent")
    if is_plat("mingw") then
        add_cflags("-ffunction-sections", "-fdata-sections", { force = true })
        add_ldflags("-Wl,--subsystem,windows", "-Wl,--gc-sections", "-Wl,--icf=safe", { force = true })
    end
    if is_plat("windows") then
        add_ldflags("/SUBSYSTEM:WINDOWS", "/OPT:REF", "/OPT:ICF", { force = true })
    end
target_end()

target("titlebar_demo")
    set_kind("binary")
    add_deps("fluxent")
    add_files("examples/titlebar_demo/*.c")
    add_includedirs("include", "examples/titlebar_demo")
    if is_plat("mingw") then
        add_cflags("-ffunction-sections", "-fdata-sections", { force = true })
        add_ldflags("-Wl,--subsystem,windows", "-Wl,--gc-sections", "-Wl,--icf=safe", { force = true })
    end
    if is_plat("windows") then
        add_ldflags("/SUBSYSTEM:WINDOWS", "/OPT:REF", "/OPT:ICF", { force = true })
    end
target_end()
