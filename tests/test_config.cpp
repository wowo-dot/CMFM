// Unit tests for cmfm::config: JSON with positions, INI, surgical edits.
#include "config/config_pages.hpp"
#include "config/description.hpp"
#include "config/document.hpp"
#include "config/json.hpp"
#include "menu/menu.hpp"

#include <cstdio>
#include <cstring>
#include <map>
#include <string>

using namespace cmfm::config;

namespace {

int g_failures = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++g_failures;
    }
}

Path path(const char* text) {
    Path p;
    parse_path(text, p);
    return p;
}

// `text` with `from` replaced by `to` once (the expected result of an edit).
std::string replaced(std::string text, const std::string& from, const std::string& to) {
    const std::size_t at = text.find(from);
    if (at != std::string::npos) {
        text.replace(at, from.size(), to);
    }
    return text;
}

// A config shaped like Fexty's Mod's (tabs, nested objects, keys as strings).
const std::string kFexty =
    "{\n"
    "\t\"Defaults\": {\n"
    "\t\t\"RegularModeDefaultOn\": false,\n"
    "\t\t\"HeroicsModeDefaultOn\": true,\n"
    "\t\t\"CatHelpersDefault\": \"gajalaka\"\n"
    "\t},\n"
    "\t\"Hotkeys\": {\n"
    "\t\t\"HeroicsModeHotkey\": \"62\"\n"
    "\t},\n"
    "\t\"Cooldowns\": {\n"
    "\t\t\"HeroicsModeCooldown\": 0\n"
    "\t}\n"
    "}";

void reads_json_with_positions() {
    JsonValue v;
    TextError e;
    check(parse_json(kFexty, v, e) && v.kind == JsonValue::Kind::Object, "parses");
    std::string why;
    const JsonValue* on = find(v, path("Defaults.HeroicsModeDefaultOn"), why);
    check(on != nullptr && on->kind == JsonValue::Kind::Bool && on->boolean &&
              kFexty.substr(on->begin, on->end - on->begin) == "true",
          "a value knows where it stands");
    const JsonValue* key = find(v, path("Hotkeys.HeroicsModeHotkey"), why);
    check(key != nullptr && key->text == "62" && kFexty.substr(key->begin, key->end - key->begin) == "\"62\"",
          "a string's place includes its quotes");
    check(find(v, path("Defaults.Missing"), why) == nullptr && why.find("not in the file") != std::string::npos,
          "a missing key says so");
    check(find(v, path("Defaults.HeroicsModeDefaultOn.x"), why) == nullptr, "below a plain value: nothing");

    const std::string commented = "{ // a comment\n \"a\": 1, /* two\n lines */ \"b\": [1, 2,],\n}";
    check(parse_json(commented, v, e) && v.members.size() == 2 && v.members[1].second.items.size() == 2,
          "comments and trailing commas");
    const std::string twice = "{\"a\": 1, \"a\": 2}";
    check(parse_json(twice, v, e) && find(v, path("a"), why) == nullptr && why.find("twice") != std::string::npos,
          "a key that appears twice is not guessed");
    check(parse_json("{\"s\": \"caf\\u00e9 \\ud83d\\ude00\"}", v, e) && v.members[0].second.text == "caf\xC3\xA9 \xF0\x9F\x98\x80",
          "\\u escapes and surrogate pairs");
    check(!parse_json("{\n  \"a\": tru\n}", v, e) && e.line == 2, "an error knows its line");
    check(!parse_json("{\"a\": 1} x", v, e) && !parse_json("{\"a\": \"no end}", v, e) && !parse_json("/* open", v, e),
          "trailing text, an open string, an open comment");
    std::string deep(100, '[');
    check(!parse_json(deep, v, e) && e.what.find("deeply") != std::string::npos, "nesting has a limit");
}

