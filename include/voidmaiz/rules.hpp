/* rules.hpp — a rule of the mantle: shared behaviour, not shared content.
 *
 * Some switches are not this device's preference. "Live physics is on" changes
 * what every screen showing this net does, and the author put it exactly right
 * (2026-09-22):
 *
 *     "i think 'live physics' is a rule or state of the mantle, and therefore,
 *      if live physics is turned on, it should be turned on for all synced
 *      devices. rather than a constant update of position information."
 *
 * Both halves matter. Sharing the RULE is one small command that two devices
 * agree on; sharing the RESULT would be a position broadcast at frame rate
 * forever. So the rule crosses, and each device decides what to do about it.
 *
 * The Core already has the place for this: a mantle holds `rules`, and `rule
 * add` / `rule rm` are ordinary dispatcher commands — undoable, logged, visible
 * in the CLI, and (net_smoke, 2026-09-22) carried between devices by Palabra
 * like everything else. Void Maiz adds no storage here; it only gives the
 * shape a name so that hosts agree on one:
 *
 *     {"rule":"<name>", "driver":"<who runs it>"}
 *
 * `driver` is the answer to the question a shared rule always raises: if every
 * device simulates, every device writes positions and they fight. One device
 * drives — normally whoever switched it on — and the others watch the result
 * arrive as ordinary moves. Nothing enforces this; it is a convention the hosts
 * keep, and `rule_driver` is how a host asks whether it is the one.
 *
 * A rule with no driver is simply on for everyone (a display mode, say).
 *
 * Usage, in a host that dispatches commands:
 *
 *     if (std::string cmd = compile_rule_on(core, "physics", my_id); !cmd.empty())
 *         dispatch(cmd);
 *     bool i_drive = rule_driver(core, "physics") == my_id;
 *     bool anyone  = rule_on(core, "physics");
 */
#ifndef VOIDMAIZ_RULES_HPP
#define VOIDMAIZ_RULES_HPP

#include "voidmaiz/embed.hpp"

#include <string>
#include <string_view>

namespace maiz {

/* Is this rule on in the active mantle? */
bool rule_on(const Core& core, std::string_view rule);

/* Who drives it — empty when it is off, or on with nobody named. */
std::string rule_driver(const Core& core, std::string_view rule);

/* The command that turns it on, or hands the driving to `driver`. Empty when it
 * is already on with that same driver, so a host can call it every frame. */
std::string compile_rule_on(const Core& core, std::string_view rule,
                            std::string_view driver = {});

/* The command that turns it off. Empty when it is already off. Anyone may turn
 * a rule off, including a device that is not driving it: it is the net's rule,
 * not its author's. */
std::string compile_rule_off(const Core& core, std::string_view rule);

} // namespace maiz

#endif
