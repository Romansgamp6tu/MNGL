"""Start an interactive Python prompt for the MNGL CLI or live GUI."""

from __future__ import annotations

import argparse
import code

from .client import MNGL


def _enable_completion(namespace: dict[str, object]) -> None:
    try:
        import readline
        import rlcompleter
    except ImportError:
        return
    readline.set_completer(rlcompleter.Completer(namespace).complete)
    readline.parse_and_bind("tab: complete")


def main() -> int:
    parser = argparse.ArgumentParser(
        prog="python -m mngl_cli",
        description="Open a Python prompt connected to MNGL's tree CLI or GUI.",
    )
    parser.add_argument("--exe", help="path to mngl_demo.exe")
    parser.add_argument("--cwd", help="working directory for tree file paths")
    parser.add_argument(
        "--gui",
        action="store_true",
        help="connect to a running MNGL window instead of starting a CLI process",
    )
    parser.add_argument(
        "--session-id",
        help="select one GUI session when multiple MNGL windows are running",
    )
    arguments = parser.parse_args()

    client = MNGL(arguments.exe, cwd=arguments.cwd)
    if arguments.gui:
        with client.connect_gui(session_id=arguments.session_id) as gui:
            namespace: dict[str, object] = {
                "MNGL": gui,
                "gui": gui,
                "mngl": client,
            }
            _enable_completion(namespace)
            banner = (
                f"MNGL live Python console ({client.executable})\n"
                "Use MNGL.selected_tree(), MNGL.opened_trees(), or "
                "MNGL.loaded_trees() to get tree handles.\n"
                "Example: tree = MNGL.selected_tree(); print(42 in tree)\n"
                "Use help(MNGL) or help(tree) for details."
            )
            code.interact(banner=banner, local=namespace)
        return 0

    with client.session() as cli:
        namespace: dict[str, object] = {"mngl": client, "cli": cli}
        _enable_completion(namespace)
        banner = (
            f"MNGL Python console ({client.executable})\n"
            "Use `cli` for a persistent tree session and `mngl` for one-shot calls.\n"
            "Example: tree = cli.create_tree('mngl.tree.bst', 'demo.tree')\n"
            "Type help(cli) or help(tree) for API details."
        )
        code.interact(banner=banner, local=namespace)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
