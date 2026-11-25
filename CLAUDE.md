# UltiView: ultraView with a remote C64 keyboard

UltiView: a renamed fork of ultraView 1.4.4 (GPL-3, upstream https://github.com/Toddler-Boy/ultraView)
that also sends keys to the C64 Ultimate while it streams. What the keyboard does and how to
build it on this PC: `README-C64-KEYBOARD.md`.

## Repository

- Remote `origin`: public https://github.com/RunStopRestored/UltiView (renamed from ultraView); `upstream`: Toddler-Boy.
- Work on branch `c64-keyboard` (GitHub's default). `main` stays the untouched 1.4.4.
- The git identity is set in this repo only (RunStopRestored); there is no global one.
- Commit only when asked; after every commit, `git push` and check it worked (the GitHub
  repo is the backup).
- `modules/lime` shows as modified: `patches/lime-msvc-regex.patch` is applied there for
  the MSVC build. Expected; do not commit or reset it.
- `Source/ultra-shared` shows as modified: `patches/ultra-shared-ultiview.patch` is applied
  there (the stream-receiver hang fix). Same rule. A change to that submodule's code goes
  into the patch file (`git -C Source/ultra-shared diff > patches/ultra-shared-ultiview.patch`).

## Build (Windows, MSVC, no clang-cl, no 7-Zip)

Steps in `README-C64-KEYBOARD.md`: `ULTRA_MSVC=ON`, JUCE pinned to b7baf331, the lime and
ultra-shared patches, then `Tools/make_win_exe.py` makes `dist/UltiView.exe`. A new source
file needs the CMake configure step again (the source glob runs at configure time). After a
PRODUCT_NAME or COMPANY_NAME change, delete
`Builds/vs22/ultraView_artefacts/JuceLibraryCode/ultraView_resources.rc`, or the exe keeps
the old names in its version info (only a VERSION change evicts it).

The program is UltiView (PRODUCT_NAME, exe, title, settings folder %APPDATA%\UltiView,
copied once from %APPDATA%\ultraView); code names (GUI_ultraView, the CMake target,
ultraView_artefacts) stay as upstream has them, so upstream merges stay simple.

## The keyboard

`Source/Remote/C64Keyboard.cpp` presses keys in the C64 keyboard matrix with
`POST /v1/machine:input` (JSON `tap` events) when `GET /v1/machine:input` answers 200
(C64U firmware 1.2.1 does). That JSON POST leaves no /Temp file (checked). On 404 it falls
back to the KERNAL keyboard buffer ($0277, count $C6) with
`PUT /v1/machine:writemem?address=..&data=..`. Keep that one a PUT with the bytes in the
URL: a writemem POST with a body leaves a file in the C64U's /Temp, and after about 330 of
them every POST write fails. Ctrl+K switches the keyboard between the C64 and UltiView.

## Shared C64U quirks

`C:\Users\palad\Documents\Claude\Quirks` (private GitHub RunStopRestored/quirks) is the shared notebook of
C64U / Ultimate hardware and firmware quirks from all the owner's projects. When the C64U misbehaves (REST,
FTP, stream, keyboard input), read its `INDEX.md` first; add new findings there (its `README.md` has the
rules), not only here. From this project: Q-057 (Ultimate menu and network keys, `machine:menu_button`),
Q-058 (send the REST `:` unescaped). It also holds the facts above: `machine:input` (Q-020), the ~330
writemem POSTs that fill /Temp (Q-023), the ping before a stream start (Q-027, Q-017), the stream's audio
rate (Q-035), FTP MLSD (Q-025).

Related project: Ultimidi (../Ultimidi, beside this repo), the C64U MIDI player this
keyboard is used with; it reads keys from the keyboard buffer.

## Releases and updates

The updater reads this fork's `docs/api/latest-windows.json` (raw.githubusercontent,
branch c64-keyboard), never upstream's. Fork versions start at 1.5.0 (`VERSION`). Release
steps are in `README-C64-KEYBOARD.md` ("Updates"); Actions is on (GitHub Pages needs it); upstream's
`.github/workflows/build.yaml` is deleted here (it would run on release tags). An upstream
merge that brings it back: delete it again. The web page is GitHub Pages from
`docs/` (https://runstoprestored.github.io/UltiView/).
