set_project("fluxent")
set_version(os.getenv("XENT_VERSION") or "0.4.0")
set_languages("c23")
add_rules("mode.debug", "mode.release")
if is_plat("mingw") and is_mode("release") then
    set_symbols("none")
end
set_warnings("all")
set_runtimes("MT")

if is_mode("release") then
    set_optimize("smallest")
end

option("lto")
    set_default(false)
    set_showmenu(true)
    set_description("Enable ThinLTO for release builds (breaks cross-toolchain static archive linking)")
option_end()

if is_plat("windows") then
    add_defines("_CRT_SECURE_NO_WARNINGS")
end

if is_plat("mingw") and is_mode("release") and has_config("lto") then
    add_cflags("-flto=thin", { force = true })
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
                 "cwinrt-bindings-ui-notifications",
                 "cwinrt-bindings-data-xml-dom",
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
        "src/controls/*.c",
        "src/controls/behavior/*.c",
        "src/controls/draw/*.c",
        "src/controls/factory/*.c",
        "src/controls/textbox/*.c",
        "src/bridge/*.c",
        "src/host/*.c",
        "src/graphics/*.c",
        "src/input/*.c",
        "src/motion/*.c",
        "src/popup/*.c",
        "src/render/*.c",
        "src/runtime/*.c",
        "src/shell/*.c",
        "src/store/*.c",
        "src/text/*.c",
        "src/theme/*.c",
        "src/window/*.c"
    )
    if is_plat("mingw") then
        add_cflags("-ffunction-sections", "-fdata-sections", { force = true })
        if is_mode("release") then
            add_cflags("-fmerge-all-constants", { force = true })
        end
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

option("hot")
    set_default(false)
    set_showmenu(true)
    set_description("Build the galleries as a host executable plus a hot-reloadable module")
option_end()

if is_plat("windows") and is_mode("release") and has_config("hot") then
    for _, name in ipairs({ "fluxent", "xtk", "xent_core", "xent_dev" }) do
        target(name)
            set_optimize("faster")
        target_end()
    end
end

local function hybrid_crt()
    local d = is_mode("debug") and "d" or ""
    return { "/NODEFAULTLIB:libucrt" .. d .. ".lib", "/DEFAULTLIB:ucrt" .. d .. ".lib" }
end

target("xent_dev")
    set_default(false)
    set_kind("shared")
    add_deps("fluxent")
    add_files("src/hot/*.c")
    add_includedirs("src", { private = true })
    add_includedirs("dev/include", { public = true })
    if is_plat("mingw") then
        add_ldflags("-Wl,--export-all-symbols", { force = true })
        add_linkgroups("fluxent", "xtk", "xent_core", { whole = true })
    end
    if is_plat("windows") then
        add_defines("FLUX_IMPORT", { interface = true })
        local crt = hybrid_crt()
        add_shflags(crt[1], crt[2], { force = true })
        local tools = path.join(os.scriptdir(), "tools")
        on_config(function (target)
            target:add("shflags", "/def:" .. path.join(target:autogendir(), "xent_dev.def"), { force = true })
        end)
        before_link(function (target)
            import("xent_dev_def", { rootdir = tools })(target, path.join(target:autogendir(), "xent_dev.def"))
        end)
    end
target_end()

local function gui_target(name, spec)
    target(name)
        set_kind(spec.kind or "binary")
        add_deps(spec.hot and "xent_dev" or "fluxent")
        add_includedirs("include")
        if spec.hot == "module" then
            set_prefixname("")
            add_defines("GALLERY_HOT_MODULE")
        elseif spec.hot == "host" then
            add_defines("GALLERY_HOT")
        end
        local host = spec.hot == "host"
        on_load(function (target)
            target:add("defines", format('GALLERY_VERSION="%s"', target:version() or "dev"))
            if host then target:add("defines", format('GALLERY_ROOT=L"%s"', path.unix(os.projectdir()))) end
        end)
        add_files(table.unpack(spec.files))
        for _, dir in ipairs(spec.dirs) do
            add_includedirs(dir)
            if dir == "examples/gallery_shared" then
                add_files(dir .. "/assets/*", { rules = "utils.bin2c", zeroend = false })
            end
        end
        if is_plat("mingw") then
            add_cflags("-ffunction-sections", "-fdata-sections", { force = true })
            if is_mode("release") then
                add_cflags("-fmerge-all-constants", { force = true })
            end
            add_ldflags("-Wl,--gc-sections", "-Wl,--icf=safe", { force = true })
            if (spec.kind or "binary") == "binary" then
                add_ldflags("-Wl,--subsystem,windows", { force = true })
            end
            if is_mode("release") and has_config("lto") then
                add_ldflags("-flto=thin", { force = true })
            end
        end
        if is_plat("windows") then
            add_cflags("/Gy", "/Gw", { tools = "cl", force = true })
            add_ldflags("/SUBSYSTEM:WINDOWS", "/OPT:REF", "/OPT:ICF", { force = true })
            if spec.hot then
                local crt = hybrid_crt()
                add_ldflags(crt[1], crt[2], { force = true })
                add_shflags(crt[1], crt[2], { force = true })
            elseif is_mode("release") then
                add_ldflags("/NODEFAULTLIB:libucrt.lib", "/DEFAULTLIB:ucrt.lib", { force = true })
            end
        end
    target_end()
end

local function gui_binary(name, dirs)
    local files = {}
    for _, dir in ipairs(dirs) do table.insert(files, dir .. "/**.c") end
    gui_target(name, { dirs = dirs, files = files })
end

local decl_dirs = { "examples/gallery_decl", "examples/gallery_shared" }
local imp_dirs  = { "examples/gallery_imp", "examples/gallery_shared" }
local shared_c  = "examples/gallery_shared/*.c"
local decl_view = "examples/gallery_decl/**.c|gallery_host.c"
local imp_host  = { "examples/gallery_imp/main.c", "examples/gallery_imp/app.c", "examples/gallery_imp/gallery_post.c" }
local imp_view  = {
    "examples/gallery_imp/pages/**.c", "examples/gallery_imp/helpers.c",
    "examples/gallery_imp/gallery_page.c", "examples/gallery_imp/gallery_post.c",
}

if has_config("hot") then
    gui_target("fluxent_gallery_decl", { dirs = decl_dirs, hot = "host", files = { "examples/gallery_decl/gallery_host.c", shared_c } })
    gui_target("fluxent_gallery_decl_view", { dirs = decl_dirs, hot = "module", kind = "shared", files = { decl_view, shared_c } })
    gui_target("fluxent_gallery_imp", { dirs = imp_dirs, hot = "host", files = { imp_host[1], imp_host[2], imp_host[3], shared_c } })
    gui_target("fluxent_gallery_imp_view", { dirs = imp_dirs, hot = "module", kind = "shared", files = { imp_view[1], imp_view[2], imp_view[3], imp_view[4], shared_c } })
else
    gui_target("fluxent_gallery_decl", { dirs = decl_dirs, files = { decl_view, shared_c } })
    gui_target("fluxent_gallery_imp", { dirs = imp_dirs, files = { "examples/gallery_imp/**.c", shared_c } })
end
gui_binary("titlebar_demo", { "examples/titlebar_demo" })
gui_binary("installer_wizard", { "examples/installer_wizard" })
gui_binary("a11y_conformance", { "examples/a11y_conformance" })
gui_binary("shell_demo", { "examples/shell_demo" })

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