void parses_paths() {
    Path p;
    check(parse_path("a.b[0].c", p) && p.size() == 4 && p[2].is_index && p[2].index == 0 && p[3].key == "c",
          "keys and an index");
    check(path_text(p) == "a.b[0].c", "back to text");
    check(!parse_path("a..b", p) && !parse_path("a[", p) && !parse_path("a[x]", p) && !parse_path("", p) &&
              !parse_path("a.", p),
          "malformed paths refused");
}

void edits_json_surgically() {
    std::string out;
    std::string why;
    check(apply_edits(Syntax::Json, kFexty,
                      {{path("Defaults.HeroicsModeDefaultOn"), "false"}, {path("Hotkeys.HeroicsModeHotkey"), "64"}},
                      out, why),
          "two edits");
    std::string want = replaced(kFexty, "\"HeroicsModeDefaultOn\": true", "\"HeroicsModeDefaultOn\": false");
    want = replaced(want, "\"62\"", "\"64\"");
    check(out == want, "only the values change; tabs, order and quotes stay");
    check(apply_edits(Syntax::Json, kFexty, {{path("Cooldowns.HeroicsModeCooldown"), "30"}}, out, why) &&
              out == replaced(kFexty, "\"HeroicsModeCooldown\": 0", "\"HeroicsModeCooldown\": 30"),
          "a bare number stays bare");
    check(apply_edits(Syntax::Json, kFexty, {{path("Defaults.CatHelpersDefault"), "say \"hi\""}}, out, why) &&
              out.find("\"say \\\"hi\\\"\"") != std::string::npos,
          "a string is escaped");
    check(!apply_edits(Syntax::Json, kFexty, {{path("Cooldowns.HeroicsModeCooldown"), "ten"}}, out, why) &&
              why.find("not a number") != std::string::npos,
          "a number token takes only a number");
    check(!apply_edits(Syntax::Json, kFexty, {{path("Defaults.HeroicsModeDefaultOn"), "maybe"}}, out, why),
          "a JSON literal takes only true or false");
    check(!apply_edits(Syntax::Json, kFexty, {{path("Defaults"), "x"}}, out, why) &&
              why.find("not a plain value") != std::string::npos,
          "an object is not edited");
    check(!apply_edits(Syntax::Json, "{\"a\": null}", {{path("a"), "1"}}, out, why), "null: the type is unknown");
    check(!apply_edits(Syntax::Json, "{\"a\": ", {{path("a"), "1"}}, out, why) &&
              why.find("does not parse") != std::string::npos,
          "a file that does not parse is not written");

    const std::string foodskill = "{\n  \"healthType\": \"5\",\n\n  \"skills\": [\"A\", \"B\"], // note\n  \"logToConsole\": false\n}";
    check(apply_edits(Syntax::Json, foodskill, {{path("healthType"), "6"}, {path("skills[1]"), "C"}}, out, why) &&
              out == replaced(replaced(foodskill, "\"5\"", "\"6\""), "\"B\"", "\"C\""),
          "a number kept in quotes stays in quotes; a list item; a comment stays");
}

const std::string kQuest = "[Post]\nQuestId=66844\nPlayers=2\nPassEnabled=1\nPasscode=8008\n\n[Debug]\nVerbose=1";

void reads_ini() {
    Document d;
    TextError e;
    check(d.parse(Syntax::Ini, kQuest, e), "parses");
    Scalar s;
    std::string why;
    check(d.lookup(path("Post.Players"), s, why) && s.value == "2" && s.quote == 0, "Section.Key");
    check(d.lookup(path("post.PLAYERS"), s, why) && s.value == "2", "without regard to case");
    check(!d.lookup(path("Post.Nothing"), s, why) && why.find("not in the file") != std::string::npos, "missing");
    check(!d.lookup(path("Post.Players.x"), s, why), "three parts are no INI path");

    const std::string dlss =
        "# comment\r\nframe_generation = 2x      ; off, 2x, 3x\r\nname = \"My name\" ; quoted\r\ncolor = #FF0000\r\n";
    check(d.parse(Syntax::Ini, dlss, e), "CRLF, comments, quotes");
    check(d.lookup(path("frame_generation"), s, why) && s.value == "2x", "a key before any section; the comment cut");
    check(d.lookup(path("name"), s, why) && s.value == "My name" && s.quote == '"', "a quoted value");
    check(d.lookup(path("color"), s, why) && s.value == "#FF0000", "a value that starts with # is no comment");
    check(d.parse(Syntax::Ini, "a=1\na=2\n", e) && !d.lookup(path("a"), s, why) && why.find("twice") != std::string::npos,
          "a key that appears twice");
    check(!d.parse(Syntax::Ini, "[open\n", e) && !d.parse(Syntax::Ini, "just words\n", e), "malformed lines");
}

