# USM Toolkit

Cross-platform C++ tool to convert CRI Middleware USM video files to MP4.

## Features

- Convert VP9 USM files to MP4 (native pipeline, no FFmpeg needed)
- Re-encode VP9 to H.264 via FFmpeg for WMP/player compatibility (`--reencode`)
- Non-VP9 streams handled by FFmpeg
- Supports batch processing of directories
- Works on Windows and Linux

## Status

| Feature | Status |
|---------|--------|
| VP9 -> MP4 (native) | Working, tested |
| `--reencode` VP9 -> H.264 | Working, tested |
| ADX/HCA audio decode + mux | Code exists, **untested** (no VP9+audio USM available) |
| Non-VP9 -> MP4 (FFmpeg) | Working, tested |

## Requirements

- C++17 compiler (GCC 8+, Clang 7+, MSVC 2019+)
- CMake 3.16+
- FFmpeg (required for non-VP9 streams and `--reencode` mode)

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
# Convert VP9 to MP4 (native, fast)
usm_toolkit convert video.usm

# Re-encode to H.264 (WMP compatible, slower)
usm_toolkit convert video.usm -r

# Extract raw streams
usm_toolkit extract video.usm

# Specify output directory
usm_toolkit convert video.usm -o output/

# Convert and cleanup temp files
usm_toolkit convert video.usm -c
```

## Options

| Option | Description |
|--------|-------------|
| `-o, --output-dir <dir>` | Output directory |
| `-c, --clean` | Remove temporary .m2v and audio files |
| `-r, --reencode` | Re-encode VP9 to H.264 via FFmpeg (WMP compatible) |

## How it works

VP9 USM files are converted natively by:
1. Demuxing the CRI USM container (CriUsmStream)
2. Parsing IVF-wrapped VP9 frames (IvfParser)
3. Muxing into ISO BMFF (MP4) with vpcC codec config (NativeMp4Muxer)

ADX/HCA audio decoding to L16 PCM is implemented but not yet validated on real VP9+audio files.

Use `--reencode` to transcode VP9 to H.264 for maximum player compatibility (requires FFmpeg).

Non-VP9 streams fall through to FFmpeg.

## Credits

- [UsmToolkit](https://github.com/Rikux3/UsmToolkit) - Original C# implementation
- [VGMToolbox](https://github.com/snakemeat/VGMToolbox) by snakemeat - USM parsing library
- [nlohmann/json](https://github.com/nlohmann/json) - JSON parser (bundled in third_party/)

## License

MIT
