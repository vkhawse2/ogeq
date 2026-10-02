#!/usr/bin/env python3
"""
rename_app.py -- Rename the app with one command.

Usage:
    python3 rename_app.py <NewName>

Example:
    python3 rename_app.py ClearSound

This updates:
  - CMakeLists.txt project name and APP_NAME define
  - All source file contents (case variants: UPPER, Capitalized, lower)
  - Filenames containing the app name
  - WiX installer files
  - Resource files

The script derives three case variants from the new name:
  UPPER       -> e.g. OGEQ
  Capitalized -> e.g. Ogeq
  lower       -> e.g. ogeq

It replaces the corresponding variants of the CURRENT name.
The current name is read from APP_NAME in the root CMakeLists.txt.
"""

import os
import re
import sys

ROOT = os.path.dirname(os.path.abspath(__file__))

# Files/dirs to skip (build output, VCS, etc.)
SKIP_DIRS = {".git", "build", "out", "__pycache__", ".vs"}
SKIP_EXTS = {".exe", ".dll", ".obj", ".pdb", ".lib", ".msi", ".zip"}


def get_current_name():
    cmake_path = os.path.join(ROOT, "CMakeLists.txt")
    with open(cmake_path, "r", encoding="utf-8") as f:
        content = f.read()
    m = re.search(r'set\s*\(\s*APP_NAME\s+"([^"]+)"\s*\)', content)
    if not m:
        print("ERROR: Could not find set(APP_NAME ...) in root CMakeLists.txt")
        sys.exit(1)
    return m.group(1)


def variants(name):
    return {
        "upper": name.upper(),
        "title": name[0].upper() + name[1:].lower() if name else name,
        "cap": name[0].upper() + name[1:] if name else name,
        "lower": name.lower(),
    }


def replace_in_file(path, old_vars, new_vars):
    _, ext = os.path.splitext(path)
    if ext.lower() in SKIP_EXTS:
        return False
    try:
        with open(path, "r", encoding="utf-8") as f:
            content = f.read()
    except (UnicodeDecodeError, OSError):
        return False  # binary or unreadable; skip

    new_content = content
    # Replace longest first to avoid partial overlaps
    for key in ("upper", "title", "cap", "lower"):
        old = old_vars[key]
        new = new_vars[key]
        if old != new:
            new_content = new_content.replace(old, new)

    if new_content != content:
        with open(path, "w", encoding="utf-8") as f:
            f.write(new_content)
        return True
    return False


def main():
    if len(sys.argv) != 2:
        print(f"Usage: python3 {sys.argv[0]} <NewName>")
        print(f"Example: python3 {sys.argv[0]} ClearSound")
        sys.exit(1)

    new_name = sys.argv[1].strip()
    if not re.match(r"^[A-Za-z][A-Za-z0-9_]*$", new_name):
        print("ERROR: Name must start with a letter and contain only letters, digits, underscores.")
        sys.exit(1)

    old_name = get_current_name()
    if old_name == new_name:
        print(f"App is already named '{new_name}'. Nothing to do.")
        return

    old_vars = variants(old_name)
    new_vars = variants(new_name)

    print(f"Renaming '{old_name}' -> '{new_name}'")
    print(f"  UPPER: {old_vars['upper']} -> {new_vars['upper']}")
    print(f"  Title: {old_vars['title']} -> {new_vars['title']}")
    print(f"  Cap:   {old_vars['cap']} -> {new_vars['cap']}")
    print(f"  lower: {old_vars['lower']} -> {new_vars['lower']}")

    # Phase 1: file contents
    changed_files = []
    for dirpath, dirnames, filenames in os.walk(ROOT):
        dirnames[:] = [d for d in dirnames if d not in SKIP_DIRS]
        for fn in filenames:
            fp = os.path.join(dirpath, fn)
            if replace_in_file(fp, old_vars, new_vars):
                changed_files.append(os.path.relpath(fp, ROOT))

    # Phase 2: filenames (deepest first so renames don't clash)
    renamed = []
    all_paths = []
    for dirpath, dirnames, filenames in os.walk(ROOT):
        dirnames[:] = [d for d in dirnames if d not in SKIP_DIRS]
        for fn in filenames:
            all_paths.append(os.path.join(dirpath, fn))
    # Sort by depth descending
    all_paths.sort(key=lambda p: p.count(os.sep), reverse=True)
    for fp in all_paths:
        dirname, basename = os.path.split(fp)
        new_basename = basename
        for key in ("upper", "title", "cap", "lower"):
            if old_vars[key] != new_vars[key]:
                new_basename = new_basename.replace(old_vars[key], new_vars[key])
        if new_basename != basename:
            new_fp = os.path.join(dirname, new_basename)
            os.rename(fp, new_fp)
            renamed.append(f"{os.path.relpath(fp, ROOT)} -> {os.path.relpath(new_fp, ROOT)}")

    print(f"\nDone. {len(changed_files)} files updated, {len(renamed)} files renamed.")
    if changed_files:
        print("\nUpdated files:")
        for f in sorted(changed_files)[:20]:
            print(f"  {f}")
        if len(changed_files) > 20:
            print(f"  ... and {len(changed_files) - 20} more")
    if renamed:
        print("\nRenamed files:")
        for r in renamed:
            print(f"  {r}")
    print(f"\nNext: rebuild. The binary, installer, and registry keys now use '{new_name}'.")


if __name__ == "__main__":
    main()
