/*
 * voidmaiz/wires.hpp — connections stored as WIRE RUNES, so concurrent rewires
 * commute (UI-free, no Palabra).
 *
 * okf/concepts/collaborative-canvas.md §4.2, and Void Palabra's normative answer
 * of 2026-09-20 (VoidPalabra SPEC §5.11, okf/concepts/concurrent-structure.md).
 *
 * THE PROBLEM. A plain edge `a.1–c.2` is one value. Two devices that each rewrite
 * one end of it — two disjoint rewrites sharing a boundary wire — each REMOVE that
 * value and ADD their half. The merge keeps both halves and loses the wire nobody
 * wrote. The fix is never in the merge; it is in what each device writes:
 * "mint new things, add facts relating them to old ones, remove only what you
 * consumed, and never edit a shared thing in place."
 *
 * THE ENCODING (a host opts in; nothing changes for hosts that do not):
 *
 *   a wire                    a rune of the wire glyph (default `wire`); its
 *                             identity is its spirit.id
 *   node port i on a wire     a link  node → wire,  relation "i:0"
 *   a rewrite inheriting a    a NEW wire segment for the new node, plus a FUSION
 *   boundary                  link  new → old,  relation "="  — never an edit of old
 *
 * A wire is then an EQUIVALENCE CLASS of segments (the partition lattice: fusing
 * is only ever added, so it never conflicts), and its ends are every live
 * attachment on any segment of the class. The §4.2 case re-run: device 1 writes
 * `a'.1 → w1` and `w1 = w`, device 2 writes `c'.1 → w2` and `w2 = w`; the merged
 * class {w, w1, w2} has ends a'.1 and c'.1 — the wire nobody wrote, read from
 * what both wrote.
 *
 * WHAT THIS HEADER DOES. Reads the classes out of a projected Scene, collapses
 * them into ordinary node-to-node SceneWires for drawing (so the canvas, the
 * gestures and every host see the net they always saw), and compiles the
 * commands that write the encoding. Its union-find is a few lines and works with
 * no Palabra checkout, because a net must draw whether or not it is networked;
 * `voidmaiz_net` checks the SAME document with Palabra's normative `check_links`
 * (which also counts ports and ends) after every merge.
 *
 * WHAT IT DOES NOT DO: know what a rewrite is. Which segments a rewrite mints and
 * which it fuses is the host's (IC's), exactly as the rewrite rules are.
 */
#pragma once

#include "voidmaiz/gesture.hpp" // PortRef
#include "voidmaiz/scene.hpp"

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace maiz {

struct WireEncoding {
    std::string glyph = "wire"; // the glyph of a wire rune (a host registers it)
    std::string fuse = "=";     // the fusion relation between two segments
};

bool is_wire(const SceneNode& n, const WireEncoding& enc = {});

/* One end of a wire: a (non-wire) node and its port. */
struct WireEnd {
    std::string node; // node NAME (what commands and SceneWires use)
    int port = 0;
    bool operator==(const WireEnd&) const = default;
    bool operator<(const WireEnd& o) const {
        return node != o.node ? node < o.node : port < o.port;
    }
};

struct WireClass {
    std::string rep;                   // representative: the least segment by spirit.id
    std::vector<std::string> segments; // segment NAMES, sorted
    std::vector<WireEnd> ends;         // attachments across every segment, sorted
    /* Which segment each end hangs on, parallel to `ends` — what a gesture that
     * detaches that end must unlink. */
    std::vector<std::string> end_segment;
};

/* Every wire class in a raw projected scene (one holding wire runes as nodes and
 * the attachment/fusion links as SceneWires). Deterministic: every peer holding
 * the same document computes the same classes and representatives. */
std::vector<WireClass> wire_classes(const Scene& raw, const WireEncoding& enc = {});

/* The scene a canvas draws: wire runes and their links removed; each class with
 * exactly TWO ends becomes one SceneWire `a.i – b.j` (ends in canonical order,
 * undirected, relation "i:j", kind by ports) whose `via` names the class
 * representative. A class with ONE end is a free port and draws nothing. A class
 * with MORE than two is a merge that broke "a wire has two ends": it draws every
 * end to the first, marked `contested`, so the damage is visible rather than
 * silently hidden (the violation itself is Palabra's to name). Links that do not
 * touch a wire rune pass through unchanged. */
Scene collapse_wires(const Scene& raw, const WireEncoding& enc = {},
                     std::vector<WireClass>* classes_out = nullptr);

/* The class holding this end, or nullptr. */
const WireClass* class_of(const std::vector<WireClass>& classes, const WireEnd& end);

// ── compiling the encoding ────────────────────────────────────────────────────

/* A wire rune name no other device will mint: the host's device tag and its own
 * counter ("w-3fa9c1-17"). Two devices choosing one NAME would be a duplicate
 * name anomaly, so names must be device-scoped even though ids are random. */
std::string fresh_wire_name(std::string_view device_tag, unsigned long counter);

/* A new wire between two ends: mint the segment, attach both. ONE batch. */
std::string compile_wire(const WireEncoding& enc, std::string_view wire_name, const WireEnd& a,
                         const WireEnd& b);
/* Attach one end to a segment / detach it (the gesture of dragging a wire end
 * away). Detaching never removes the segment: another device may be fusing onto
 * it, and removing it could split a class under them. */
std::string compile_attach(std::string_view segment, const WireEnd& end);
std::string compile_detach(std::string_view segment, const WireEnd& end);
/* Fuse a newer segment onto an older one. */
std::string compile_fuse(const WireEncoding& enc, std::string_view newer, std::string_view older);
/* Mint a segment (no attachments) — for a rewrite that attaches and fuses it. */
std::string compile_segment(const WireEncoding& enc, std::string_view name);

/* One past the largest counter this device has used in a wire name, read from
 * the scene — so a restarted app does not re-mint a name it already holds. */
unsigned long next_wire_counter(const Scene& raw, std::string_view device_tag);

/* The one-time upgrade of a document whose connections are PLAIN "i:j" edges
 * between nodes (every saved project before 2026-09-21): each becomes a wire
 * rune with two attachments. Commands in order, for ONE batch; empty when there
 * is nothing to upgrade. Loose edges and edges touching wire runes are left
 * alone. `fresh` names each new segment. */
std::vector<std::string> compile_upgrade(const Scene& raw, const WireEncoding& enc,
                                         const std::function<std::string()>& fresh);

// ── how the canvas writes a connection ────────────────────────────────────────

/* The canvas compiles wire gestures (drop a wire on a port, drag a wire end
 * away, cut a wire, add-and-link) through this. Each returns the commands for
 * ONE batch, so a rewire stays one undo frame. Leave it empty and the canvas
 * writes plain edges exactly as it always has. */
struct WireWriter {
    std::function<std::vector<std::string>(const PortRef& from, const PortRef& to)> link;
    std::function<std::vector<std::string>(const SceneWire& wire)> unlink;
    explicit operator bool() const { return link && unlink; }
};

/* Wire runes. `fresh` names a new segment; `classes` returns the classes of the
 * scene the canvas is drawing (the host's last collapse_wires), which is how a
 * drawn wire's `via` finds the segments whose attachments to cut. A drawn wire
 * that is a plain edge (no `via`) is still unlinked as a plain edge. */
WireWriter reified_writer(WireEncoding enc, std::function<std::string()> fresh,
                          std::function<const std::vector<WireClass>&()> classes);

} // namespace maiz
