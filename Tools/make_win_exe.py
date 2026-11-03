"""Windows release exe without 7-Zip: the built ultraView.exe with Data.pak appended.

Usage: python Tools/make_win_exe.py BUILT_EXE OUT_EXE

Does what build_pak in ci/build.sh does with 7z: the ultra-shared Data tree, then
the app Data tree, into one zip (UTF-8 names; png/jpg stored, the rest deflated at
the weakest level), skipping "!src" folders, dot files and Thumbs.db; checks that
every file made it in; appends the zip to the exe (zip is end-anchored, so the
file stays a valid PE and a valid zip).
"""
import io
import os
import shutil
import sys
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TREES = [os.path.join(ROOT, "Source", "ultra-shared", "Data"), os.path.join(ROOT, "Data")]


def files(tree):
    for top, dirs, names in os.walk(tree):
        dirs[:] = sorted(d for d in dirs if d != "!src" and not d.startswith("."))
        for n in sorted(names):
            if n.startswith(".") or n == "Thumbs.db":
                continue
            path = os.path.join(top, n)
            yield os.path.relpath(path, tree).replace(os.sep, "/"), path


def main():
    built, out = sys.argv[1], sys.argv[2]
    buf = io.BytesIO()
    expected = 0
    with zipfile.ZipFile(buf, "w") as z:
        for tree in TREES:
            for name, path in files(tree):
                expected += 1
                image = name.lower().endswith((".png", ".jpg"))
                z.write(path, name, compress_type=zipfile.ZIP_STORED if image else zipfile.ZIP_DEFLATED,
                        compresslevel=None if image else 1)
    with zipfile.ZipFile(io.BytesIO(buf.getvalue())) as z:
        actual = len(z.namelist())
    if actual != expected:
        sys.exit(f"Data.pak entry count mismatch: {actual} packed, {expected} on disk")
    shutil.copyfile(built, out)
    with open(out, "ab") as f:
        f.write(buf.getvalue())
    print(f"{out}: exe {os.path.getsize(built):,} bytes + Data.pak {len(buf.getvalue()):,} bytes ({actual} files)")


if __name__ == "__main__":
    main()
