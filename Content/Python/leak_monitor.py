"""
Overnight leak monitor. Runs `MemReport -full` on a fixed interval so we can
diff the snapshots in the morning and see which UObject classes / texture pools
are growing without bound.

Pair with `-LLMCSV` on the editor command line for low-overhead per-tag memory
sampling (the LLM CSV gives the big-picture "which subsystem leaks?" answer;
MemReport gives the per-class detail to confirm it).

Run in the UE Editor's Output Log:
    py "Content/Python/leak_monitor.py"

Stops on editor shutdown. Re-run to restart.

MemReport output lands in: Saved/Profiling/MemReports/
"""

import time
import unreal


INTERVAL_SECONDS = 3600  # 1 hour
FIRST_FIRE_DELAY_SECONDS = 10  # initial baseline shortly after start

_state = {"last_fire": 0.0, "tick_handle": None, "fire_count": 0}


def _get_editor_world():
    # UE 5.4: prefer UnrealEditorSubsystem; fall back to EditorLevelLibrary.
    try:
        subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
        if subsystem:
            world = subsystem.get_editor_world()
            if world:
                return world
    except Exception:
        pass
    return unreal.EditorLevelLibrary.get_editor_world()


def _fire_memreport():
    world = _get_editor_world()
    if world is None:
        unreal.log_warning("[LeakMonitor] No editor world; skipping MemReport")
        return
    unreal.SystemLibrary.execute_console_command(world, "MemReport -full")
    _state["fire_count"] += 1
    unreal.log(
        "[LeakMonitor] Fired MemReport -full #{0} at {1}".format(
            _state["fire_count"],
            time.strftime("%Y-%m-%d %H:%M:%S"),
        )
    )


def _tick(_delta):
    now = time.time()
    if now - _state["last_fire"] < INTERVAL_SECONDS:
        return
    _state["last_fire"] = now
    _fire_memreport()


def start():
    if _state["tick_handle"] is not None:
        unreal.unregister_slate_post_tick_callback(_state["tick_handle"])
        _state["tick_handle"] = None

    # Schedule the baseline to land FIRST_FIRE_DELAY_SECONDS after start.
    _state["last_fire"] = time.time() - INTERVAL_SECONDS + FIRST_FIRE_DELAY_SECONDS
    _state["tick_handle"] = unreal.register_slate_post_tick_callback(_tick)
    unreal.log(
        "[LeakMonitor] Started. Will fire MemReport every {0}s. Baseline in ~{1}s.".format(
            INTERVAL_SECONDS, FIRST_FIRE_DELAY_SECONDS
        )
    )


start()
