/* action.cpp — the canvas-action registry: named, introspectable actions whose
 * one host-supplied compile serves both a gesture front-end and (via Core's
 * future host-verbs) a CLI verb. UI-free; part of the base voidmaiz library.
 * okf/concepts/canvas-actions.md. */
#include "voidmaiz/action.hpp"

#include <cstdio>
#include <string>
#include <utility>

namespace maiz {

namespace {

/* Minimal JSON string escaper for the manifest (host-authored labels/docs may
 * carry quotes/newlines). Mirrors gesture.cpp's json_escape — kept local so
 * action.cpp stays dependency-free (no cJSON coupling for a tiny manifest). */
void json_str(std::string& out, std::string_view s) {
    out += '"';
    for (char c : s) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\t': out += "\\t"; break;
        case '\r': out += "\\r"; break;
        default:
            if ((unsigned char)c < 0x20) {
                char buf[8];
                std::snprintf(buf, sizeof buf, "\\u%04x", c);
                out += buf;
            } else {
                out += c;
            }
        }
    }
    out += '"';
}

void json_field(std::string& out, const char* key, std::string_view val, bool& first) {
    if (!first) out += ',';
    first = false;
    out += '"';
    out += key;
    out += "\":";
    json_str(out, val);
}

} // namespace

void ActionRegistry::add(ActionDescriptor a) { actions.push_back(std::move(a)); }

const ActionDescriptor* ActionRegistry::find(std::string_view name) const {
    for (const auto& a : actions)
        if (a.name == name) return &a;
    return nullptr;
}

std::vector<std::string> ActionRegistry::run(std::string_view name, const Scene& scene,
                                             const ActionArgs& args) const {
    const ActionDescriptor* a = find(name);
    if (!a || !a->compile) return {};
    return a->compile(scene, args);
}

std::string ActionRegistry::manifest() const {
    std::string out = "[";
    bool first_action = true;
    for (const auto& a : actions) {
        if (!first_action) out += ',';
        first_action = false;
        out += '{';
        bool f = true;
        json_field(out, "name", a.name, f);
        json_field(out, "label", a.label, f);
        json_field(out, "doc", a.doc, f);
        json_field(out, "gesture", a.gesture, f);
        out += ",\"params\":[";
        bool first_param = true;
        for (const auto& p : a.params) {
            if (!first_param) out += ',';
            first_param = false;
            out += '{';
            bool pf = true;
            json_field(out, "name", p.name, pf);
            json_field(out, "type", p.type, pf);
            out += ",\"required\":";
            out += p.required ? "true" : "false";
            if (!p.doc.empty()) json_field(out, "doc", p.doc, pf);
            out += '}';
        }
        out += "]}";
    }
    out += ']';
    return out;
}

} // namespace maiz
