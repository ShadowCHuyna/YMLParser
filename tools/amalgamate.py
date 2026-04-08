#!/usr/bin/env python3
"""
tools/amalgamate.py — generic single-header amalgamator.

Usage:
    python3 tools/amalgamate.py [--config tools/amalgamate.json]

All project-specific settings live in the JSON config:
    output          — output file path (relative to project root)
    public_header   — public API header (relative to project root)
    guard           — include guard macro (e.g. EVENTBUS_H)
    impl_guard      — implementation guard macro (e.g. EVENTBUS_IMPLEMENTATION)
    private_macro   — macro used to hide internal symbols (e.g. EB_PRIVATE)
    banner          — list of lines for the top-of-file comment
    impl_files      — ordered list of files to inline in the impl block
    strip_includes  — list of #include lines to remove (already inlined)
"""

import argparse
import json
import os
import re


def load_config(config_path: str) -> dict:
    with open(config_path, encoding="utf-8") as f:
        return json.load(f)


def inline_file(path: str, strip_includes: list[str]) -> str:
    """
    Read a file and remove:
      - #pragma once
      - internal #include lines listed in strip_includes
    System #include <...> lines are kept — they are idempotent.
    """
    with open(path, encoding="utf-8") as f:
        content = f.read()

    content = re.sub(r"[ \t]*#pragma once[ \t]*\n", "", content)

    for inc in strip_includes:
        content = re.sub(
            r"[ \t]*" + re.escape(inc) + r"[ \t]*\n",
            "",
            content,
        )

    return content


def separator(rel_path: str) -> str:
    name   = rel_path.replace("\\", "/")
    dashes = "─" * max(0, 60 - len(name))
    return f"/* ── {name} {dashes} */\n"


def generate(root: str, cfg: dict) -> str:
    strip    = cfg["strip_includes"]
    parts    = []

    # ── banner ────────────────────────────────────────────────────────
    parts.append("\n".join(cfg["banner"]) + "\n\n")

    # ── public API ────────────────────────────────────────────────────
    public_path = os.path.join(root, cfg["public_header"])
    public_api  = inline_file(public_path, strip)

    parts.append(f"#ifndef {cfg['guard']}\n#define {cfg['guard']}\n\n")
    parts.append("/* ════════════════════════ PUBLIC API ════════════════════════ */\n\n")
    parts.append(public_api.rstrip("\n") + "\n\n")

    # ── implementation block ──────────────────────────────────────────
    parts.append(f"#ifdef {cfg['impl_guard']}\n")
    parts.append(
        f"/* {cfg['private_macro']} — hides internal symbols. */\n"
        f"#define {cfg['private_macro']} static\n\n"
    )

    for rel in cfg["impl_files"]:
        path    = os.path.join(root, rel)
        content = inline_file(path, strip)
        parts.append(separator(rel))
        parts.append(content.strip("\n") + "\n\n")

    parts.append(f"#endif /* {cfg['impl_guard']} */\n\n")
    parts.append(f"#endif /* {cfg['guard']} */\n")

    return "".join(parts)


def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

    parser = argparse.ArgumentParser(description="Single-header amalgamator")
    parser.add_argument(
        "--config",
        default=os.path.join(root, "tools", "amalgamate.json"),
        help="Path to amalgamate.json (default: tools/amalgamate.json)",
    )
    args = parser.parse_args()

    cfg    = load_config(args.config)
    result = generate(root, cfg)

    out_path = os.path.join(root, cfg["output"])
    with open(out_path, "w", encoding="utf-8") as f:
        f.write(result)

    lines = result.count("\n")
    print(f"Wrote {os.path.relpath(out_path, root)}  ({lines} lines)")


if __name__ == "__main__":
    main()
