/* embed.cpp — maiz::Core, the RAII embedding of the Void Core C ABI.
 * JSON parsing uses the vendored cJSON — the same parser the core itself
 * vendors, so both sides of the seam read JSON identically. */
#include "voidmaiz/embed.hpp"

#include "voidcore.h"
#include "cJSON.h"

#include <stdexcept>
#include <utility>

namespace maiz {

namespace {

/* RAII for library-owned strings (everything vc_dispatch/vc_export_state
 * return must go back through vc_free_str — never the host allocator). */
struct VcStr {
    char* p = nullptr;
    ~VcStr() { vc_free_str(p); }
    explicit operator bool() const { return p != nullptr; }
};

struct CJson {
    cJSON* p = nullptr;
    ~CJson() { cJSON_Delete(p); }
    explicit operator bool() const { return p != nullptr; }
};

std::string_view sv(const char* s) { return s ? std::string_view(s) : std::string_view(); }

Result parse_envelope(const char* raw) {
    Result r;
    if (!raw) {
        r.lines.push_back("voidmaiz: dispatch returned null (destroyed manager?)");
        return r;
    }
    CJson doc{cJSON_Parse(raw)};
    if (!doc || !cJSON_IsObject(doc.p)) {
        r.lines.push_back("voidmaiz: could not parse dispatch envelope");
        r.lines.push_back(raw);
        return r;
    }
    r.ok = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(doc.p, "ok"));
    if (cJSON* lines = cJSON_GetObjectItemCaseSensitive(doc.p, "lines"); cJSON_IsArray(lines)) {
        for (cJSON* it = lines->child; it; it = it->next)
            if (cJSON_IsString(it)) r.lines.emplace_back(it->valuestring);
    }
    if (cJSON* data = cJSON_GetObjectItemCaseSensitive(doc.p, "data"); data && !cJSON_IsNull(data)) {
        if (char* printed = cJSON_PrintUnformatted(data)) {
            r.data = printed;
            cJSON_free(printed);
        }
    }
    return r;
}

} // namespace

std::string Result::text() const {
    std::string out;
    for (const auto& line : lines) {
        if (!out.empty()) out += '\n';
        out += line;
    }
    return out;
}

/* ── the SPEC §6.1 codec, forwarded to Void Core ─────────────────────────────
 * Not one rule of §6.1 is decided in this file. See embed.hpp for why, and for
 * the law (split_argv(arg(v)).argv == {v}) that command_smoke re-checks here. */

namespace {

/* The ABI hands every codec result back as a JSON string, allocated by the
 * library's allocator. Both facts are handled in one place. */
std::string owned(char* p) {
    VcStr s{p};
    return s.p ? std::string(s.p) : std::string();
}

std::vector<std::string> strings_of(const cJSON* arr) {
    std::vector<std::string> out;
    if (!cJSON_IsArray(const_cast<cJSON*>(arr))) return out;
    for (const cJSON* it = arr->child; it; it = it->next)
        if (cJSON_IsString(const_cast<cJSON*>(it))) out.emplace_back(it->valuestring);
    return out;
}

std::string str_of(const cJSON* o, const char* key) {
    const cJSON* v = cJSON_GetObjectItemCaseSensitive(const_cast<cJSON*>(o), key);
    return cJSON_IsString(const_cast<cJSON*>(v)) ? std::string(v->valuestring) : std::string();
}

} // namespace

std::string arg(std::string_view value) {
    std::string owned_in(value); // the ABI wants NUL-termination
    VcStr q{vc_arg_quote(owned_in.c_str())};
    if (!q) throw std::runtime_error("voidmaiz: vc_arg_quote failed (allocation)");
    return std::string(q.p);
}

std::string command_line(const std::vector<std::string>& argv) {
    std::string out;
    for (const std::string& a : argv) {
        if (!out.empty()) out += ' ';
        out += arg(a);
    }
    return out;
}

Argv split_argv(std::string_view line) {
    Argv r;
    std::string owned_in(line);
    const std::string json = owned(vc_argv_split_json(owned_in.c_str()));
    CJson doc{cJSON_ParseWithLength(json.data(), json.size())};
    if (!doc || !cJSON_IsObject(doc.p)) {
        r.error = "voidmaiz: could not parse vc_argv_split_json result";
        return r;
    }
    r.ok = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(doc.p, "ok"));
    if (r.ok) r.argv = strings_of(cJSON_GetObjectItemCaseSensitive(doc.p, "argv"));
    else r.error = str_of(doc.p, "error");
    return r;
}

Transcript split_transcript(std::string_view src) {
    Transcript r;
    std::string owned_in(src);
    const std::string json = owned(vc_transcript_split_json(owned_in.c_str()));
    CJson doc{cJSON_ParseWithLength(json.data(), json.size())};
    if (!doc || !cJSON_IsObject(doc.p)) {
        r.error = "voidmaiz: could not parse vc_transcript_split_json result";
        return r;
    }
    r.ok = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(doc.p, "ok"));
    if (!r.ok) {
        r.error = str_of(doc.p, "error");
        const cJSON* ln = cJSON_GetObjectItemCaseSensitive(doc.p, "line");
        if (cJSON_IsNumber(const_cast<cJSON*>(ln))) r.error_line = ln->valueint;
        return r;
    }
    r.flat = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(doc.p, "flat"));
    const cJSON* cmds = cJSON_GetObjectItemCaseSensitive(doc.p, "commands");
    if (cJSON_IsArray(const_cast<cJSON*>(cmds))) {
        for (const cJSON* it = cmds->child; it; it = it->next) {
            if (!cJSON_IsObject(const_cast<cJSON*>(it))) continue;
            Statement st;
            const cJSON* ln = cJSON_GetObjectItemCaseSensitive(const_cast<cJSON*>(it), "line");
            if (cJSON_IsNumber(const_cast<cJSON*>(ln))) st.line = ln->valueint;
            st.text = str_of(it, "text");
            st.argv = strings_of(cJSON_GetObjectItemCaseSensitive(const_cast<cJSON*>(it), "argv"));
            r.commands.push_back(std::move(st));
        }
    }
    return r;
}

