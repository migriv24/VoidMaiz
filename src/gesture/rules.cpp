/* rules.cpp — rules of the mantle (voidmaiz/rules.hpp).
 *
 * Everything here is a read of the exported state and a command string. The
 * rules live in the Core's document; this file holds none of them. */
#include "voidmaiz/rules.hpp"

#include "voidmaiz/gesture.hpp" // compile_batch

#include "cJSON.h"

#include <memory>

namespace maiz {
namespace {

struct Json {
    cJSON* p = nullptr;
    explicit Json(cJSON* q) : p(q) {}
    ~Json() { cJSON_Delete(p); }
    Json(const Json&) = delete;
    Json& operator=(const Json&) = delete;
};

/* The active mantle, or null. A rule belongs to the mantle in view, the same
 * one `rule add` writes to. */
const cJSON* active_mantle(const cJSON* root) {
    const cJSON* active = cJSON_GetObjectItemCaseSensitive(root, "active");
    const cJSON* name = cJSON_GetObjectItemCaseSensitive(active, "mantle");
    if (!cJSON_IsString(name)) return nullptr;
    const cJSON* m = nullptr;
    cJSON_ArrayForEach(m, cJSON_GetObjectItemCaseSensitive(root, "mantles")) {
        const cJSON* nm = cJSON_GetObjectItemCaseSensitive(m, "name");
        if (cJSON_IsString(nm) && std::string_view(nm->valuestring) == name->valuestring) return m;
    }
    return nullptr;
}

/* The index of `rule` in the active mantle's rules, and its driver. -1 when it
 * is not there. The LAST match wins, so a rule written twice (two devices
 * turning it on at once) reads as the later one and `rule rm` removes that. */
int find_rule(const cJSON* root, std::string_view rule, std::string* driver) {
    const cJSON* m = active_mantle(root);
    if (!m) return -1;
    int i = 0, found = -1;
    const cJSON* r = nullptr;
    cJSON_ArrayForEach(r, cJSON_GetObjectItemCaseSensitive(m, "rules")) {
        const cJSON* k = cJSON_GetObjectItemCaseSensitive(r, "rule");
        if (cJSON_IsString(k) && std::string_view(k->valuestring) == rule) {
            found = i;
            if (driver) {
                const cJSON* d = cJSON_GetObjectItemCaseSensitive(r, "driver");
                *driver = cJSON_IsString(d) ? d->valuestring : "";
            }
        }
        ++i;
    }
    return found;
}

} // namespace

bool rule_on(const Core& core, std::string_view rule) {
    Json root(cJSON_Parse(core.export_state().c_str()));
    return root.p && find_rule(root.p, rule, nullptr) >= 0;
}

std::string rule_driver(const Core& core, std::string_view rule) {
    Json root(cJSON_Parse(core.export_state().c_str()));
    if (!root.p) return {};
    std::string driver;
    return find_rule(root.p, rule, &driver) >= 0 ? driver : std::string();
}

std::string compile_rule_on(const Core& core, std::string_view rule,
                            std::string_view driver) {
    Json root(cJSON_Parse(core.export_state().c_str()));
    if (!root.p) return {};
    std::string current;
    int at = find_rule(root.p, rule, &current);
    if (at >= 0 && current == driver) return {}; // already so: say nothing
    std::string body = R"({"rule":")" + std::string(rule) + '"';
    if (!driver.empty()) body += R"(,"driver":")" + std::string(driver) + '"';
    body += '}';
    std::string add = "rule add " + arg(body);
    /* Taking over the driving is a removal and an addition, as one frame: undo
     * puts the previous driver back rather than leaving the rule on twice. */
    if (at >= 0) return compile_batch({"rule rm " + std::to_string(at), add});
    return add;
}

std::string compile_rule_off(const Core& core, std::string_view rule) {
    Json root(cJSON_Parse(core.export_state().c_str()));
    if (!root.p) return {};
    int at = find_rule(root.p, rule, nullptr);
    if (at < 0) return {};
    return "rule rm " + std::to_string(at);
}

} // namespace maiz
