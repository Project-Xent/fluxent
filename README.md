# Fluxent

A WinUI 3-style UI framework written in pure C.

## Features

- **DirectComposition** for hardware-accelerated compositing and smooth animations
- **Direct2D / DirectWrite** rendering with render caching and snapshot optimization
- **DirectManipulation** for touch/trackpad inertia scrolling and pinch-to-zoom
- **Fluent Design** with Mica backdrop, Acrylic materials, and reveal effects
- Built-in controls: Button, Checkbox, Slider, ProgressBar/Ring, TextBox, ScrollViewer, and more
- Flyout, MenuFlyout, and Tooltip support
- Lottie animation playback via integrated lottie renderer

## Build

```bash
xmake
xmake run hello_fluxent
```

Requires Windows 10 1903+ and a C17 compiler (MSVC or MinGW).

## Demo

<video src="https://raw.githubusercontent.com/Project-Xent/fluxent/main/examples/bin/HelloFluxentVideo.mp4" controls="controls" width="700"></video>

**Downloads** (Windows x64, MinGW/UCRT):
- [hello_fluxent.exe](examples/bin/hello_fluxent.exe)
- [HelloFluxentVideo.mp4](examples/bin/HelloFluxentVideo.mp4)

## License

0BSD