void edits_ini_surgically() {
    std::string out;
    std::string why;
    check(apply_edits(Syntax::Ini, kQuest, {{path("Post.Players"), "3"}, {path("Post.PassEnabled"), "0"}}, out, why) &&
              out == replaced(replaced(kQuest, "Players=2", "Players=3"), "PassEnabled=1", "PassEnabled=0"),
          "two values, nothing else");
    const std::string dlss = "frame_generation = 2x      ; off, 2x\r\nreflex = on\r\n";
    check(apply_edits(Syntax::Ini, dlss, {{path("frame_generation"), "4x"}}, out, why) &&
              out == replaced(dlss, "2x      ;", "4x      ;"),
          "the same length");
    check(apply_edits(Syntax::Ini, dlss, {{path("frame_generation"), "off"}}, out, why) &&
              out == replaced(dlss, "2x      ;", "off     ;"),
          "longer: the comment keeps its column");
    check(apply_edits(Syntax::Ini, dlss, {{path("frame_generation"), "a-much-longer-value"}}, out, why) &&
              out == replaced(dlss, "2x      ;", "a-much-longer-value ;"),
          "much longer: one space stays");
    check(apply_edits(Syntax::Ini, "name = \"My name\"\n", {{path("name"), "Other"}}, out, why) &&
              out == "name = \"Other\"\n",
          "quotes stay");
    check(!apply_edits(Syntax::Ini, dlss, {{path("reflex"), "on ; off"}}, out, why) &&
              !apply_edits(Syntax::Ini, dlss, {{path("reflex"), " on"}}, out, why) &&
              !apply_edits(Syntax::Ini, dlss, {{path("reflex"), "a\nb=c"}}, out, why) &&
              !apply_edits(Syntax::Ini, "n = \"x\"\n", {{path("n"), "say \"hi\""}}, out, why),
          "values the file's form cannot hold are refused");
    check(!apply_edits(Syntax::Ini, kQuest, {{path("Post.Players"), "3"}, {path("post.players"), "4"}}, out, why) &&
              why.find("twice") != std::string::npos,
          "one value edited twice");
}

void knows_syntaxes() {
    Syntax s;
    check(syntax_from_name("nativePC/plugins/FextyModConfig.JSON", s) && s == Syntax::Json, ".json");
    check(syntax_from_name("quest_post.ini", s) && s == Syntax::Ini && syntax_from_name("ini", s), ".ini, by name");
    check(!syntax_from_name("mas-config.toml", s), "TOML not yet");
}

// ---- Descriptions ----

const char* const kFextyPage = R"({
  // A community page for Fexty's Mod (comments are allowed).
  "cmfm": 1,
  "mod": "Fexty's Mod",
  "source": "community",
  "made_by": "akif",
  "config": {
    "file": "nativePC/plugins/FextyModConfig.json",
    "expect": ["Defaults.HeroicsModeDefaultOn", "Hotkeys"]
  },
  "pages": [
    { "title": "Modes", "settings": [
      { "section": "At start" },
      { "id": "heroics", "label": "Heroics mode", "type": "bool",
        "path": "Defaults.HeroicsModeDefaultOn", "default": true },
      { "label": "Cat helpers", "type": "choice", "path": "Defaults.CatHelpersDefault",
        "choices": { "gajalaka": "Gajalaka", "boaboa": "Boaboa" }, "default": "gajalaka" },
      { "label": "Heroics cooldown", "type": "int", "path": "Cooldowns.HeroicsModeCooldown",
        "min": 0, "max": 600, "step": 10, "unit": " s", "default": 0, "lock_unless": { "heroics": true } },
      { "label": "Missing value", "type": "bool", "path": "Defaults.NoSuchKey" }
    ]},
    { "title": "Hotkeys", "settings": [
      { "label": "Heroics hotkey", "type": "key", "path": "Hotkeys.HeroicsModeHotkey", "keys": "dik", "default": "F4" },
      { "label": "Cat helper", "type": "text", "path": "Defaults.CatHelpersDefault" }
    ]}
  ]
})";

