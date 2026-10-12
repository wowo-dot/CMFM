// Unit tests for cmfm::config: JSON with positions, INI, surgical edits.
#include "config/document.hpp"
#include "config/json.hpp"

#include <cstdio>
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

}  // namespace

int main() {
    reads_json_with_positions();
    parses_paths();
    edits_json_surgically();
    reads_ini();
    edits_ini_surgically();
    knows_syntaxes();
    if (g_failures == 0) {
        std::printf("test_config: all passed\n");
    }
    return g_failures == 0 ? 0 : 1;
}
