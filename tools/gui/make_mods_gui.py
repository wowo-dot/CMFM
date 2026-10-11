"""Build CMFM's page layout, assets/nativePC/ui/cmfm/mods.gui, from the game's
title Options layout (ui\\title\\options.gui, exe 421810). The game file is
only read (decisions.md item 10); every edit checks the value it replaces,
and the file size never changes.

Layout for CMFM's page (docs/research/native-menu.md, "Layout study"):
- the mod list on the left is the layout's spare `camera` list, moved under
  the category list's parent and styled like the category list (row
  template, cursor, spacing), 11 rows;
- the game's category list stays (the page's code needs it) but moves off
  screen; CMFM keeps its cursor on the audio panel, which shows the
  highlighted mod's settings;
- the camera panel's title text moves above the mod list: CMFM's line for
  changes that wait for a game restart (empty otherwise);
- the split panel's title, that line and the rows' star icon (CMFM's
  restart mark) are switched on.

    py -3.12 -B tools/gui/make_mods_gui.py "<extract>/chunk/ui/title/options.gui"
"""
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
from gui_edit import GuiFile, Scene, sha256  # noqa: E402

SOURCE_SHA256 = "ce319136a650983d22cfc8684f11e0d72c669f4c307ff40710b7dc03d3c8b066"
OUTPUT = os.path.join(os.path.dirname(__file__), "..", "..", "assets", "nativePC", "ui", "cmfm", "mods.gui")
MOD_LIST_ROWS = 11


class Build:
    def __init__(self, gui):
        self.g = gui
        self.s = Scene(gui)

    def i(self, name):
        return self.s.find(name)

    def value(self, name, param):
        v = self.g.value(self.s.param(self.i(name), param))
        return v[0] if len(v) == 1 else v

    def expect(self, name, param, expected):
        found = self.value(name, param)
        rounded = tuple(round(v, 4) for v in found) if isinstance(found, tuple) else round(found, 4)
        if rounded != expected:
            raise ValueError("%s %s is %r, expected %r" % (name, param, found, expected))

    def like(self, name, param, expected, source):
        """name.param (expected) takes source.param's value."""
        self.expect(name, param, expected)
        self.s.point_at(self.s.param(self.i(name), param), self.s.param(self.i(source), param))
        print("  %s %s: %r -> %r (as %s)" % (name, param, expected, self.value(name, param), source))

    def like_param(self, name, param, expected, source, source_param):
        self.expect(name, param, expected)
        self.s.point_at(self.s.param(self.i(name), param), self.s.param(self.i(source), source_param))
        print("  %s %s: %r -> %r (as %s %s)" % (name, param, expected, self.value(name, param), source, source_param))

    def number(self, name, param, expected, new):
        self.expect(name, param, expected)
        self.s.point_at_value(self.s.param(self.i(name), param), new)
        print("  %s %s: %r -> %r" % (name, param, expected, self.value(name, param)))

    def switch_on(self, label, k, source):
        """Parameter k, a Visible flag (type 3, one byte in the pool at slot
        0x178) that is 0, takes `source`'s value, a Visible flag that is 1."""
        for p, want in ((k, 0), (source, 1)):
            kind, slot = self.g.param_record(p)
            if kind != 3 or self.g.d[self.g.off[0x178] + slot] != want:
                raise ValueError("%s: Visible flag %d is not type 3 with value %d" % (label, p, want))
        self.s.point_at(k, source)
        print("  %s Visible: 0 -> 1" % label)

    def links(self, name, next_name, child_name):
        want = (self.i(next_name) if next_name else -1, self.i(child_name) if child_name else -1)
        if self.s.links(self.i(name)) != want:
            raise ValueError("%s links are %r, expected %r" % (name, self.s.links(self.i(name)), want))

    def relink(self, name, next_name, child_name):
        self.s.set_links(self.i(name), self.i(next_name) if next_name else -1,
                         self.i(child_name) if child_name else -1)


