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
- that line and the rows' star icon (CMFM's restart mark) are switched on;
  the other panel titles stay off, as in the game;
- the audio panel's title (the panel CMFM shows) becomes the mod's header:
  switched on and moved above the panel, level with the restart line; the
  panels' title text is twice as wide (600 px);
- two scrollbars, the game's own (the PC graphics settings' pair, a track
  and a bar drawing ui\\common\\scrollbar, docs/research/scrollbar.md): one
  right of the mod list, one right of the settings panel. Adding parts
  grows the file (tools/gui/gui_graft.py, docs/research/gui-format.md);
  CMFM's code shows them only while their list scrolls.

    py -3.12 -B tools/gui/make_mods_gui.py "<extract>/chunk/ui/title/options.gui"

The scrollbar parts come from <extract>/chunk/ui/pc/graphic_option.gui next
to it (only read).
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(__file__))
from gui_check import check  # noqa: E402
from gui_edit import GuiFile, Scene, sha256  # noqa: E402
from gui_graft import GraftError, find_instance, graft  # noqa: E402
from gui_rebuild import VALUE_POOL, Gui, rd  # noqa: E402

SOURCE_SHA256 = "ce319136a650983d22cfc8684f11e0d72c669f4c307ff40710b7dc03d3c8b066"
SCROLLBAR_SOURCE_SHA256 = "a0e4092cd4c3acc628c7a634b391232f360effc5cece194e5d8750df902b2b99"
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
    print("the restart line and the rows' star (the row filler hides it per row); the other panel titles")
    print("stay off (run 20261011-design-2: a title sits on the first row's line, under its value box)")
    on = b.s.param(b.i("a_title_options"), "Visible")
    b.switch_on("a_title_menu_camera", b.s.param(b.i("a_title_menu_camera"), "Visible"), on)
    print("the shown panel's title becomes the mod's header (placed when the file grows)")
    b.switch_on("a_title_menu_audio", b.s.param(b.i("a_title_menu_audio"), "Visible"), on)
    b.switch_on("menu_config/null_star", gui.param("menu_config", "null_star", "Visible"),
                gui.param("menu_config", "coladj_star", "Visible"))


# The scrollbars (design pixels, positions inside their parent; docs/research/scrollbar.md):
# the plain animations (2 track, 1 bar in ui\common\scrollbar) draw from the root's
# top-left, 16 px wide, MaxFixFrame = BarMaxPos = the track's length.
# - the mod list (parent null_prog_color0 at 170, 250; rows 50 + 10 apart, 11
#   shown, the first centred about 4 px below the list's origin): between the
#   cursor bar's end (about 518) and the panel's frame (about 557);
# - the panel (parent null_audio, in null_prog_color1 at 576, 212; rows 40 + 12
#   apart from PosY 33, CMFM shows at most 14): just right of the panel's frame
#   (56 x 14.4 = 806 px wide).
SCROLLBARS = (
    # name, parent, PosX, PosY, track length
    ("cmfm_list_scroll", "null_prog_color0", 359.0, -21.0, 650.0),
    ("cmfm_panel_scroll", "null_audio", 814.0, 20.0, 716.0),
)


def init_param(g, instance, name):
    rec = g.tables["instance"][find_instance(g, instance)]
    for k in range(rd(rec, 40), rd(rec, 40) + rd(rec, 16)):
        if g.string(rd(g.tables["initparam"][k], 16)) == name:
            return g.tables["initparam"][k]
    raise KeyError("%s has no parameter %s" % (instance, name))


def set_param(g, instance, name, expected, new):
    """A grafted instance's own value (the graft gave it a pool entry of its own)."""
    p = init_param(g, instance, name)
    fmt = "<f" if p[0] == 2 else "<i"
    pool, off = g.pools[VALUE_POOL[p[0]]], rd(p, 24, "<Q")
    found = struct.unpack_from(fmt, pool, off)[0]
    if round(found, 4) != expected:
        raise ValueError("%s %s is %r, expected %r" % (instance, name, found, expected))
    struct.pack_into(fmt, pool, off, new)
    print("  %s %s: %r -> %r" % (instance, name, expected, new))


# The mod's header: the audio panel's title, whose text (title_menu/msg_sub_title,
# shared by every panel title and the restart line) is centred on the instance
# (top-centre control point) in a 300 x 50 box, its line in the middle. Centred
# over the panel (null_prog_color1 at 576, 212; the panel 806 px wide from
# about -19) and level with the restart line (its text centred at y 185): top
# at 160, i.e. PosY -52.
HEADER = ("a_title_menu_audio", 403.0, -52.0)
TITLE_TEXT_SIZE = (600.0, 50.0, 0.0, 0.0)