void reads_descriptions() {
    Description d;
    std::string why;
    check(parse_description(kFextyPage, "fexty_mod.json", d, why), "a good description reads");
    check(d.mod == "Fexty's Mod" && !d.author && d.made_by == "akif" && d.syntax == Syntax::Json && !d.live &&
              d.expect.size() == 2 && d.pages.size() == 2 && d.pages[0].settings.size() == 5,
          "its fields");
    const SettingSpec& cat = d.pages[0].settings[2];
    check(cat.type == SettingSpec::Type::Choice && cat.choices.size() == 2 && cat.choices[1].second == "Boaboa" &&
              cat.default_value == "gajalaka",
          "a choice: stored values and labels in order");
    check(d.pages[0].settings[3].lock_unless.size() == 1 && d.pages[0].settings[3].lock_unless[0].first == "heroics",
          "a condition");

    const auto refused = [&](const std::string& text, const char* part, const char* what) {
        Description x;
        std::string w;
        const bool bad = !parse_description(text, "x.json", x, w) && w.find(part) != std::string::npos;
        if (!bad) {
            std::printf("  (%s: got \"%s\")\n", what, w.c_str());
        }
        check(bad, what);
    };
    const std::string base = R"({"cmfm": 1, "mod": "M", "source": "author", "config": {"file": "a.ini"},
  "pages": [{"title": "P", "settings": [)";
    refused(base + R"({"lable": "x", "type": "bool", "path": "a"}]}]})", "line 2: unknown field \"lable\" in a setting",
            "a typo names the field and its line");
    refused(base + R"({"label": "x", "type": "bool"}]}]})", "\"path\" is missing", "a missing path");
    refused(base + R"({"label": "x", "type": "slider", "path": "a"}]}]})", "\"type\" is bool", "an unknown type");
    refused(base + R"({"label": "x", "type": "choice", "path": "a", "choices": ["A"], "default": "B"}]}]})",
            "stored values", "a choice's default must be a choice");
    refused(base + R"({"label": "x", "type": "int", "path": "a", "min": 0, "max": 1000}]}]})", "101 values",
            "too many slider values");
    refused(base + R"({"label": "x", "type": "bool", "path": "a", "hide_unless": {"nope": true}}]}]})",
            "no setting has as its id", "a condition naming nothing");
    refused(base + R"({"id": "a", "label": "x", "type": "bool", "path": "a"}, {"id": "a", "label": "y", "type": "bool", "path": "b"}]}]})",
            "used twice", "an id used twice");
    refused(base + R"({"section": "Only a heading"}]}]})", "besides its sections", "a page of headings only");
    refused(R"({"cmfm": 1, "mod": "M", "source": "author", "config": {"file": "../../Windows/x.ini"}, "pages": []})",
            "inside the game folder", "a path out of the game folder");
    refused(R"({"cmfm": 1, "mod": "M", "source": "author", "config": {"file": "C:/x.ini"}, "pages": []})",
            "inside the game folder", "an absolute path");
    refused(R"({"cmfm": 2, "mod": "M", "source": "author", "config": {"file": "a.ini"}, "pages": []})",
            "format 1", "another format version");
    refused(R"({"cmfm": 1, "mod": "M", "source": "someone", "config": {"file": "a.ini"}, "pages": []})",
            "\"author\" or \"community\"", "an unknown source");
    refused(R"({"cmfm": 1, "mod": "M", "source": "author", "config": {"file": "a.toml"}, "pages": []})",
            "syntax is unknown", "a config syntax CMFM does not edit");
    check(safe_relative_path("nativePC/plugins/x.json") && safe_relative_path("loader-config.json") &&
              !safe_relative_path("a/../b") && !safe_relative_path("/a") && !safe_relative_path("a//b"),
          "safe paths");
}

// ---- A description's pages ----

class FakeFiles : public Files {
public:
    std::map<std::string, std::string> files;
    int writes = 0;
    bool read(const std::string& relative, std::string& out, std::string& why) override {
        const auto f = files.find(relative);
        if (f == files.end()) {
            why = "not there";
            return false;
        }
        out = f->second;
        return true;
    }
    bool write(const std::string& relative, const std::string& text, std::string&) override {
        files[relative] = text;
        ++writes;
        return true;
    }
};

std::string g_log;
void log_line(int, const char* line) { g_log += std::string(line) + "\\n"; }

const cmfm_setting_t* by_label(const cmfm_page_t& page, const char* label) {
    for (std::uint32_t i = 0; i < page.setting_count; ++i) {
        if (std::strcmp(page.settings[i].label, label) == 0) {
            return &page.settings[i];
        }
    }
    return nullptr;
}

void shows_and_writes_a_config() {
    Description d;
    std::string why;
    parse_description(kFextyPage, "fexty_mod.json", d, why);
    FakeFiles files;
    const std::string path = "nativePC/plugins/FextyModConfig.json";
    files.files[path] = kFexty;
    ConfigPages pages(std::move(d), files, &log_line);
    check(pages.check(why), "the config is there and has what the page expects");
    const auto& p = pages.pages();
    check(p.size() == 2 && std::strcmp(p[0].mod, "Fexty's Mod") == 0 && p[0].user == &pages, "two pages, one mod");
    check(std::strstr(p[0].plugin, "community page by akif") != nullptr, "the Overview names a community page");
    for (const cmfm_page_t& page : p) {
        cmfm::menu::Page copy;
        std::string w;
        check(cmfm::menu::copy_page(&page, copy, w), "CMFM accepts the built page as it accepts a plugin's");
    }
    check((p[0].flags & CMFM_PAGE_NO_RESET) != 0 && (p[1].flags & CMFM_PAGE_NO_RESET) == 0,
          "a reset row only where every value has a default");

    p[0].on_event(p[0].user, CMFM_EVENT_OPENED);
    const cmfm_setting_t* heroics = by_label(p[0], "Heroics mode");
    const cmfm_setting_t* cat = by_label(p[0], "Cat helpers");
    const cmfm_setting_t* cooldown = by_label(p[0], "Heroics cooldown");
    const cmfm_setting_t* missing = by_label(p[0], "Missing value");
    const cmfm_setting_t* hotkey = by_label(p[1], "Heroics hotkey");
    cmfm_value_t v = {};
    check(p[0].get(p[0].user, heroics->id, &v) == CMFM_OK && v.i == 1, "a bool from the file");
    check(p[0].get(p[0].user, cat->id, &v) == CMFM_OK && v.i == 0, "a choice from the file");
    check(p[1].get(p[1].user, hotkey->id, &v) == CMFM_OK && v.i == 0x73, "a DirectInput hotkey: \"62\" is F4");
    check((heroics->flags & CMFM_FLAG_RESTART) != 0, "applies: restart marks the rows");
    char why_text[256] = {};
    check(p[0].locked(p[0].user, missing->id, why_text, sizeof why_text) != 0 &&
              std::strstr(why_text, "not in the file") != nullptr,
          "a value not in the file is read-only, with the reason");
    check(p[0].locked(p[0].user, cooldown->id, why_text, sizeof why_text) == 0, "the condition holds");
    char line[64] = {};
    p[1].get_text(p[1].user, by_label(p[1], "Cat helper")->id, line, sizeof line);
    check(std::strcmp(line, "gajalaka") == 0, "a text line shows the value");

    cmfm_value_t off = {};
    check(p[0].validate(p[0].user, heroics->id, &off, why_text, sizeof why_text) == CMFM_OK &&
              p[0].set(p[0].user, heroics->id, &off) == CMFM_OK,
          "Heroics off");
    check(p[0].get(p[0].user, heroics->id, &v) == CMFM_OK && v.i == 0, "the change reads back before the write");
    check(p[0].locked(p[0].user, cooldown->id, why_text, sizeof why_text) != 0 &&
              std::strcmp(why_text, "Needs Heroics mode: On.") == 0,
          "lock_unless follows the change, with its reason");
    cmfm_value_t f6 = {};
    f6.i = 0x75;
    check(p[1].validate(p[1].user, hotkey->id, &f6, why_text, sizeof why_text) == CMFM_OK &&
              p[1].set(p[1].user, hotkey->id, &f6) == CMFM_OK,
          "the hotkey to F6");
    cmfm_value_t ctrl_f6 = {};
    ctrl_f6.i = CMFM_KEY_CTRL | 0x75;
    check(p[1].validate(p[1].user, hotkey->id, &ctrl_f6, why_text, sizeof why_text) != CMFM_OK &&
              std::strstr(why_text, "without Ctrl") != nullptr,
          "a key the file cannot hold is refused, with the reason");
    check(files.writes == 0, "nothing written while the page is open");
    p[0].on_event(p[0].user, CMFM_EVENT_CLOSED);
    std::string want = replaced(kFexty, "\"HeroicsModeDefaultOn\": true", "\"HeroicsModeDefaultOn\": false");
    want = replaced(want, "\"62\"", "\"64\"");
    check(files.writes == 1 && files.files[path] == want, "closed: both changes written, nothing else");
    check(g_log.find("2 change(s) written") != std::string::npos, "and logged");

    // A change made outside while the page is open keeps the file's value.
    p[0].on_event(p[0].user, CMFM_EVENT_OPENED);
    cmfm_value_t on = {};
    on.i = 1;
    p[0].set(p[0].user, heroics->id, &on);
    cmfm_value_t boa = {};
    boa.i = 1;
    p[0].set(p[0].user, cat->id, &boa);
    files.files[path] = replaced(files.files[path], "\"gajalaka\"", "\"other\"");
    p[0].on_event(p[0].user, CMFM_EVENT_CLOSED);
    check(files.files[path].find("\"other\"") != std::string::npos &&
              files.files[path].find("\"HeroicsModeDefaultOn\": true") != std::string::npos,
          "the cat helper changed outside: kept; the other change written");

    // Expect: another version of the mod.
    Description d2;
    parse_description(kFextyPage, "fexty_mod.json", d2, why);
    FakeFiles other;
    other.files[path] = "{\"Defaults\": {}}";
    ConfigPages old(std::move(d2), other, &log_line);
    check(!old.check(why) && why.find("another version") != std::string::npos, "expect catches another version");
    FakeFiles none;
    Description d3;
    parse_description(kFextyPage, "fexty_mod.json", d3, why);
    ConfigPages absent(std::move(d3), none, &log_line);
    check(!absent.check(why) && why.find("cannot be read") != std::string::npos, "no config file: not shown");
}

void writes_ini_switches_in_their_form() {
    const char* page = R"({"cmfm": 1, "mod": "Quest post", "source": "community", "config": {"file": "nativePC/plugins/quest_post.ini"},
      "applies": "live",
      "pages": [{"title": "Post", "settings": [
        {"label": "Players", "type": "int", "path": "Post.Players", "min": 1, "max": 4, "default": 4},
        {"label": "Passcode on", "type": "bool", "path": "Post.PassEnabled", "default": false},
        {"label": "Verbose", "type": "bool", "path": "Debug.Verbose", "default": false}]}]})";
    Description d;
    std::string why;
    check(parse_description(page, "quest_post.json", d, why), "an INI page reads");
    FakeFiles files;
    const std::string path = "nativePC/plugins/quest_post.ini";
    files.files[path] = "[Post]\nPlayers=2\nPassEnabled=1\n[Debug]\nVerbose=TRUE\n";
    ConfigPages pages(std::move(d), files, &log_line);
    check(pages.check(why), "checked");
    const cmfm_page_t& p = pages.pages()[0];
    check((p.settings[0].flags & CMFM_FLAG_RESTART) == 0, "applies: live: no restart marks");
    p.on_event(p.user, CMFM_EVENT_OPENED);
    cmfm_value_t v = {};
    v.i = 3;
    p.set(p.user, p.settings[0].id, &v);
    v.i = 0;
    p.set(p.user, p.settings[1].id, &v);
    p.set(p.user, p.settings[2].id, &v);
    p.on_event(p.user, CMFM_EVENT_CLOSED);
    check(files.files[path] == "[Post]\nPlayers=3\nPassEnabled=0\n[Debug]\nVerbose=FALSE\n",
          "1/0 stays 1/0, TRUE becomes FALSE");
}

