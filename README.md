# USM Toolkit

Cross-platform C++ tool to convert CRI Middleware USM video files to MP4, and other formats.

## Features

- Extract raw video (`.m2v`) and audio (`.adx`/`.hca`) streams from USM files
- Convert USM to MP4 using FFmpeg
- Supports batch processing of directories
- Works on Windows and Linux

## Requirements

- C++17 compiler (GCC 8+, Clang 7+, MSVC 2019+)
- CMake 3.16+
- FFmpeg (must be in PATH)

## Building

### Windows

```powershell
cmake -B build
cmake --build build --config Release
```

Executable: `build/Release/usm_toolkit.exe`

### Linux

```bash
./build.sh
```

Executable: `build_linux/usm_toolkit`

## Usage

```bash
# Extract raw streams
usm_toolkit extract video.usm

# Convert to MP4
usm_toolkit convert video.usm

# Convert and cleanup temp files
usm_toolkit convert video.usm -c

# Specify output directory
usm_toolkit convert video.usm -o output/ -c
```

## Configuration

Edit `config.json` to customize FFmpeg parameters:

```json
{
    "VideoParameter": "-c:v copy",
    "AudioParameter": "-c:a aac",
    "OutputFormat": "mp4"
}
```

## Documentation

See [USM_FORMAT.md](USM_FORMAT.md) for technical documentation on the USM format.

## Credits

- [UsmToolkit](https://github.com/Rikux3/UsmToolkit) - Original C# implementation
- [VGMToolbox](https://github.com/snakemeat/VGMToolbox) by snakemeat - USM parsing library
- [nlohmann/json](https://github.com/nlohmann/json) - JSON parser (bundled in third_party/)

## License

MIT
