"""
Sends a Python script (or file) to the running UE editor via Python Remote
Execution and prints the result. Requires Editor Preferences -> Plugins ->
Python -> Enable Remote Execution to be ON.

Usage:
    python scripts/ue_remote_exec.py --file path/to/script.py
    python scripts/ue_remote_exec.py --code 'print(unreal.EditorLevelLibrary.get_editor_world().get_name())'

Also importable: run_code(code, mode, ...) -> (success, output_text).
"""

import argparse
import sys
import time
from pathlib import Path

# UE's remote_execution module lives in the engine plugin dir.
REMOTE_EXEC_PATH = Path(r"C:\Program Files\Epic Games\UE_5.8\Engine\Plugins\Experimental\PythonScriptPlugin\Content\Python")
sys.path.insert(0, str(REMOTE_EXEC_PATH))

import remote_execution as re_mod  # type: ignore


class EditorNotFound(RuntimeError):
    pass


def run_code(code, mode="ExecuteFile", discover_timeout=5.0):
    """Execute code (or, in ExecuteFile mode, the file at the absolute path in
    `code` — UE 5.7's MODE_EXEC_FILE resolves a path, not file contents) in the
    running editor. Returns (success: bool, output: str).
    Raises EditorNotFound if no editor answers the multicast probe."""
    config = re_mod.RemoteExecutionConfig()
    # Defaults match the UE editor's defaults: multicast 239.0.0.1:6766.
    conn = re_mod.RemoteExecution(config)
    conn.start()
    try:
        deadline = time.time() + discover_timeout
        while not conn.remote_nodes:
            if time.time() > deadline:
                raise EditorNotFound(
                    "no UE editor found via remote execution. Is the editor running "
                    "and is the setting in Project Settings (not Editor Preferences)?")
            time.sleep(0.1)
        node = conn.remote_nodes[0]
        conn.open_command_connection(node["node_id"])
        try:
            result = conn.run_command(
                code,
                unattended=True,
                exec_mode=getattr(re_mod, "MODE_" + (
                    "EXEC_FILE" if mode == "ExecuteFile" else
                    "EXEC_STATEMENT" if mode == "ExecuteStatement" else
                    "EVAL_STATEMENT"
                )),
            )
        finally:
            conn.close_command_connection()
    finally:
        conn.stop()

    if not result:
        return (False, "ERROR: no result from UE")
    text = "".join(entry.get("output", "") for entry in (result.get("output") or []))
    ret = result.get("result")
    if result.get("success") and ret is not None and ret != "None":
        text += f"\n[result] {ret}"
    return (bool(result.get("success")), text)


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

    try:
        success, text = run_code(code, mode=args.mode)
    except EditorNotFound as e:
        print(f"ERROR: {e}", file=sys.stderr)
        sys.exit(2)

    if success:
        print(text, end="")
        if text and not text.endswith("\n"):
            print()
    else:
        print("ERROR: command failed", file=sys.stderr)
        print(text, file=sys.stderr, end="")
        sys.exit(1)


if __name__ == "__main__":
    main()
