# UltiView: ultraView with a remote C64 keyboard

A modified fork of [ultraView](https://github.com/Toddler-Boy/ultraView) 1.4.4
(GPL-3, changed since November 2026) that also sends keys to the C64 while it
streams. Fork versions start at 1.5.0.

## Updates

The version badge checks this fork's releases, not upstream's:
`https://raw.githubusercontent.com/RunStopRestored/UltiView/c64-keyboard/docs/api/latest-windows.json`
(version, download url, sha256, bytes, notes). To publish a release: raise
`VERSION`, build `dist/UltiView.exe`, create the GitHub release `v<version>`
with that exe attached, then write `docs/api/latest-windows.json` with the
exe's sha256 and size, commit and push. The release asset must be exactly the
file the manifest's sha256 was taken from. Upstream's release workflow
(`.github/workflows/build.yaml`) is deleted on this branch: it would run on
the release tags and publish upstream-style releases and manifests. If an
upstream merge brings it back, delete it again. Actions itself stays on:
GitHub Pages builds the web page with it.

The web page, https://runstoprestored.github.io/UltiView/, is GitHub Pages
from `docs/` on branch c64-keyboard. Its download button reads
`docs/api/latest-windows.json`, so it follows each release by itself.

## Keyboard

Typed keys go to the C64 through the C64u REST API
(`Source/Remote/C64Keyboard.cpp`), one of two ways, chosen per C64u.

**Keyboard matrix** (newer firmware, e.g. C64U 1.2.1). The first key asks
`GET /v1/machine:input`: 200 means the firmware can press keys in the C64's
keyboard matrix. Each PC key becomes the C64 keys that type it
(`["left_shift", "a"]`), and the keys typed so far go in one
`POST /v1/machine:input` as `tap` events (up to 64; the firmware plays them one
after the other). Every program sees these keys, games that scan the matrix
and the Ultimate's own menu included (it no longer closes on the first key).
This JSON POST leaves no file in /Temp. If the route later answers 404 or 501,
the next key asks again.

**Keyboard buffer** (fallback, when `machine:input` answers 404). The keys go
into the KERNAL keyboard buffer ($0277, count $C6). For each key it waits until
the program has taken the previous one ($C6 = 0), then writes the key and the
count with `PUT /v1/machine:writemem?address=..&data=..`. The bytes travel in the
URL: a `POST` with a body leaves a file in the C64u's /Temp, and once /Temp is
full (about 330 files) every POST write fails ("Could not read data from
attachment").

Ctrl+K switches the keyboard between the C64 and UltiView (the title says
which). Ctrl shortcuts, F11 and F12 stay UltiView's.

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
| Alt + key | C= + that key (matrix only) |
| Alt alone | C= (tapped when Alt goes up, if no key was typed with it) |
| Alt + Shift | C= + SHIFT, the character set switch |
| Page Up | RESTORE (matrix only) |
| Alt + Page Up | C= + RESTORE: the Ultimate menu, through `PUT /v1/machine:menu_button` (firmware 1.2.1 takes an injected C= + RESTORE but does not open the menu for it) |

Alt is the Commodore key and Page Up is RESTORE, as in VICE. Neither has a
PETSCII code, so with the keyboard buffer they do nothing. Alt+Enter
(fullscreen) and Alt+F4 stay UltiView's. JUCE does not tell left Alt from
right, so either Alt works; AltGr (Ctrl+Alt) is not C=.

With the keyboard buffer, only programs that read the buffer see these keys
(BASIC, the KERNAL, Ultimidi); programs that scan the keyboard matrix
themselves do not. With the matrix, all of them do. The log (Shift+F11) says
which way the C64u got.

## Stream pause

Ctrl+S pauses the stream so another program on this PC (Claude's C64u capture
tools, for example) can use it without quitting UltiView. UltiView sends
`PUT /v1/streams/video:stop` and `audio:stop` (only if it started them) and
closes its UDP ports (11000 video, 11001 audio). The last frame stays on screen
under a "STREAM PAUSED" banner, and the title shows "stream paused". While
paused, the reconnect loop does not touch the streams.

Ctrl+S again resumes: UltiView starts both streams again, aimed at this PC,
taking them back from whoever has them.

## Stream start

Before every stream start (startup, reconnect, Retry) UltiView stops both
streams on the C64U and pings it once, then starts them. A C64U stuck
refusing streams to this PC ("Network Host Resolve Error", HTTP 404, after
another computer had the stream) takes them again after a ping from it.
A refused start is tried once more the same way; then the streams stay
paused under a "STREAM NOT STARTED" banner and a dialog gives the C64U's
reason, with Retry (Ctrl+S also retries) and Close.

The UDP receivers read one datagram at a time without blocking (see
`patches/ultra-shared-ultiview.patch`): a blocking read waited for a full
packet's worth of bytes, so a short datagram with nothing after it froze the
receiver thread, and with it the reconnect and the app's shutdown.

## Stream audio

The C64U streams a little under 48 kHz by its own clock (PAL about 47983 Hz,
NTSC about 47940 Hz). UltiView resamples the stream, also at 48 kHz, and
nudges the ratio (at most 0.5 %) to hold its buffer at about two reads' worth;
before, the buffer ran dry every few seconds and each time clicked. A buffer
that does run dry plays silence until it is refilled.

## C64U files (Ctrl+O)

With older firmware the Ultimate's own menu closes on the first key that comes
over the network (the keyboard buffer way), so UltiView has its own browser for the C64U's storage (SD, USB0, Flash, Temp).
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
git -C Source/ultra-shared apply ../../patches/ultra-shared-ultiview.patch
cmake -S . -B Builds/vs22 -G "Visual Studio 17 2022" -A x64 -DULTRA_MSVC=ON -DBUILD_EXTRAS=OFF -DBUILD_TESTING=OFF
cmake --build Builds/vs22 --config Release --parallel
python Tools/make_win_exe.py Builds/vs22/ultraView_artefacts/Release/UltiView.exe dist/UltiView.exe
```

- `ULTRA_MSVC=ON` builds with MSVC instead of the ClangCL toolset.
- JUCE is pinned to develop as of the 1.4.4 release (b7baf331, 2026-09-22); later
  develop changed `MessageBoxOptions::withButton`.
- `patches/lime-msvc-regex.patch`: MSVC's `<regex>` has no `multiline`; the
  shader include search uses an equivalent pattern there.
- `patches/ultra-shared-ultiview.patch`: the stream receivers read one datagram
  without blocking and stop with a bounded wait (see "Stream start"). After
  updating the `Source/ultra-shared` submodule, apply it again.
- `Tools/make_win_exe.py` appends Data.pak to the exe (what `ci/build.sh` does
  with 7z).
