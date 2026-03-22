#!/usr/bin/env python3
"""gen-aine-res.py -- Generate aine-res.txt companion files for AINE from an APK.

Usage:
    python3 gen-aine-res.py <app-debug.apk> [--res-dir <path/to/source/res>]

Outputs:
    aine-res.txt in the current directory (or --out-dir if specified)
    Copies source XML layouts if --res-dir is given.

The aine-res.txt format is:
    layout:<hex_id>=<relative_xml_path>
    id:<name>=<hex_id>
    string:<name>=<text_value>

AINE reads this file (alongside the DEX) to:
  - Map setContentView(int) -> layout XML path
  - Map findViewById(int)   -> view node by name
  - Map @string/xxx         -> literal text for XML inflation
"""

import sys
import os
import re
import subprocess
import shutil
import argparse

AAPT2 = os.path.expanduser(
    "~/Library/Android/sdk/build-tools/35.0.0/aapt2"
)

def aapt2_dump_resources(apk_path):
    r = subprocess.run([AAPT2, "dump", "resources", apk_path],
                       capture_output=True, text=True, encoding="utf-8",
                       errors="replace")
    return r.stdout

def aapt2_dump_strings(apk_path):
    r = subprocess.run([AAPT2, "dump", "strings", apk_path],
                       capture_output=True, text=True, encoding="utf-8",
                       errors="replace")
    return r.stdout

def parse_resources(apk_path):
    """Return (layouts, ids) where:
        layouts = {0x7f040000: 'res/layout/activity_main.xml'}
        ids     = {'textDisplay': 0x7f030014, ...}
    """
    text = aapt2_dump_resources(apk_path)
    layouts = {}
    ids = {}
    for line in text.splitlines():
        line = line.strip()
        # layout entries look like:
        #   resource 0x7f040000 layout/activity_main
        #     () (file) res/layout/activity_main.xml type=XML
        m = re.match(r"resource (0x[0-9a-fA-F]+) layout/(\S+)", line)
        if m:
            _last_layout_id = int(m.group(1), 16)
            _last_layout_name = m.group(2)
        m2 = re.match(r"\(\) \(file\) (res/layout/\S+\.xml)", line)
        if m2 and _last_layout_id is not None:
            layouts[_last_layout_id] = m2.group(1)
            _last_layout_id = None
            continue
        # id entries: resource 0x7f030014 id/textDisplay
        m3 = re.match(r"resource (0x[0-9a-fA-F]+) id/(\S+)", line)
        if m3:
            ids[m3.group(2)] = int(m3.group(1), 16)

    return layouts, ids

def parse_strings_xml(strings_xml_path):
    """Parse source strings.xml and return {name: value}."""
    strings = {}
    if not os.path.exists(strings_xml_path):
        return strings
    with open(strings_xml_path, "r", encoding="utf-8") as f:
        content = f.read()
    for m in re.finditer(r'<string\s+name="([^"]+)">([^<]*)</string>', content):
        strings[m.group(1)] = m.group(2)
    return strings

def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("apk", help="Path to app-debug.apk")
    parser.add_argument("--res-dir", help="Path to source res/ directory (for XML layouts + strings.xml)")
    parser.add_argument("--out-dir", default=".", help="Output directory (default: .)")
    parser.add_argument("--layout-name", default="activity_main",
                        help="Name of the main layout (default: activity_main)")
    args = parser.parse_args()

    apk = os.path.abspath(args.apk)
    out_dir = os.path.abspath(args.out_dir)
    os.makedirs(out_dir, exist_ok=True)

    print(f"[gen-aine-res] Processing {apk}")

    # Parse resources from APK
    layouts, ids = parse_resources(apk)
    print(f"  layouts: {len(layouts)}, ids: {len(ids)}")

    # Find activity_main layout ID
    main_layout_id = None
    main_layout_path = None
    for lid, lpath in layouts.items():
        if args.layout_name in lpath:
            main_layout_id = lid
            main_layout_path = lpath
            break

    if main_layout_id is None:
        print(f"  WARNING: layout/{args.layout_name} not found in APK", file=sys.stderr)

    # Parse strings from source XML if available
    strings = {}
    if args.res_dir:
        strings_path = os.path.join(args.res_dir, "values", "strings.xml")
        strings = parse_strings_xml(strings_path)
        print(f"  strings: {len(strings)}")

        # Copy layout XML to output directory
        src_layout = os.path.join(args.res_dir, "layout", f"{args.layout_name}.xml")
        if os.path.exists(src_layout):
            dst_layout_dir = os.path.join(out_dir, "res", "layout")
            os.makedirs(dst_layout_dir, exist_ok=True)
            shutil.copy2(src_layout, dst_layout_dir)
            print(f"  copied layout XML -> {dst_layout_dir}")
        else:
            print(f"  WARNING: source layout not found at {src_layout}", file=sys.stderr)

    # Write aine-res.txt
    out_path = os.path.join(out_dir, "aine-res.txt")
    with open(out_path, "w", encoding="utf-8") as f:
        f.write("# AINE resource companion file (auto-generated by gen-aine-res.py)\n")
        f.write("# layout:<hex_id>=<relative_xml_path>\n")
        f.write("# id:<name>=<hex_id>\n")
        f.write("# string:<name>=<value>\n\n")

        # Layout
        if main_layout_id is not None:
            # Use the copy in out_dir/res/layout/
            local_xml = f"res/layout/{args.layout_name}.xml"
            f.write(f"layout:0x{main_layout_id:08x}={local_xml}\n")

        f.write("\n")

        # IDs (sorted by name for readability)
        for name in sorted(ids.keys()):
            f.write(f"id:{name}=0x{ids[name]:08x}\n")

        f.write("\n")

        # Strings
        for name in sorted(strings.keys()):
            val = strings[name]
            # Escape newlines in values
            val = val.replace("\n", "\\n")
            f.write(f"string:{name}={val}\n")

    print(f"  wrote {out_path}")

if __name__ == "__main__":
    # Track last layout ID across lines
    _last_layout_id = None
    _last_layout_name = None
    main()
