"""
Sends a Python script (or file) to the running UE editor via Python Remote
Execution and prints the result. Requires Editor Preferences -> Plugins ->
Python -> Enable Remote Execution to be ON.

Usage:
    python scripts/ue_remote_exec.py --file path/to/script.py
    python scripts/ue_remote_exec.py --code 'print(unreal.EditorLevelLibrary.get_editor_world().get_name())'
"""

import argparse
import sys
import time
from pathlib import Path

# UE's remote_execution module lives in the engine plugin dir.
REMOTE_EXEC_PATH = Path(r"C:\Program Files\Epic Games\UE_5.7\Engine\Plugins\Experimental\PythonScriptPlugin\Content\Python")
sys.path.insert(0, str(REMOTE_EXEC_PATH))

import remote_execution as re_mod  # type: ignore


def main():
    ap = argparse.ArgumentParser()
    src = ap.add_mutually_exclusive_group(required=True)
    src.add_argument("--file", type=str)
    src.add_argument("--code", type=str)
    ap.add_argument("--mode", default="ExecuteFile", choices=["ExecuteFile", "ExecuteStatement", "EvaluateStatement"])
    ap.add_argument("--timeout", type=float, default=30.0)
    args = ap.parse_args()

    if args.file:
        code = Path(args.file).read_text(encoding="utf-8")
    else:
        code = args.code

    config = re_mod.RemoteExecutionConfig()
    # Defaults match the UE editor's defaults: multicast 239.0.0.1:6766.
    conn = re_mod.RemoteExecution(config)
    conn.start()
    try:
        # Discover at least one node.
        deadline = time.time() + 5.0
        while not conn.remote_nodes:
            if time.time() > deadline:
                print("ERROR: no UE editor found via remote execution. Is the editor running and is the setting in Project Settings (not Editor Preferences)?", file=sys.stderr)
                sys.exit(2)
            time.sleep(0.1)
        node = conn.remote_nodes[0]
        conn.open_command_connection(node["node_id"])
        try:
            result = conn.run_command(
                code,
                unattended=True,
                exec_mode=getattr(re_mod, "MODE_" + (
                    "EXEC_FILE" if args.mode == "ExecuteFile" else
                    "EXEC_STATEMENT" if args.mode == "ExecuteStatement" else
                    "EVAL_STATEMENT"
                )),
            )
        finally:
            conn.close_command_connection()
    finally:
        conn.stop()

    if not result:
        print("ERROR: no result from UE", file=sys.stderr)
        sys.exit(3)

    if result.get("success"):
        out = result.get("output") or []
        for entry in out:
            print(entry.get("output", ""), end="")
        ret = result.get("result")
        if ret is not None:
            print("\n[result]", ret)
    else:
        print("ERROR: command failed", file=sys.stderr)
        out = result.get("output") or []
        for entry in out:
            print(entry.get("output", ""), file=sys.stderr, end="")


if __name__ == "__main__":
    main()
