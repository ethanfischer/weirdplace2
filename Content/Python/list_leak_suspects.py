"""
Enumerates actors currently in the editor world and writes a report to
Saved/Profiling/Heap/level_actors.txt. Lets us see what's actually in the
level right now without needing the user to search the Outliner.

Run in UE Editor's Output Log:
    py "Content/Python/list_leak_suspects.py"
"""

import re
import unreal


PATTERNS = [
    "UltraDynamic", "UDS", "UDW",
    "Niagara", "NS_", "Particle", "Effect",
    "HangingPot", "Hudson", "Rick", "Seneca", "MetaHuman",
    "TV", "Car", "Spawner", "Door", "Envelope",
    "Audio", "Sound",
    "Foliage",
    "BP_",
]

OUT_PATH = r"C:\Users\ethan\repos\weirdplace2\Saved\Profiling\Heap\level_actors.txt"


def main():
    subsys = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = subsys.get_all_level_actors()

    rows = []
    for a in actors:
        name = a.get_actor_label()
        cls_name = a.get_class().get_name()
        primary_tick = False
        try:
            primary_tick = bool(a.primary_actor_tick.b_can_ever_tick)
        except Exception:
            pass
        rows.append((name, cls_name, primary_tick))

    # Build counts by pattern (case-insensitive).
    counts = {p: [] for p in PATTERNS}
    for name, cls_name, tick in rows:
        target = name + " " + cls_name
        for p in PATTERNS:
            if re.search(p, target, re.IGNORECASE):
                counts[p].append((name, cls_name, tick))

    lines = []
    lines.append(f"Total actors in level: {len(rows)}")
    lines.append("")
    lines.append("=== Pattern matches (case-insensitive, name OR class) ===")
    for p in PATTERNS:
        matches = counts[p]
        if matches:
            lines.append(f"\n[{p}] {len(matches)} match(es):")
            for name, cls_name, tick in matches:
                t = "TICK" if tick else "    "
                lines.append(f"  {t}  {name}  ({cls_name})")

    # Also list every actor with primary tick enabled (likely leak suspects).
    lines.append("")
    lines.append("=== All actors with primary tick enabled ===")
    ticking = [r for r in rows if r[2]]
    lines.append(f"Total ticking actors: {len(ticking)}")
    for name, cls_name, tick in sorted(ticking, key=lambda r: r[1] + r[0]):
        lines.append(f"  {name}  ({cls_name})")

    with open(OUT_PATH, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))

    unreal.log(f"[LeakSuspects] Wrote report to {OUT_PATH}")
    unreal.log(f"[LeakSuspects] {len(rows)} total actors, {len(ticking)} ticking.")


main()