def build(gui):
    b = Build(gui)
    before = len(b.s.walk())
    # The tree as the game ships it.
    b.links("null_prog_color0", "null_prog_color1", "btnlist_menu")
    b.links("btnlist_menu", None, "cursor_menu")
    b.links("a_line_camera", "btnlist_camera", None)
    b.links("btnlist_camera", "num_input_camera", "auto_menu_config_camera")
    print("tree: the camera list moves under the category list's parent")
    b.relink("a_line_camera", "num_input_camera", None)
    b.relink("null_prog_color0", "null_prog_color1", "btnlist_camera")
    b.relink("btnlist_camera", "btnlist_menu", "auto_menu_config_camera")
    if len(b.s.walk()) != before:
        raise ValueError("the tree lost or gained instances")
    print("the game's category list goes off screen (the page still uses it)")
    b.like_param("btnlist_menu", "PosX", 0.0, "null_all", "PosX")
    print("the mod list takes the category list's place and look")
    b.like("btnlist_camera", "PosY", 33.0, "btnlist_menu")
    b.number("btnlist_camera", "DuplicateNum", 13, MOD_LIST_ROWS)
    b.like("btnlist_camera", "DuplicateSpace", 12.0, "btnlist_menu")
    b.like("btnlist_camera", "DuplicateButtonSize", 40.0, "btnlist_menu")
    b.like("btnlist_camera", "EffectOfs", (-20.0, 0.0, 0.0, 0.0), "btnlist_menu")
    b.like("auto_menu_config_camera", "ResourceId", -1, "auto_menu_title")
    b.like("auto_menu_config_camera", "AnimationId", 1, "auto_menu_title")
    b.like("auto_menu_config_camera", "PosX", 0.0, "auto_menu_title")
    b.like("cursor_camera", "SclX", 81.0, "cursor_menu")
    b.like("cursor_camera", "SclY", 5.2, "cursor_menu")
    print("the camera panel's number control no longer writes into the row template (its object 2 is no")
    print("longer a text in the category row style; run 20261011-split-1 crashed in formatText at setup)")
    b.number("num_input_camera", "MsgInstID", 38, -1)
    print("the camera panel's title (a text of its own, 300 x 50, centred) becomes the restart line,")
    print("under the header above the mod list")
    b.links("a_window_camera", "a_title_menu_camera", None)
    b.links("a_title_menu_camera", "a_line_camera", None)
    b.relink("a_window_camera", "a_line_camera", None)
    b.relink("a_title_menu_camera", None, None)
    b.relink("btnlist_menu", "a_title_menu_camera", "cursor_menu")
    if len(b.s.walk()) != before:
        raise ValueError("the tree lost or gained instances")
    b.number("a_title_menu_camera", "PosX", 408.0, 200.0)
    b.number("a_title_menu_camera", "PosY", 6.0, -90.0)
    print("switched on (the game ships them with Visible 0; run 20261011-design-1b showed none of them):")
    print("the split panel's title, the restart line, and the rows' star (the row filler hides it per row)")
    on = b.s.param(b.i("a_title_options"), "Visible")
    b.switch_on("a_title_menu_audio", b.s.param(b.i("a_title_menu_audio"), "Visible"), on)
    b.switch_on("a_title_menu_camera", b.s.param(b.i("a_title_menu_camera"), "Visible"), on)
    b.switch_on("menu_config/null_star", gui.param("menu_config", "null_star", "Visible"),
                gui.param("menu_config", "coladj_star", "Visible"))


def main():
    if len(sys.argv) != 2:
        print(__doc__)
        return 2
    data = open(sys.argv[1], "rb").read()
    if sha256(data) != SOURCE_SHA256:
        print("source is not the expected options.gui (SHA-256 %s)" % sha256(data))
        return 1
    gui = GuiFile(data)
    try:
        build(gui)
    except (KeyError, ValueError, TypeError) as e:
        print("stopped: %s" % e)
        return 1
    out = bytes(gui.d)
    os.makedirs(os.path.dirname(OUTPUT), exist_ok=True)
    with open(OUTPUT, "wb") as f:
        f.write(out)
    print("wrote %s: %d bytes, SHA-256 %s" % (os.path.normpath(OUTPUT), len(out), sha256(out)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
