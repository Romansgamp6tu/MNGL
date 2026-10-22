from __future__ import annotations

import tempfile
import threading
import time
import unittest
from pathlib import Path
from unittest.mock import Mock, patch

from mngl_cli.client import (
    CommandResult,
    GuiSession,
    GuiTree,
    MNGLCommandError,
    MNGLProtocolError,
    Tree,
)


SESSION_ID = "0123456789abcdef0123456789abcdef"
SESSION_TOKEN = "feedfacecafebeef"


class FakeClient:
    def __init__(self, timeout: float = 1.0, cwd: Path | None = None):
        self.timeout = timeout
        self.cwd = cwd


class LiveGuiClientTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temp = tempfile.TemporaryDirectory(prefix="mngl-gui-tests-")
        self.root = Path(self.temp.name)
        self.gettempdir = patch(
            "mngl_cli.client.tempfile.gettempdir", return_value=str(self.root)
        )
        self.gettempdir.start()
        self.addCleanup(self.gettempdir.stop)
        self.addCleanup(self.temp.cleanup)

    def add_session(
        self,
        session_id: str = SESSION_ID,
        token: str = SESSION_TOKEN,
        *,
        protocol: str = "1",
    ) -> Path:
        directory = self.root / f"mngl-live-{session_id}"
        directory.mkdir()
        (directory / "session.meta").write_text(
            f"protocol={protocol}\nsession_id={session_id}\ntoken={token}\n",
            encoding="ascii",
        )
        return directory

    def make_session(
        self,
        *,
        timeout: float = 1.0,
        session_id: str | None = None,
    ) -> GuiSession:
        return GuiSession(FakeClient(timeout), session_id=session_id)

    def start_responder(
        self,
        session: GuiSession,
        *,
        exit_code: int = 0,
        stdout: str = "",
        stderr: str = "",
        protocol: str = "1",
        token: str | None = None,
        request_id: str | None = None,
        stdout_hex: str | None = None,
        stderr_hex: str | None = None,
        exit_code_text: str | None = None,
    ) -> tuple[threading.Thread, list[dict[str, str]], list[BaseException]]:
        requests: list[dict[str, str]] = []
        errors: list[BaseException] = []

        def respond() -> None:
            pending = session.directory / "request.pending"
            deadline = time.monotonic() + 2.0
            while not pending.is_file() and time.monotonic() < deadline:
                time.sleep(0.002)
            if not pending.is_file():
                errors.append(TimeoutError("test responder saw no request"))
                return
            fields = dict(
                line.partition("=")[::2]
                for line in pending.read_text(encoding="ascii").splitlines()
                if "=" in line
            )
            pending.unlink()
            requests.append(fields)
            identifier = request_id or fields.get("request_id", "")
            token_value = token or fields.get("token", "")
            response_fields = [
                f"protocol={protocol}",
                f"request_id={identifier}",
                f"token={token_value}",
                f"exit_code={exit_code if exit_code_text is None else exit_code_text}",
                f"stdout_hex={stdout.encode('utf-8').hex() if stdout_hex is None else stdout_hex}",
                f"stderr_hex={stderr.encode('utf-8').hex() if stderr_hex is None else stderr_hex}",
                "",
            ]
            # The transport filename always follows the request. The body ID
            # can differ to exercise the client's correlation check.
            filename_id = fields.get("request_id", "")
            response_path = session.directory / f"response-{filename_id}.ready"
            temporary = session.directory / f"response-{filename_id}.tmp"
            temporary.write_text("\n".join(response_fields), encoding="ascii")
            temporary.replace(response_path)

        thread = threading.Thread(target=respond, daemon=True)
        thread.start()
        return thread, requests, errors

    def run_with_responder(
        self,
        session: GuiSession,
        operation: str = "insert",
        reference: str = r"C:\trees\demo.tree",
        key: str = "42",
        **response: object,
    ) -> tuple[CommandResult, dict[str, str], list[BaseException]]:
        thread, requests, errors = self.start_responder(session, **response)
        result = session.run(operation, reference, key)
        thread.join(timeout=2.0)
        self.assertFalse(thread.is_alive(), "test responder must complete")
        self.assertFalse(errors, str(errors))
        self.assertEqual(len(requests), 1)
        return result, requests[0], errors

    def test_discovers_the_only_running_gui_session(self) -> None:
        directory = self.add_session()
        gui = self.make_session()
        self.assertEqual(gui.directory, directory)
        self.assertEqual(gui.session_id, SESSION_ID)
        self.assertEqual(gui._token, SESSION_TOKEN)

    def test_missing_gui_session_has_actionable_error(self) -> None:
        with self.assertRaisesRegex(ConnectionError, "Launch MNGL"):
            self.make_session()

    def test_multiple_gui_sessions_require_explicit_id(self) -> None:
        self.add_session(SESSION_ID, "first")
        second_id = "fedcba9876543210fedcba9876543210"
        self.add_session(second_id, "second")
        with self.assertRaisesRegex(
            ConnectionError,
            "multiple sessions from an older MNGL build",
        ):
            self.make_session()
        selected = self.make_session(session_id=second_id)
        self.assertEqual(selected.session_id, second_id)
        self.assertEqual(selected._token, "second")

    def test_ignores_metadata_with_unknown_protocol(self) -> None:
        self.add_session(protocol="2")
        with self.assertRaises(ConnectionError):
            self.make_session()

    def test_insert_request_round_trips_utf8_path_and_result(self) -> None:
        self.add_session()
        gui = self.make_session()
        reference = "C:\\Деревья\\с пробелом.tree"
        key = "42"
        result, request, _ = self.run_with_responder(
            gui,
            "insert",
            reference,
            key,
            stdout="operation=insert\nchanged=true\nключ=42\n",
        )
        self.assertEqual(request["protocol"], "1")
        self.assertEqual(request["token"], SESSION_TOKEN)
        self.assertEqual(request["operation"], "insert")
        self.assertEqual(bytes.fromhex(request["tree_reference_hex"]).decode(), reference)
        self.assertEqual(bytes.fromhex(request["key_hex"]).decode(), key)
        self.assertEqual(result.value("changed"), "true")
        self.assertIn("ключ=42", result.stdout)

    def test_command_failure_raises_with_original_result(self) -> None:
        self.add_session()
        gui = self.make_session()
        thread, requests, errors = self.start_responder(
            gui, exit_code=1, stderr="The tree tab is closed"
        )
        with self.assertRaises(MNGLCommandError) as caught:
            gui.run("find", "archive:trees/missing.tree", "5")
        thread.join(timeout=2.0)
        self.assertFalse(thread.is_alive())
        self.assertFalse(errors, str(errors))
        self.assertEqual(caught.exception.result.exit_code, 1)
        self.assertEqual(caught.exception.result.stderr, "The tree tab is closed")
        self.assertEqual(len(requests), 1)

    def test_check_false_returns_failure_result(self) -> None:
        self.add_session()
        gui = self.make_session()
        thread, requests, errors = self.start_responder(
            gui, exit_code=1, stderr="unsupported key type"
        )
        result = gui.run("erase", "archive:trees/test.tree", "bad", check=False)
        thread.join(timeout=2.0)
        self.assertFalse(thread.is_alive())
        self.assertFalse(errors, str(errors))
        self.assertFalse(result.ok)
        self.assertEqual(result.stderr, "unsupported key type")
        self.assertEqual(len(requests), 1)

    def test_invalid_operation_is_rejected_without_queue_write(self) -> None:
        self.add_session()
        gui = self.make_session()
        with self.assertRaisesRegex(ValueError, "GUI operations are"):
            gui.run("merge", "tree.tree", "1")
        self.assertFalse((gui.directory / "request.pending").exists())

    def test_pending_request_prevents_overwriting_another_command(self) -> None:
        self.add_session()
        gui = self.make_session()
        pending = gui.directory / "request.pending"
        pending.write_text("another request", encoding="ascii")
        with self.assertRaisesRegex(RuntimeError, "pending live Python command"):
            gui.run("find", "tree.tree", "3")
        self.assertEqual(pending.read_text(encoding="ascii"), "another request")

    def test_timeout_closes_connection_and_leaves_no_retry_path(self) -> None:
        self.add_session()
        gui = self.make_session(timeout=0.04)
        with self.assertRaises(TimeoutError):
            gui.run("find", "tree.tree", "3")
        self.assertTrue(gui._closed)
        with self.assertRaisesRegex(RuntimeError, "connection is closed"):
            gui.run("find", "tree.tree", "3")

    def test_malformed_response_raises_protocol_error(self) -> None:
        self.add_session()
        gui = self.make_session()
        thread, requests, errors = self.start_responder(gui, stdout_hex="not-hex")
        with self.assertRaises(MNGLProtocolError):
            gui.run("find", "tree.tree", "3")
        thread.join(timeout=2.0)
        self.assertFalse(thread.is_alive())
        self.assertFalse(errors, str(errors))
        self.assertEqual(len(requests), 1)

    def test_wrong_response_token_is_rejected(self) -> None:
        self.add_session()
        gui = self.make_session()
        thread, requests, errors = self.start_responder(gui, token="wrong")
        with self.assertRaisesRegex(MNGLProtocolError, "invalid session token"):
            gui.run("find", "tree.tree", "3")
        thread.join(timeout=2.0)
        self.assertFalse(thread.is_alive())
        self.assertFalse(errors, str(errors))
        self.assertEqual(len(requests), 1)

    def test_wrong_response_id_is_rejected(self) -> None:
        self.add_session()
        gui = self.make_session()
        thread, requests, errors = self.start_responder(
            gui,
            request_id="f" * 32,
        )
        with self.assertRaisesRegex(MNGLProtocolError, "does not match"):
            gui.run("find", "tree.tree", "3")
        thread.join(timeout=2.0)
        self.assertFalse(thread.is_alive())
        self.assertFalse(errors, str(errors))
        self.assertEqual(len(requests), 1)

    def test_bad_exit_code_is_reported_as_protocol_error(self) -> None:
        self.add_session()
        gui = self.make_session()
        thread, requests, errors = self.start_responder(gui, exit_code_text="NaN")
        with self.assertRaisesRegex(MNGLProtocolError, "malformed response"):
            gui.run("find", "tree.tree", "3")
        thread.join(timeout=2.0)
        self.assertFalse(thread.is_alive())
        self.assertFalse(errors, str(errors))
        self.assertEqual(len(requests), 1)

    def test_context_manager_closes_session(self) -> None:
        self.add_session()
        gui = self.make_session()
        with gui:
            self.assertFalse(gui._closed)
        self.assertTrue(gui._closed)

    def test_gui_tree_methods_dispatch_expected_operations(self) -> None:
        session = Mock()
        session.run.side_effect = lambda operation, reference, key: CommandResult(
            f"{operation} {reference} {key}", 0, "ok", ""
        )
        tree = GuiTree(session, "archive:trees/example.tree")
        self.assertTrue(tree.find(3).ok)
        self.assertTrue(tree.insert(4).ok)
        self.assertTrue(tree.erase(5).ok)
        self.assertEqual(
            [call.args[0] for call in session.run.call_args_list],
            ["find", "insert", "erase"],
        )

    def test_tree_handle_uses_its_cli_working_directory(self) -> None:
        self.add_session()
        cwd = Path(self.temp.name) / "workspace"
        cwd.mkdir()
        cli_session = type("CliSession", (), {"client": FakeClient(cwd=cwd)})()
        cli_tree = Tree(cli_session, "tree-id", Path("nested/demo.tree"), "bst")
        gui = self.make_session()
        handle = gui.tree(cli_tree)
        self.assertEqual(handle.reference, str((cwd / "nested/demo.tree").resolve()))

    def test_archive_reference_is_not_normalized_as_local_path(self) -> None:
        self.add_session()
        gui = self.make_session()
        tree = gui.tree("archive:trees/demo.tree")
        self.assertEqual(tree.reference, "archive:trees/demo.tree")


if __name__ == "__main__":
    unittest.main()
