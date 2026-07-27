# Fluxent

The Windows Host for Project Xent, written in C23 over Win32, Direct2D,
DirectWrite, DirectComposition, and pure-C WinRT bindings from
[cwinrt](https://github.com/Project-Xent/cwinrt).

Applications use the standard XTK control protocol. Fluxent does not expose a
second `flux_create_*` control family or a renderer-plugin ABI.

## Features

- atomic `XtkBatch` Host with generation-checked controls, typed properties,
  normalized events, transactional Components, and typed Providers;
- immutable Xent display-list record/replay with private built-in renderer
  metadata and resource interning;
- DirectWrite measurement/rendering, grapheme-aware editing, IME composition,
  selection, undo, and clipboard integration;
- DirectManipulation and Composition scrolling;
- UI Automation, focus/capture/routing, system accent, light/dark themes,
  NumberBox globalization, popups, menus, tooltips, and window backdrops;
- declarative `flux_run` and direct imperative `XtkScene`/`XtkHost` entry points
  over the same retained controls.

## Controls

The generated XTK catalog is the source of truth. It includes text and image,
buttons and split/menu variants, toggles, value controls, text editors,
navigation, TabView, dialogs and teaching tips, virtualized list/grid/items/tree
controls, scrolling and pull-to-refresh, status controls, and layout containers.

The declarative and imperative galleries exercise the same pages and behavior.

## Build

```sh
xmake
xmake test
xmake run fluxent_gallery_decl
xmake run fluxent_gallery_imp
```

`FLUX_USE_COMPOSITION=1` selects the Composition path; otherwise Fluxent uses
the Direct2D path. The full gate includes both Gallery models, control behavior,
Host transactions, display-list ownership, DirectWrite measurement, IME,
virtualization, allocation failures, and degraded rendering.

Requires Windows 10 1903+, xmake, and a C23 compiler.

## Dependencies

- [`xent-core`](https://github.com/Project-Xent/xent-core)
- [`xent-kit`](https://github.com/Project-Xent/xent-kit)
- [`cwinrt`](https://github.com/Project-Xent/cwinrt)

## License

0BSD
