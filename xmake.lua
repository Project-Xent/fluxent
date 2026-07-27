set_project("fluxent")
-- Version from the git tag: release CI injects $XENT_VERSION (tag minus "v").
set_version(os.getenv("XENT_VERSION") or "0.3.0-dev")
set_languages("c23")
add_rules("mode.debug", "mode.release")
set_warnings("all")
set_runtimes("MT")

local unit_tests = {
    "test_subtree",
    "test_fx_host",
    "test_fx_display",
    "test_fx_reconcile",
    "test_fx_rows",
    "test_fx_probe",
    "test_fx_crossaxis",
    "test_fx_grow_wrap",
    "test_fx_list",
    "test_fx_ime",
    "test_fx_text",
    "test_fx_runtime_assembly",
    "test_fx_new_controls",
	"test_gallery_decl",
	"test_gallery_imp",
    "test_fx_engine_degraded",
    "test_fx_txn_errors",
}

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

target("fluxent_gallery_decl")
    set_kind("binary")
    add_deps("fluxent")
    add_files("examples/gallery_decl/*.c", "examples/gallery_decl/pages/**.c", "examples/gallery_shared/*.c")
    add_includedirs("include", "examples/gallery_decl", "examples/gallery_shared")
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

target("fluxent_gallery_imp")
    set_kind("binary")
    add_deps("fluxent")
    add_files("examples/gallery_imp/*.c", "examples/gallery_imp/pages/**.c", "examples/gallery_shared/*.c")
    add_includedirs("include", "src", "examples/gallery_imp", "examples/gallery_shared")
    if is_plat("mingw") then
        add_cflags("-ffunction-sections", "-fdata-sections", { force = true })
        add_ldflags("-Wl,--subsystem,windows", "-Wl,--gc-sections", "-Wl,--icf=safe", { force = true })
    end
    if is_plat("windows") then
        add_ldflags("/SUBSYSTEM:WINDOWS", "/OPT:REF", "/OPT:ICF", { force = true })
    end
target_end()

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

for _, test_name in ipairs(unit_tests) do
    target(test_name)
        set_default(false)
        set_kind("binary")
        add_deps("fluxent")
        add_files("examples/tests/" .. test_name .. ".c")
		if test_name == "test_gallery_imp" then
			add_files(
				"examples/gallery_imp/app.c",
				"examples/gallery_imp/helpers.c",
				"examples/gallery_imp/pages/**.c",
				"examples/gallery_imp/shell.c",
				"examples/gallery_shared/*.c"
			)
		elseif test_name == "test_gallery_decl" then
			add_files("examples/gallery_decl/*.c", "examples/gallery_decl/pages/**.c", "examples/gallery_shared/*.c")
			add_defines("GALLERY_DECL_NO_MAIN")
		end
        add_includedirs("include")
        if test_name == "test_subtree" or test_name == "test_fx_ime"
            or test_name == "test_fx_host" or test_name == "test_fx_display" then
            add_includedirs("src")
        elseif test_name == "test_gallery_imp" then
			add_includedirs("src", "examples/gallery_imp", "examples/gallery_shared")
		elseif test_name == "test_gallery_decl" then
			add_includedirs("src", "examples/gallery_decl", "examples/gallery_shared")
		else
            add_includedirs("src", "src/bridge")
        end
        -- kit's alloc fault-injection hooks live in kit's private src tree
        -- (sibling-layout dev mode only; tests are exempt from API purity).
        if dev and (test_name == "test_fx_runtime_assembly" or test_name == "test_fx_engine_degraded"
            or test_name == "test_fx_txn_errors") then
            add_includedirs("../xent-kit/src")
        end
    target_end()
end

task("test")
    set_menu {
        usage = "xmake test",
        description = "Build and run all unit tests.",
    }
    on_run(function ()
        for _, test_name in ipairs(unit_tests) do
            os.exec("xmake build " .. test_name)
            os.exec("xmake run " .. test_name)
        end
    end)

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
