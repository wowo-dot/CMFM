"""Build CMFM's page layout, assets/nativePC/ui/cmfm/mods.gui, from the game's
title Options layout (ui\\title\\options.gui, exe 421810) by applying the
edits below. Each edit names the element and parameter, the value the game's
file must hold and the elements that share that value; anything else stops
the build. The game file is only read (decisions.md item 10).

    py -3.12 -B tools/gui/make_mods_gui.py "<extract>/chunk/ui/title/options.gui"
"""
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
from gui_edit import GuiFile, sha256  # noqa: E402

SOURCE_SHA256 = "ce319136a650983d22cfc8684f11e0d72c669f4c307ff40710b7dc03d3c8b066"
OUTPUT = os.path.join(os.path.dirname(__file__), "..", "..", "assets", "nativePC", "ui", "cmfm", "mods.gui")

# (animation, instance, parameter, expected, new, elements sharing the value, why)
EDITS = [
    ("title_options", "tex_base_r", "VertexColorLeft", (32, 32, 32, 0), (180, 30, 30, 0),
     {"tex_base_r", "tex_base_l"}, "loading probe: the header band turns red"),
    ("title_options", "tex_base_r", "VertexColorRight", (32, 32, 32, 255), (180, 30, 30, 255),
     {"tex_base_r", "tex_base_l"}, "loading probe: the header band turns red"),
]


def main():
    if len(sys.argv) != 2:
        print(__doc__)
        return 2
    data = open(sys.argv[1], "rb").read()
    if sha256(data) != SOURCE_SHA256:
        print("source is not the expected options.gui (SHA-256 %s)" % sha256(data))
        return 1
    gui = GuiFile(data)
    for animation, instance, name, expected, new, sharers, why in EDITS:
        k = gui.param(animation, instance, name)
        found = tuple(round(v, 3) for v in gui.value(k))
        if found != tuple(float(v) for v in expected):
            print("%s/%s %s is %s, expected %s" % (animation, instance, name, found, expected))
            return 1
        if gui.sharers(k) != sharers:
            print("%s/%s %s is shared by %s, expected %s" % (animation, instance, name, sorted(gui.sharers(k)),
                                                             sorted(sharers)))
            return 1
        gui.set_value(k, new)
        print("%s/%s %s: %s -> %s (%s)" % (animation, instance, name, expected, new, why))
    os.makedirs(os.path.dirname(OUTPUT), exist_ok=True)
    out = bytes(gui.d)
    with open(OUTPUT, "wb") as f:
        f.write(out)
    print("wrote %s: %d bytes, SHA-256 %s" % (os.path.normpath(OUTPUT), len(out), sha256(out)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
