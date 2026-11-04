# ultraView with a remote C64 keyboard

Private fork of ultraView 1.4.4 (GPL-3, upstream https://github.com/Toddler-Boy/ultraView)
that also sends keys to the C64 Ultimate while it streams. What the keyboard does and how to
build it on this PC: `README-C64-KEYBOARD.md`.

## Repository

- Remote `origin`: private https://github.com/RunStopRestored/ultraView; `upstream`: Toddler-Boy.
- Work on branch `c64-keyboard` (GitHub's default). `main` stays the untouched 1.4.4.
- The git identity is set in this repo only (RunStopRestored); there is no global one.
- Commit only when asked; after every commit, `git push` and check it worked (the GitHub
  repo is the backup).
- `modules/lime` shows as modified: `patches/lime-msvc-regex.patch` is applied there for
  the MSVC build. Expected; do not commit or reset it.

## Build (Windows, MSVC, no clang-cl, no 7-Zip)

Steps in `README-C64-KEYBOARD.md`: `ULTRA_MSVC=ON`, JUCE pinned to b7baf331, the lime
patch, then `Tools/make_win_exe.py` makes `dist/ultraView.exe`.

## The keyboard

`Source/Remote/C64Keyboard.cpp` writes into the KERNAL keyboard buffer ($0277, count $C6)
with `PUT /v1/machine:writemem?address=..&data=..`. Keep it a PUT with the bytes in the
URL: a POST with a body leaves a file in the C64U's /Temp, and after about 330 of them every
POST write fails. Ctrl+K switches the keyboard between the C64 and ultraView.

Related project: Ultimidi (C:\Users\palad\Documents\Claude\Ultimidi), the C64U MIDI player
this keyboard is used with; it reads keys from the keyboard buffer.