/* The std::function targets live here, behind a unique_ptr, so the raw `user`
 * pointer handed to the C ABI stays valid across moves of the Core. */
struct Core::Callbacks {
    LogSink log;
    EffectHandler effect;
    std::vector<std::string> glyphs; // registered descriptors, replayed by replace_state

    static void log_thunk(const char* level, const char* op, const char* msg, void* user) {
        auto* cb = static_cast<Callbacks*>(user);
        if (cb->log) cb->log(sv(level), sv(op), sv(msg));
    }
    static char* effect_thunk(const char* op, const char* args_json, void* user) {
        auto* cb = static_cast<Callbacks*>(user);
        if (!cb->effect) return nullptr;
        std::string out = cb->effect(sv(op), sv(args_json));
        /* The core frees this with ITS allocator, so it must come from
         * vc_alloc_str — never the host's malloc/new. */
        return out.empty() ? nullptr : vc_alloc_str(out.c_str());
    }
};

Core::Core() : Core(std::string_view{}) {}

Core::Core(std::string_view state_json) : cb_(std::make_unique<Callbacks>()) {
    if (state_json.empty()) {
        m_ = vc_create(nullptr);
    } else {
        std::string owned(state_json); // the ABI wants NUL-termination
        m_ = vc_create(owned.c_str());
    }
    if (!m_) throw std::runtime_error("voidmaiz: vc_create failed (allocation)");
}

Core::~Core() {
    if (m_) vc_destroy(m_);
}

Core::Core(Core&& other) noexcept : m_(std::exchange(other.m_, nullptr)), cb_(std::move(other.cb_)) {}

Core& Core::operator=(Core&& other) noexcept {
    if (this != &other) {
        if (m_) vc_destroy(m_);
        m_ = std::exchange(other.m_, nullptr);
        cb_ = std::move(other.cb_);
    }
    return *this;
}

Result Core::dispatch(std::string_view command) {
    std::string owned(command);
    VcStr raw{vc_dispatch(m_, owned.c_str())};
    return parse_envelope(raw.p);
}

std::string Core::dispatch_raw(std::string_view command) {
    std::string owned(command);
    VcStr raw{vc_dispatch(m_, owned.c_str())};
    return raw ? std::string(raw.p) : std::string();
}

std::string Core::export_state() const {
    VcStr raw{vc_export_state(m_)};
    return raw ? std::string(raw.p) : std::string();
}

bool Core::register_glyph(std::string_view glyph_json) {
    std::string owned(glyph_json);
    bool ok = vc_register_glyph(m_, owned.c_str()) == 1;
    if (ok) cb_->glyphs.push_back(std::move(owned));
    return ok;
}

bool Core::replace_state(std::string_view state_json) {
    std::string owned(state_json);
    VC_Manager* fresh = vc_create(owned.empty() ? nullptr : owned.c_str());
    if (!fresh) return false;
    // the Callbacks object keeps its address, so the same user pointer serves
    vc_set_log_sink(fresh, cb_->log ? &Callbacks::log_thunk : nullptr, cb_.get());
    vc_set_effect_handler(fresh, cb_->effect ? &Callbacks::effect_thunk : nullptr, cb_.get());
    for (const auto& g : cb_->glyphs) vc_register_glyph(fresh, g.c_str());
    if (m_) vc_destroy(m_);
    m_ = fresh;
    return true;
}

void Core::set_log_sink(LogSink sink) {
    cb_->log = std::move(sink);
    vc_set_log_sink(m_, cb_->log ? &Callbacks::log_thunk : nullptr, cb_.get());
}

void Core::set_effect_handler(EffectHandler handler) {
    cb_->effect = std::move(handler);
    vc_set_effect_handler(m_, cb_->effect ? &Callbacks::effect_thunk : nullptr, cb_.get());
}

bool Core::tag_match(std::string_view expr, const std::vector<std::string>& tags) {
    CJson arr{cJSON_CreateArray()};
    for (const auto& t : tags)
        cJSON_AddItemToArray(arr.p, cJSON_CreateString(t.c_str()));
    char* tags_json = cJSON_PrintUnformatted(arr.p);
    std::string owned_tags = tags_json ? tags_json : "[]";
    cJSON_free(tags_json);
    std::string owned_expr(expr);
    int rc = vc_tag_match(owned_expr.c_str(), owned_tags.c_str());
    if (rc < 0) throw std::invalid_argument("voidmaiz: vc_tag_match rejected input");
    return rc == 1;
}

std::string_view Core::core_version() {
    return sv(vc_version());
}

} // namespace maiz