std::string read_file(const std::string& path) {
    std::string text;
    if (FILE* f = std::fopen(path.c_str(), "rb")) {
        char b[4096];
        std::size_t n = 0;
        while ((n = std::fread(b, 1, sizeof b, f)) > 0) {
            text.append(b, n);
        }
        std::fclose(f);
    }
    return text;
}

// The example pages in examples/config_pages, as the game reads them.
void checks_the_examples() {
    const std::string root = std::string(CMFM_SOURCE_DIR) + "/examples/config_pages/";
    FakeFiles files;
    files.files["nativePC/plugins/cmfm_test_config.json"] = read_file(root + "configs/cmfm_test_config.json");
    files.files["nativePC/plugins/cmfm_test_config.ini"] = read_file(root + "configs/cmfm_test_config.ini");
    check(!files.files["nativePC/plugins/cmfm_test_config.json"].empty(), "the example configs are found");
    for (const char* name : {"cmfm_test_json.json", "cmfm_test_ini.json", "cmfm_test_replaced.json"}) {
        Description d;
        std::string why;
        const bool parsed = parse_description(read_file(root + "pages/" + name), name, d, why);
        if (!parsed) {
            std::printf("  (%s: %s)\n", name, why.c_str());
        }
        check(parsed, "an example page reads");
        ConfigPages pages(std::move(d), files, &log_line);
        check(pages.check(why), "its config has what it expects");
        for (const cmfm_page_t& page : pages.pages()) {
            cmfm::menu::Page copy;
            check(cmfm::menu::copy_page(&page, copy, why), "CMFM accepts its pages");
        }
        pages.opened();
        for (const cmfm_page_t& page : pages.pages()) {
            for (std::uint32_t i = 0; i < page.setting_count; ++i) {
                const cmfm_setting_t& s = page.settings[i];
                char reason[256] = {};
                if (s.type != CMFM_TYPE_SECTION && page.locked(page.user, s.id, reason, sizeof reason) != 0 &&
                    std::strncmp(reason, "Needs ", 6) != 0) {
                    std::printf("  (%s, %s: %s)\n", name, s.label, reason);
                    check(false, "every example value reads from its config");
                }
            }
        }
    }
    Description broken;
    std::string why;
    check(!parse_description(read_file(root + "pages/cmfm_test_broken.json"), "cmfm_test_broken.json", broken, why) &&
              why == "line 7: unknown field \"lable\" in a setting",
          "the broken example is refused with its line");
}

}  // namespace

int main() {
    reads_json_with_positions();
    parses_paths();
    edits_json_surgically();
    reads_ini();
    edits_ini_surgically();
    knows_syntaxes();
    reads_descriptions();
    shows_and_writes_a_config();
    writes_ini_switches_in_their_form();
    checks_the_examples();
    if (g_failures == 0) {
        std::printf("test_config: all passed\n");
    }
    return g_failures == 0 ? 0 : 1;
}