VALUE_FORMAT = {1: "<i", 2: "<f", 4: "<4f", 0xB: "<i", 0xD: "<i"}


def own_value(g, p, label, expected, new):
    """Parameter record p takes a pool entry of its own holding `new` (no other
    parameter that shared the old entry changes)."""
    fmt = VALUE_FORMAT[p[0]]
    pool = VALUE_POOL[p[0]]
    found = struct.unpack_from(fmt, g.pools[pool], rd(p, 24, "<Q"))
    rounded = tuple(round(v, 4) for v in found)
    want = expected if isinstance(expected, tuple) else (expected,)
    if rounded != want:
        raise ValueError("%s is %r, expected %r" % (label, found, expected))
    new = new if isinstance(new, tuple) else (new,)
    struct.pack_into("<Q", p, 24, g.append_pool(pool, struct.pack(fmt, *new)))
    print("  %s: %r -> %r" % (label, expected, new if len(new) > 1 else new[0]))


def object_param(g, name, param):
    """A layout part's (cGUIObj*) parameter; the part's name must be unique."""
    hits = [r for r in g.tables["object"] if g.string(rd(r, 16)) == name]
    if len(hits) != 1:
        raise ValueError("%d parts named %r" % (len(hits), name))
    first, count = rd(hits[0], 32), hits[0][4]
    for k in range(first, first + count):
        if g.string(rd(g.tables["initparam"][k], 16)) == param:
            return g.tables["initparam"][k]
    raise KeyError("%s has no parameter %s" % (name, param))


def place_header(g):
    name, x, y = HEADER
    print("the mod's header: %s above the panel" % name)
    own_value(g, init_param(g, name, "PosX"), name + " PosX", 408.0, x)
    own_value(g, init_param(g, name, "PosY"), name + " PosY", 6.0, y)
    own_value(g, object_param(g, "msg_sub_title", "Size"), "title_menu/msg_sub_title Size", (300.0, 50.0, 0.0, 0.0),
              TITLE_TEXT_SIZE)


def add_scrollbars(data, source):
    """CMFM's layout with the mod's header placed and the two scrollbars
    grafted in (the file grows)."""
    g = Gui.parse(data)
    src = Gui.parse(source)
    place_header(g)
    for name, parent, x, y, length in SCROLLBARS:
        print("scrollbar %s under %s" % (name, parent))
        graft(g, src, "null_scrollbar_root", parent, entries_like="btnlist_camera", log=lambda m: print("  " + m),
              rename={"null_scrollbar_root": name, "scrollbar_auto_base": name + "_base",
                      "scrollbar_auto": name + "_bar"})
        set_param(g, name, "PosX", 610.0, x)
        set_param(g, name, "PosY", 16.0, y)
        for part, animation in (("_base", 2), ("_bar", 1)):  # the plain track and bar
            set_param(g, name + part, "AnimationId", 4 if part == "_base" else 3, animation)
            set_param(g, name + part, "MaxFixFrame", 590.0, length)
            set_param(g, name + part, "BarMaxPos", 590.0, length)
    out = g.build()
    again = Gui.parse(out)
    if again.build() != out:
        raise ValueError("the grown layout does not rebuild identically")
    problems = check(again)
    if problems:
        raise ValueError("the grown layout breaks %d rule(s): %s" % (len(problems), "; ".join(problems[:5])))
    return out


def main():
    if len(sys.argv) != 2:
        print(__doc__)
        return 2
    data = open(sys.argv[1], "rb").read()
    if sha256(data) != SOURCE_SHA256:
        print("source is not the expected options.gui (SHA-256 %s)" % sha256(data))
        return 1
    source_path = os.path.join(os.path.dirname(sys.argv[1]), "..", "pc", "graphic_option.gui")
    source = open(source_path, "rb").read()
    if sha256(source) != SCROLLBAR_SOURCE_SHA256:
        print("%s is not the expected graphic_option.gui (SHA-256 %s)" % (source_path, sha256(source)))
        return 1
    gui = GuiFile(data)
    try:
        build(gui)
        out = add_scrollbars(bytes(gui.d), source)
    except (KeyError, ValueError, TypeError, GraftError) as e:
        print("stopped: %s" % e)
        return 1
    os.makedirs(os.path.dirname(OUTPUT), exist_ok=True)
    with open(OUTPUT, "wb") as f:
        f.write(out)
    print("wrote %s: %d bytes, SHA-256 %s" % (os.path.normpath(OUTPUT), len(out), sha256(out)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
