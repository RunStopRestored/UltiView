# ultraView with a remote C64 keyboard

A private build of [ultraView](https://github.com/Toddler-Boy/ultraView) 1.4.4
(GPL-3) that also sends keys to the C64 while it streams.

## Keyboard

Typed keys go into the C64's KERNAL keyboard buffer ($0277, count $C6) through
the C64u REST API (`Source/Remote/C64Keyboard.cpp`). For each key it waits until
the program has taken the previous one ($C6 = 0), then writes the key and the
count with `PUT /v1/machine:writemem?address=..&data=..`. The bytes travel in the
URL: a `POST` with a body leaves a file in the C64u's /Temp, and once /Temp is
full (about 330 files) every POST write fails ("Could not read data from
attachment").

Ctrl+K switches the keyboard between the C64 and ultraView (the title says
which). Ctrl shortcuts, F11 and F12 stay ultraView's.

| PC | C64 |
|---|---|
| letters, digits, punctuation | the same; SHIFT + letter: the shifted (graphics) character |
| Return | RETURN |
| Backspace, Delete | DEL (SHIFT: INST) |
| Insert | INST |
| Home | HOME (SHIFT: CLR) |
| cursor keys | cursor keys |
| F1-F8 | F1-F8 |
| Esc | RUN/STOP |
| ` or _ | left arrow |

Only programs that read the keyboard buffer see these keys (BASIC, the KERNAL,
Ultimidi); programs that scan the keyboard matrix themselves do not.

## Stream pause

Ctrl+S pauses the stream so another program on this PC (Claude's C64u capture
tools, for example) can use it without quitting ultraView. ultraView sends
`PUT /v1/streams/video:stop` and `audio:stop` (only if it started them) and
closes its UDP ports (11000 video, 11001 audio). The last frame stays on screen
under a "STREAM PAUSED" banner, and the title shows "stream paused". While
paused, the reconnect loop does not touch the streams.

Ctrl+S again resumes: ultraView starts both streams again, aimed at this PC,
taking them back from whoever has them.

## C64U files (Ctrl+O)

The Ultimate's own menu closes on the first key that comes over the network, so
ultraView has its own browser for the C64U's storage (SD, USB0, Flash, Temp).
It lists folders over the C64U's FTP server (the REST API has no listing) and
runs files with the REST runners, straight from the card, without uploading.

Keys: Up/Down, Enter (open a folder or run a file), Backspace or Left (up a
folder), Right (open a folder), letters (jump to the first name starting with
them), Esc or Ctrl+O (close). Double-click works too. The last folder is kept.

| File | What happens |
|---|---|
| .prg | `PUT /v1/runners:run_prg?file=` |
| .crt | `PUT /v1/runners:run_crt?file=` (a reboot first if the last cartridge may have been an EasyFlash, as with dropped files) |
| .sid, .mod | `sidplay` / `modplay` |
| .d64 .g64 .d71 .g71 .d81 | drive A set to 1541/1571/1581 and switched on, the image mounted, the C64 reset; at READY. `LOAD"*",8,1` is written on the screen and RETURN, RUN, RETURN go into the keyboard buffer, so the first program loads and runs |

PRG, CRT and disk runs look the name up in Games.csv for PAL/NTSC and the
joystick port, as dropped files do. A disk run after a cartridge reboots the
machine first, since a cartridge survives a reset.

## Building on Windows without clang-cl or 7-Zip

```
git submodule update --init --recursive
git -C modules/lime apply ../../patches/lime-msvc-regex.patch
cmake -S . -B Builds/vs22 -G "Visual Studio 17 2022" -A x64 -DULTRA_MSVC=ON -DBUILD_EXTRAS=OFF -DBUILD_TESTING=OFF
cmake --build Builds/vs22 --config Release --parallel
python Tools/make_win_exe.py Builds/vs22/ultraView_artefacts/Release/ultraView.exe dist/ultraView.exe
```

- `ULTRA_MSVC=ON` builds with MSVC instead of the ClangCL toolset.
- JUCE is pinned to develop as of the 1.4.4 release (b7baf331, 2026-09-22); later
  develop changed `MessageBoxOptions::withButton`.
- `patches/lime-msvc-regex.patch`: MSVC's `<regex>` has no `multiline`; the
  shader include search uses an equivalent pattern there.
- `Tools/make_win_exe.py` appends Data.pak to the exe (what `ci/build.sh` does
  with 7z).
