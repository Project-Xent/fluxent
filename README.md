# Fluxent

A WinUI 3-style Fluent UI framework written in pure C (C23), layered directly on
the Win32 / DirectX stack. Fluxent renders Fluent Design controls, drives them
with a retained node store, and integrates with Windows platform services through
[cwinrt](../cwinrt) (pure-C WinRT bindings).

## Features

- **Dual render backend**, selected once at startup by capability:
  - *Direct2D / DirectWrite* immediate-mode swap chain — the default path, with a
    render-command list, snapshot diffing, and a per-node render cache.
  - *Windows.UI.Composition* retained visual tree — opt-in via
    `FLUX_USE_COMPOSITION=1`; per-node surfaces, compositor-thread color animations,
    and InteractionTracker scrolling. Falls back to Direct2D over Remote Desktop.
- **Platform integration via WinRT** (cwinrt): true system accent color + light/dark
  theme and change events (`UISettings`), locale-aware `NumberBox` formatting
  (`Globalization.NumberFormatting`), compositor animations and `InteractionTracker`
  (`UI.Composition[.Interactions]`).
- **DirectWrite text** rendering with grapheme-cluster–aware caret/selection.
- **Inertial scrolling** — DirectManipulation on the Direct2D path, InteractionTracker
  on the composition path.
- **Accessibility** — a Win32 UI Automation provider (control types, patterns,
  focus/invoke events).
- **Theming** — light/dark palettes driven by the live system accent.
- **Plugin hook** — register custom renderers per control type via the plugin registry.

## Controls

Buttons (standard / toggle / repeat / hyperlink), Checkbox, RadioButton, ToggleSwitch,
Slider, ProgressBar, ProgressRing, TextBox, PasswordBox, NumberBox, ScrollViewer, Card,
InfoBadge, Divider, List, TabView, plus Flyout, MenuFlyout, and Tooltip popups. Image
and Canvas have type slots with partial support.

## Build

```bash
xmake                        # debug build (default)
xmake f -m release && xmake  # release build
xmake run hello_fluxent      # run the showcase demo
```

`FLUX_USE_COMPOSITION=1 xmake run hello_fluxent` exercises the composition backend.

Requires Windows 10 1903+, a C23 compiler (MSVC or MinGW/Clang), and `xmake`.

## Dependencies

- [`xent-core`](https://github.com/Project-Xent/xent-core)
- [`cwinrt`](https://github.com/Project-Xent/cwinrt)

## Status

The Direct2D backend is the stable default. The composition backend is functional
(DPI scaling, theming, scrolling, and most controls render identically to Direct2D)
but still being hardened — in-window acrylic backdrops are currently disabled, and it
remains opt-in behind `FLUX_USE_COMPOSITION`.

## License

0BSD
