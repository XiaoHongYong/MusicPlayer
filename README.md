# MusicPlayer

A small, fast, self-drawn desktop music player. More and more music players today are bloated, slow, and getting harder to use — this project is the opposite: it plays your music, shows the lyrics, and stays out of your way. It's a personal project built for my own enjoyment, refined bit by bit when I have time.

## Features

* **Music playback** with support for a wide range of formats (MP3, AAC/M4A, OGG, FLAC, WAV, and more)
* **Scrolling lyrics** display alongside the music, in sync with the current song
* **Small footprint & low resource usage** — light on memory, CPU, and disk
* **Web-based media center** — manage your library through the browser: albums, artists, search, stats and more
* **Cool skins** — switchable, customizable, shipped with Neon / Glass skins
* **Open source, free, and ad-free**
* **Cross-platform**: Windows, macOS

## Development

### Tech Stack

* C++, C++17
* macOS UI: Cocoa / AppKit; Windows: Win32 (plus a legacy GTK2 branch)
* CMake (generates the Xcode project, built with `xcodebuild` on macOS)
* Rendering: Agg (2D), TinyJS (embedded JS engine) for the skin system
* Media engines: faad2 (MP4/AAC), flac, minimp3 (MP3); on macOS also CoreMedia / AVPlayer; output via CoreAudio
* Web media center: Vue + Quasar (front end), backed by the C++ `LocalServer` (REST + SSE)

### Development Environment

| Tool | Purpose |
|---|---|
| C++ / VSCode | Main language and editor |
| Windows: Visual Studio Community 2022 | Build on Windows |
| Mac: Xcode | Build on macOS |
| CMake | Build system |
| Node.js 18 (Vue + Quasar) | Web media center front end |
| Python 3 | Required for the build scripts |

### Building

```bash
./build.sh Release -g -b   # -g generate Xcode project, -b build
```

See [CLAUDE.md](CLAUDE.md) for the full build options (`-p` packaging, unit tests with `-DUT=ON`, i18n workflow, and the important gotchas).

## Documentation

- Skin authoring: [`docs/skin-authoring.md`](docs/skin-authoring.md)
- Skin compiler: [`docs/skin-compiler.md`](docs/skin-compiler.md)
- Web media center design & roadmap: [`docs/web-console/README.md`](docs/web-console/README.md)
- Album art download: [`docs/album-art-download.md`](docs/album-art-download.md)

## License

Open source under the [MIT License](LICENSE-MIT). Free, with no ads.