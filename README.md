# ReiPan
Fatal Frame 3 PC Port

## Build

Requires a C/C++ compiler, CMake, Make, Git and SDL3's system development
packages. Dependencies are fetched automatically. Linux builds a minimal static
FFmpeg (MPEG-2 decoder, parser and colour conversion); movie demuxing and PCM
audio are handled by the game. Windows uses prebuilt FFmpeg from `extern/ffmpeg`.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

The executable is `build/ReiPan/ReiPan`. To reuse checked-in shader bytecode,
configure with `-DREIPAN_COMPILE_SHADERS=OFF`.

## Game files

ReiPan does not include the game, and only the European version works: Project
Zero 3, SLES-53825. The first time it starts without finding it, it asks for your
own copy:

- **Disc image (.iso):** ReiPan plays straight from it.
- **IMG_BD.BIN:** the game's data copied from the disc, with `ZERO3.FHD` from the
  disc in the same folder.

Your choice is checked, then saved as `data_folder` under `[paths]` in
`reipan.ini`. Change it later in **Settings → Game data folder…**. A folder holding those
files beside the executable or above it is found without asking.

## Graphics API

ReiPan picks a graphics API by itself: Direct3D 12 on Windows, Vulkan on Linux,
Metal on macOS. Choose another in **Options → Graphics → Graphics API** or
**Settings → Graphics**, then restart. One that cannot be started falls back to
the automatic choice. If one starts but misbehaves, set `backend = auto` under
`[render]` in `reipan.ini`, or start ReiPan once with `SDL_GPU_DRIVER` set to
`direct3d12`, `vulkan` or `metal`.

## Gyro aiming

While the Camera Obscura is raised, a controller with a gyro (DualShock 4,
DualSense, Switch Pro, Steam Controller, ...) aims it by being turned, alongside
the stick and the mouse. It is on by default; **Options → Controls → Gyro** sets
the sensitivity, yaw or roll to turn, and the inversions. The gyro's drift is
learned whenever the controller lies still with the Camera Obscura down.

## Trigger rumble

A controller with motors behind its triggers (Xbox One and later) also plays there
what the rumble is for: heartbeats, blows, a ghost's grip, thunder and the
cutscenes. The grips shake as the game asks, and any other controller behaves as
before. It is on by default; **Options → Controls → Rumble** sets the strengths,
tests each motor, and can add the Camera Obscura (the shutter, the charge and
shutter chances), which the original does not shake for. A controller that Steam
Input stands in for only has its grips.

## PS2 save transfer

Open **Settings → PS2 save transfer…** to transfer European, US or Japanese
gameplay saves. Choose the target region when exporting. For European PS2 saves,
select the original game's PAL/60 Hz timing so mission time records remain correct.

Import from an extracted save folder, an 8 MB memory card, or a PSU/MAX/XPS/SPS/CBS
archive. Select saves and assign their destination slots. Export formats are:

- **ECC (.ps2):** a memory card with spare/ECC bytes, for PCSX2.
- **Raw (.bin):** an 8 MB card without ECC bytes, for OPL.
- **PSU (.psu):** an EMS archive, for uLaunchELF and PS2 save-transfer tools.
- **MAX (.max):** an Action Replay MAX archive.
- **XPS/SPS (.xps/.sps):** an X-Port V2 / SharkPort archive.
- **CBS (.cbs):** a CodeBreaker archive.
- **Extracted folder:** the game's save files, for tools that accept folders.

Archives contain this game's selected saves, not an entire memory card. Card
exports create a blank card populated with the selected saves. Choosing a format
opens its picker; confirming the destination exports immediately. Missing
extensions are added automatically; existing output files/folders are refused.
Imports replace only the selected destination slots. Load imported saves normally
afterward, and close the game's save/load screen before transferring.

Unavailable saves remain visible with an explanation; other valid slots can still
be transferred. Regional script differences can prevent transferring an active
scene or conversation. The extra US/Japanese Trailer unlock has no European
equivalent and is left unobtained when exporting from ReiPan.

## Font texture replacements

Redraw or upscale the PNGs uniformly up to 4×, preserving glyph layout, colours
and transparency. Copy the hash-named directory into `resources/fonts/` beside
the executable and restart. Enable **HD fonts (if available)** in Renderer or
**Use HD Fonts when available** in Game options. Missing or unmatched files
fall back to the original fonts.
