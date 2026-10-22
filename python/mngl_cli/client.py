"""A small, dependency-free API for MNGL's tree CLI and live GUI bridge."""

from __future__ import annotations

from contextlib import contextmanager
from dataclasses import dataclass
import json
import math
import os
from pathlib import Path
import queue
import shutil
import subprocess
import tempfile
import threading
import time
from typing import Any, Iterator
import uuid


_gui_request_locks_guard = threading.Lock()
_gui_request_locks: dict[str, threading.Lock] = {}


def _gui_request_lock_for(directory: Path) -> threading.Lock:
    key = os.path.normcase(os.path.realpath(directory))
    with _gui_request_locks_guard:
        return _gui_request_locks.setdefault(key, threading.Lock())


@contextmanager
def _exclusive_gui_request_slot(directory: Path) -> Iterator[None]:
    """Claim the GUI's single request slot across threads and processes."""
    process_lock = _gui_request_lock_for(directory)
    if not process_lock.acquire(blocking=False):
        raise RuntimeError("MNGL already has a pending live Python command")

    lock_file = None
    file_lock_acquired = False
    try:
        lock_file = (directory / "request.lock").open("a+b")
        if os.name == "nt":
            import msvcrt

            lock_file.seek(0, os.SEEK_END)
            if lock_file.tell() == 0:
                lock_file.write(b"\0")
                lock_file.flush()
            lock_file.seek(0)
            try:
                msvcrt.locking(
                    lock_file.fileno(),
                    msvcrt.LK_NBLCK,
                    1,
                )
            except OSError as error:
                raise RuntimeError(
                    "MNGL already has a pending live Python command"
                ) from error
        else:
            import fcntl

            try:
                fcntl.flock(
                    lock_file.fileno(),
                    fcntl.LOCK_EX | fcntl.LOCK_NB,
                )
            except OSError as error:
                raise RuntimeError(
                    "MNGL already has a pending live Python command"
                ) from error
        file_lock_acquired = True
        yield
    finally:
        if lock_file is not None:
            if file_lock_acquired:
                try:
                    if os.name == "nt":
                        import msvcrt

                        lock_file.seek(0)
                        msvcrt.locking(
                            lock_file.fileno(),
                            msvcrt.LK_UNLCK,
                            1,
                        )
                    else:
                        import fcntl

                        fcntl.flock(lock_file.fileno(), fcntl.LOCK_UN)
                except OSError:
                    pass
            lock_file.close()
        process_lock.release()


class MNGLProtocolError(RuntimeError):
    """Raised when the executable did not return the documented JSON shape."""


@dataclass(frozen=True)
class CommandResult:
    """One CLI response. ``stdout`` and ``stderr`` are the CLI's inner streams."""

    command: str
    exit_code: int
    stdout: str
    stderr: str

    @property
    def ok(self) -> bool:
        return self.exit_code == 0

    @property
    def fields(self) -> dict[str, str]:
        """Parse simple ``key=value`` lines from human-readable command output."""
        values: dict[str, str] = {}
        for line in self.stdout.splitlines():
            key, separator, value = line.partition("=")
            if separator and key:
                values[key] = value
        return values

    def value(self, name: str, default: str | None = None) -> str | None:
        return self.fields.get(name, default)

    def require_success(self) -> CommandResult:
        if not self.ok:
            raise MNGLCommandError(self)
        return self


class MNGLCommandError(RuntimeError):
    """A CLI command returned a non-zero status."""

    def __init__(self, result: CommandResult):
        self.result = result
        details = result.stderr.strip() or result.stdout.strip() or "no details"
        super().__init__(
            f"MNGL command failed with exit code {result.exit_code}: "
            f"{result.command}\n{details}"
        )


def _stringify(value: Any) -> str:
    if value is None:
        raise TypeError("CLI arguments cannot be None")
    if isinstance(value, os.PathLike):
        value = os.fspath(value)
    if isinstance(value, bytes):
        return os.fsdecode(value)
    return str(value)


def _format_tree_key(value: Any) -> str:
    """Turn an ordinary Python value into the GUI's tree-key notation."""
    if isinstance(value, bool):
        raise TypeError("Tree keys can be int, float, or str; bool is ambiguous")
    if isinstance(value, str):
        escaped = (
            value.replace("\\", "\\\\")
            .replace('"', '\\"')
            .replace("\n", "\\n")
            .replace("\r", "\\r")
            .replace("\t", "\\t")
            .replace("\0", "\\0")
        )
        return f'"{escaped}"'
    if isinstance(value, int):
        return str(value)
    if isinstance(value, float):
        if not math.isfinite(value):
            raise ValueError("Tree float keys must be finite")
        return repr(value)
    raise TypeError("Tree keys must be int, float, or str")


def _boolean_field(result: CommandResult, name: str) -> bool:
    value = result.value(name)
    if value not in {"true", "false"}:
        raise MNGLProtocolError(
            f"MNGL did not return a boolean {name!r} field"
        )
    return value == "true"


def _read_result(payload: str, command: str) -> CommandResult:
    try:
        decoded = json.loads(payload)
    except json.JSONDecodeError as error:
        excerpt = payload[:500]
        raise MNGLProtocolError(
            f"MNGL returned invalid JSON for {command!r}: {excerpt!r}"
        ) from error
    if not isinstance(decoded, dict):
        raise MNGLProtocolError(
            f"MNGL returned a non-object JSON response for {command!r}"
        )
    try:
        return CommandResult(
            command=command,
            exit_code=int(decoded["exit_code"]),
            stdout=str(decoded["stdout"]),
            stderr=str(decoded["stderr"]),
        )
    except (KeyError, TypeError, ValueError) as error:
        raise MNGLProtocolError(
            f"MNGL JSON response is missing exit_code/stdout/stderr for {command!r}"
        ) from error


def _resolve_executable(executable: str | os.PathLike[str] | None) -> Path:
    requested = executable or os.environ.get("MNGL_DEMO_PATH")
    if requested:
        text = os.fspath(requested)
        candidate = Path(text).expanduser()
        if candidate.is_file():
            return candidate.resolve()
        located = shutil.which(text)
        if located:
            return Path(located).resolve()
        raise FileNotFoundError(
            f"Cannot find MNGL executable {text!r}. Pass its path to MNGL(...) "
            "or set MNGL_DEMO_PATH."
        )

    on_path = shutil.which("mngl_demo")
    if on_path:
        return Path(on_path).resolve()

    repository = Path(__file__).resolve().parents[2]
    directories = [
        repository,
        repository / "Debug",
        repository / "Release",
        repository / "x64" / "Debug",
        repository / "x64" / "Release",
        repository / "build",
        repository / "build" / "Debug",
        repository / "build" / "Release",
        repository / "out" / "build" / "x64-Debug",
        repository / "out" / "build" / "x64-Release",
    ]
    names = ("mngl_demo.exe", "mngl_demo", "MNGL.exe", "MNGL")
    for directory in directories:
        for name in names:
            candidate = directory / name
            if candidate.is_file():
                return candidate.resolve()

    raise FileNotFoundError(
        "Could not locate mngl_demo. Build the application, pass "
        "MNGL(executable=...), or set MNGL_DEMO_PATH."
    )


def _process_is_running(process_id: int) -> bool | None:
    """Return whether a process exists, or None when the OS check is inconclusive."""
    if process_id <= 0:
        return False

    if os.name == "nt":
        try:
            import ctypes
            from ctypes import wintypes

            kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
            open_process = kernel32.OpenProcess
            open_process.argtypes = (
                wintypes.DWORD,
                wintypes.BOOL,
                wintypes.DWORD,
            )
            open_process.restype = wintypes.HANDLE
            get_exit_code = kernel32.GetExitCodeProcess
            get_exit_code.argtypes = (
                wintypes.HANDLE,
                ctypes.POINTER(wintypes.DWORD),
            )
            get_exit_code.restype = wintypes.BOOL
            close_handle = kernel32.CloseHandle
            close_handle.argtypes = (wintypes.HANDLE,)
            close_handle.restype = wintypes.BOOL
        except (AttributeError, OSError):
            return None

        handle = open_process(0x1000, False, process_id)  # QUERY_LIMITED_INFORMATION
        if not handle:
            error = ctypes.get_last_error()
            if error == 5:  # Access denied still means the PID exists.
                return True
            if error == 87:  # Invalid PID.
                return False
            return None

        try:
            exit_code = wintypes.DWORD()
            if not get_exit_code(handle, ctypes.byref(exit_code)):
                return None
            return exit_code.value == 259  # STILL_ACTIVE
        finally:
            close_handle(handle)

    try:
        os.kill(process_id, 0)
    except ProcessLookupError:
        return False
    except PermissionError:
        return True
    except OSError:
        return None
    return True


class MNGL:
    """Launch one-shot commands or open a persistent command session.

    ``executable`` may be omitted when ``MNGL_DEMO_PATH`` is set or the demo
    executable is in PATH / a common build directory in this repository.
    """

    def __init__(
        self,
        executable: str | os.PathLike[str] | None = None,
        *,
        cwd: str | os.PathLike[str] | None = None,
        timeout: float = 30.0,
    ) -> None:
        self.executable = _resolve_executable(executable)
        self.cwd = Path(cwd) if cwd is not None else None
        self.timeout = timeout

    def run(self, command: str, *arguments: Any, check: bool = False) -> CommandResult:
        """Run a single command in a fresh process.

        Use :meth:`session` when commands need to share tree IDs or selection.
        """
        tokens = [command, *(_stringify(argument) for argument in arguments)]
        rendered = " ".join(tokens)
        completed = subprocess.run(
            [str(self.executable), "--json", *tokens],
            cwd=self.cwd,
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=self.timeout,
            check=False,
        )
        response_line = completed.stdout.strip().splitlines()
        if not response_line:
            raise MNGLProtocolError(
                f"MNGL produced no JSON response for {rendered!r}; "
                f"process exit code was {completed.returncode}. "
                f"stderr: {completed.stderr.strip()}"
            )
        result = _read_result(response_line[-1], rendered)
        return result.require_success() if check else result

    def session(self) -> TreeSession:
        """Start a process that keeps open-tree IDs valid across commands."""
        return TreeSession(self)

    def connect_gui(self, *, session_id: str | None = None) -> GuiSession:
        """Connect to a running MNGL window and operate on its open tree tabs."""
        return GuiSession(self, session_id=session_id)

    def gui(self, *, session_id: str | None = None) -> GuiSession:
        """Alias for :meth:`connect_gui`."""
        return self.connect_gui(session_id=session_id)


class TreeSession:
    """A persistent ``--json --console`` process with convenience methods."""

    def __init__(self, client: MNGL):
        self.client = client
        self._process = subprocess.Popen(
            [str(client.executable), "--json", "--console"],
            cwd=client.cwd,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            encoding="utf-8",
            errors="replace",
            bufsize=1,
        )
        assert self._process.stdin is not None
        assert self._process.stdout is not None
        assert self._process.stderr is not None
        self._responses: queue.Queue[str | None] = queue.Queue()
        self._stderr_lines: list[str] = []
        self._lock = threading.Lock()
        self._closed = False
        self._stdout_thread = threading.Thread(
            target=self._read_stdout, daemon=True, name="mngl-cli-stdout"
        )
        self._stderr_thread = threading.Thread(
            target=self._read_stderr, daemon=True, name="mngl-cli-stderr"
        )
        self._stdout_thread.start()
        self._stderr_thread.start()

    def _read_stdout(self) -> None:
        assert self._process.stdout is not None
        try:
            for line in self._process.stdout:
                self._responses.put(line)
        finally:
            self._responses.put(None)

    def _read_stderr(self) -> None:
        assert self._process.stderr is not None
        for line in self._process.stderr:
            self._stderr_lines.append(line)

    @staticmethod
    def _quote_token(value: str) -> str:
        if any(character in value for character in "\r\n\0"):
            raise ValueError("Command arguments cannot contain newlines or NUL")
        return '"' + value.replace("\\", "\\\\").replace('"', '\\"') + '"'

    def run(self, command: str, *arguments: Any, check: bool = False) -> CommandResult:
        """Run a command in the current process; arguments are safely quoted."""
        if self._closed:
            raise RuntimeError("MNGL command session is closed")
        tokens = [command, *(_stringify(argument) for argument in arguments)]
        rendered = " ".join(tokens)
        line = " ".join(self._quote_token(token) for token in tokens)

        with self._lock:
            if self._process.poll() is not None:
                raise RuntimeError(self._process_error("MNGL command process exited"))
            assert self._process.stdin is not None
            try:
                self._process.stdin.write(line + "\n")
                self._process.stdin.flush()
            except (BrokenPipeError, OSError) as error:
                raise RuntimeError(self._process_error("Cannot write to MNGL process")) from error

            try:
                response = self._responses.get(timeout=self.client.timeout)
            except queue.Empty as error:
                self.close()
                raise TimeoutError(
                    f"Timed out waiting for MNGL response to {rendered!r}"
                ) from error
            if response is None:
                raise RuntimeError(self._process_error("MNGL closed its output stream"))

            result = _read_result(response.strip(), rendered)
        return result.require_success() if check else result

    def _process_error(self, message: str) -> str:
        returncode = self._process.poll()
        details = "".join(self._stderr_lines).strip()
        suffix = f" (exit code {returncode})" if returncode is not None else ""
        return f"{message}{suffix}" + (f": {details}" if details else "")

    def types(self) -> CommandResult:
        return self.run("tree.types")

    def list_trees(self) -> CommandResult:
        return self.run("tree.list")

    def files(self, directory: str | os.PathLike[str] = ".") -> CommandResult:
        return self.run("tree.files", directory)

    def create_tree(
        self,
        algorithm_id: str,
        file: str | os.PathLike[str],
        *,
        force: bool = False,
    ) -> Tree:
        arguments: list[Any] = [algorithm_id, file]
        if force:
            arguments.append("--force")
        result = self.run("tree.create", *arguments, check=True)
        tree_id = result.value("tree_id")
        if not tree_id:
            raise MNGLProtocolError("tree.create succeeded without returning tree_id")
        return Tree(self, tree_id, Path(file), algorithm_id)

    def open_tree(self, file: str | os.PathLike[str]) -> Tree:
        result = self.run("tree.open", file, check=True)
        tree_id = result.value("tree_id")
        algorithm_id = result.value("algorithm_id")
        if not tree_id or not algorithm_id:
            raise MNGLProtocolError("tree.open succeeded without tree metadata")
        return Tree(self, tree_id, Path(file), algorithm_id)

    def select_tree(self, tree_id_or_file: str | os.PathLike[str]) -> CommandResult:
        return self.run("tree.select", tree_id_or_file, check=True)

    def __enter__(self) -> TreeSession:
        return self

    def __exit__(self, exc_type: Any, exc_value: Any, traceback: Any) -> None:
        self.close()

    def close(self) -> None:
        if self._closed:
            return
        self._closed = True
        if self._process.poll() is None:
            try:
                if self._process.stdin is not None:
                    self._process.stdin.write(":quit\n")
                    self._process.stdin.flush()
                    self._process.stdin.close()
                self._process.wait(timeout=min(self.client.timeout, 2.0))
            except (BrokenPipeError, OSError, subprocess.TimeoutExpired):
                if self._process.poll() is None:
                    self._process.terminate()
                try:
                    self._process.wait(timeout=1.0)
                except subprocess.TimeoutExpired:
                    self._process.kill()
                    self._process.wait()
        self._stdout_thread.join(timeout=0.2)
        self._stderr_thread.join(timeout=0.2)


class GuiSession:
    """A connection to a running MNGL GUI's live tree workspace.

    Commands are handled by the GUI thread and return after the edited tree has
    been rendered. Animated operations return after their animation completes.
    """

    def __init__(self, client: MNGL, *, session_id: str | None = None):
        self.client = client
        self.directory, self.session_id, self._token = self._find_session(
            session_id
        )
        self._lock = threading.Lock()
        self._closed = False

    @staticmethod
    def _read_fields(path: Path) -> dict[str, str]:
        fields: dict[str, str] = {}
        for line in path.read_text(encoding="ascii").splitlines():
            name, separator, value = line.partition("=")
            if separator and name:
                fields[name] = value
        return fields

    @classmethod
    def _find_session(
        cls, session_id: str | None
    ) -> tuple[Path, str, str]:
        temp_root = Path(tempfile.gettempdir())
        candidates: list[tuple[Path, str, str]] = []
        legacy_candidates: list[tuple[Path, str, str]] = []
        saw_pid_metadata = False
        for directory in temp_root.glob("mngl-live-*"):
            metadata = directory / "session.meta"
            if not metadata.is_file():
                continue
            try:
                fields = cls._read_fields(metadata)
            except (OSError, UnicodeError):
                continue
            if fields.get("protocol") != "1":
                continue
            identifier = fields.get(
                "session_id", directory.name.removeprefix("mngl-live-")
            )
            token = fields.get("token", "")
            if not token:
                continue
            candidate = (directory, identifier, token)
            if session_id is not None and identifier != session_id:
                continue

            process_id_text = fields.get("process_id")
            if process_id_text is None:
                legacy_candidates.append(candidate)
                continue
            saw_pid_metadata = True
            try:
                process_id = int(process_id_text)
            except ValueError:
                continue
            if _process_is_running(process_id) is not False:
                candidates.append(candidate)

        has_process_checked_sessions = bool(candidates)
        # Older MNGL builds did not store a PID. Prefer sessions from builds
        # that can be checked so crash leftovers from those older builds don't
        # make a single current window look ambiguous.
        if not candidates and not saw_pid_metadata:
            candidates = legacy_candidates

        if not candidates:
            detail = (
                f" with session ID {session_id!r}" if session_id else ""
            )
            raise ConnectionError(
                "No running MNGL GUI session was found"
                f"{detail}. Launch MNGL, open a tree tab, and try again."
            )
        if len(candidates) > 1:
            identifiers = ", ".join(item[1] for item in candidates)
            if not has_process_checked_sessions:
                raise ConnectionError(
                    "Found multiple sessions from an older MNGL build, which "
                    "cannot distinguish live windows from crash leftovers. "
                    "Rebuild and restart MNGL to enable stale-session detection, "
                    "or choose one with session_id=...: "
                    f"{identifiers}"
                )
            raise ConnectionError(
                "More than one MNGL GUI session is running. Pass one of these "
                f"IDs to connect_gui(session_id=...): {identifiers}"
            )
        return candidates[0]

    @staticmethod
    def _encode(value: str) -> str:
        return value.encode("utf-8").hex()

    def tree(
        self,
        reference: str | os.PathLike[str] | Tree | GuiTree,
    ) -> GuiTree:
        """Create a handle for a tree in the current workspace."""
        if isinstance(reference, GuiTree):
            if reference.session is not self:
                raise ValueError("The tree belongs to another MNGL GUI session")
            return reference
        if isinstance(reference, Tree):
            path = reference.file
            if not path.is_absolute():
                base = reference.session.client.cwd or Path.cwd()
                path = base / path
            reference = path
        if isinstance(reference, os.PathLike):
            value = str(Path(reference).expanduser().resolve())
        else:
            value = str(reference)
            if not value.startswith("archive:"):
                value = str(Path(value).expanduser().resolve())
        return GuiTree(self, value)

    def selected_tree(self) -> GuiTree | None:
        """Return the active tree tab, or ``None`` when no tree is selected."""
        trees = self._tree_inventory("tree.selected")
        return trees[0] if trees else None

    def opened_trees(self) -> list[GuiTree]:
        """Return the tree tabs currently open in the MNGL workspace."""
        return self._tree_inventory("trees.opened")

    def loaded_trees(self) -> list[GuiTree]:
        """Return every tree file in the workspace, including closed tabs."""
        return self._tree_inventory("trees.loaded")

    def _tree_inventory(self, operation: str) -> list[GuiTree]:
        result = self.run(operation, "")
        fields = result.fields
        try:
            count = int(fields["tree_count"])
        except (KeyError, TypeError, ValueError) as error:
            raise MNGLProtocolError(
                f"MNGL returned an invalid {operation} response"
            ) from error
        if count < 0 or count > 10_000:
            raise MNGLProtocolError(
                f"MNGL returned an unreasonable tree count for {operation}"
            )

        def text_field(name: str) -> str:
            try:
                value = bytes.fromhex(fields[name]).decode("utf-8")
            except (KeyError, UnicodeError, ValueError) as error:
                raise MNGLProtocolError(
                    f"MNGL returned an invalid {name!r} tree field"
                ) from error
            return value

        trees: list[GuiTree] = []
        for index in range(count):
            prefix = f"tree_{index}_"
            reference = text_field(prefix + "reference_hex")
            if not reference.startswith("archive:"):
                raise MNGLProtocolError(
                    "MNGL returned a tree reference outside the workspace"
                )
            trees.append(
                GuiTree(
                    self,
                    reference,
                    name=text_field(prefix + "name_hex"),
                    algorithm_id=text_field(prefix + "algorithm_hex"),
                    is_open=_boolean_field(result, prefix + "is_open"),
                    is_selected=_boolean_field(result, prefix + "is_selected"),
                    is_dirty=_boolean_field(result, prefix + "is_dirty"),
                )
            )
        return trees

    def run(
        self,
        operation: str,
        tree_reference: str = "",
        key: Any = None,
        *,
        check: bool = True,
    ) -> CommandResult:
        """Send one tree command or workspace query to the live MNGL window."""
        inventory_operations = {
            "tree.selected",
            "trees.opened",
            "trees.loaded",
        }
        if operation not in inventory_operations | {"find", "insert", "erase", "select"}:
            raise ValueError(
                "GUI operations are find, insert, erase, select, or a tree inventory query"
            )
        if self._closed:
            raise RuntimeError("MNGL GUI connection is closed")
        if operation in inventory_operations:
            if tree_reference or key is not None:
                raise ValueError("Tree inventory queries do not take a tree or key")
            key_text = ""
        elif operation == "select":
            if not tree_reference:
                raise ValueError("select needs a tree reference")
            key_text = ""
        else:
            if not tree_reference:
                raise ValueError(f"{operation} needs a tree reference")
            key_text = _stringify(key)
        request_id = uuid.uuid4().hex
        command = f"gui.{operation} {tree_reference} {key_text}".strip()
        request_path = self.directory / "request.pending"
        response_path = self.directory / f"response-{request_id}.ready"
        request_temp = self.directory / f"request-{request_id}.tmp"
        contents = (
            "protocol=1\n"
            f"request_id={request_id}\n"
            f"token={self._token}\n"
            f"operation={operation}\n"
            f"tree_reference_hex={self._encode(tree_reference)}\n"
            f"key_hex={self._encode(key_text)}\n"
        )

        with self._lock:
            if self._closed:
                raise RuntimeError("MNGL GUI connection is closed")
            with _exclusive_gui_request_slot(self.directory):
                try:
                    if request_path.exists():
                        raise RuntimeError(
                            "MNGL already has a pending live Python command"
                        )
                    with request_temp.open(
                        "x",
                        encoding="ascii",
                        newline="\n",
                    ) as stream:
                        stream.write(contents)
                        stream.flush()
                    os.replace(request_temp, request_path)
                except FileExistsError as error:
                    request_temp.unlink(missing_ok=True)
                    raise RuntimeError(
                        "MNGL already has a pending live Python command"
                    ) from error

                deadline = time.monotonic() + self.client.timeout
                while not response_path.is_file():
                    if time.monotonic() >= deadline:
                        self._closed = True
                        request_temp.unlink(missing_ok=True)
                        raise TimeoutError(
                            "Timed out waiting for the MNGL GUI to finish "
                            f"{command!r}"
                        )
                    time.sleep(0.01)

                try:
                    # On Windows, an antivirus or indexer can briefly lock a newly
                    # published response file between is_file() and read_text().
                    read_deadline = min(
                        deadline,
                        time.monotonic() + 1.0,
                    )
                    while True:
                        try:
                            fields = self._read_fields(response_path)
                            break
                        except PermissionError:
                            if time.monotonic() >= read_deadline:
                                raise
                            time.sleep(0.01)

                    if fields.get("protocol") != "1":
                        raise MNGLProtocolError(
                            "MNGL GUI returned an unsupported live protocol version"
                        )
                    if fields.get("request_id") != request_id:
                        raise MNGLProtocolError(
                            "MNGL GUI response does not match the pending request"
                        )
                    if fields.get("token") != self._token:
                        raise MNGLProtocolError(
                            "MNGL GUI response has an invalid session token"
                        )
                    result = CommandResult(
                        command=command,
                        exit_code=int(fields["exit_code"]),
                        stdout=bytes.fromhex(
                            fields.get("stdout_hex", "")
                        ).decode("utf-8", errors="replace"),
                        stderr=bytes.fromhex(
                            fields.get("stderr_hex", "")
                        ).decode("utf-8", errors="replace"),
                    )
                except PermissionError as error:
                    raise MNGLProtocolError(
                        f"MNGL GUI response for {command!r} stayed locked "
                        "after retrying"
                    ) from error
                except (OSError, KeyError, TypeError, ValueError) as error:
                    raise MNGLProtocolError(
                        f"MNGL GUI returned a malformed response for {command!r}"
                    ) from error
                finally:
                    try:
                        response_path.unlink(missing_ok=True)
                    except OSError:
                        # The response has already been consumed; a stale temp
                        # file is removed when the MNGL process exits.
                        pass

        return result.require_success() if check else result

    def __enter__(self) -> GuiSession:
        return self

    def __exit__(self, exc_type: Any, exc_value: Any, traceback: Any) -> None:
        self.close()

    def close(self) -> None:
        self._closed = True


@dataclass
class GuiTree:
    """A friendly handle for one tree in the live MNGL workspace."""

    session: GuiSession
    reference: str
    name: str = ""
    algorithm_id: str = ""
    is_open: bool | None = None
    is_selected: bool = False
    is_dirty: bool = False

    @property
    def workspace_path(self) -> str:
        """Path inside the current workspace, without the ``archive:`` prefix."""
        return self.reference.removeprefix("archive:")

    def select(self) -> GuiTree:
        """Make this tree the active tab; a closed workspace tree is opened."""
        self.session.run("select", self.reference)
        self.is_open = True
        self.is_selected = True
        return self

    def find(self, key: Any) -> CommandResult:
        return self._run_tree_operation("find", key)

    def insert(self, key: Any) -> CommandResult:
        return self._run_tree_operation("insert", key)

    def erase(self, key: Any) -> CommandResult:
        return self._run_tree_operation("erase", key)

    def _run_tree_operation(self, operation: str, key: Any) -> CommandResult:
        result = self.session.run(
            operation,
            self.reference,
            _format_tree_key(key),
        )
        self.is_open = True
        self.is_selected = True
        if result.value("changed") == "true":
            self.is_dirty = True
        return result

    def contains(self, key: Any) -> bool:
        """Return ``True`` when the tree contains ``key``."""
        return _boolean_field(self.find(key), "found")

    def __contains__(self, key: object) -> bool:
        return self.contains(key)

    def add(self, key: Any) -> bool:
        """Insert ``key`` and return whether the tree changed."""
        return _boolean_field(self.insert(key), "changed")

    def remove(self, key: Any) -> bool:
        """Remove ``key`` and return whether the tree changed."""
        return _boolean_field(self.erase(key), "changed")

    def __repr__(self) -> str:
        label = self.name or self.workspace_path or self.reference
        algorithm = self.algorithm_id or "unknown algorithm"
        return (
            f"<MNGL tree {label!r} algorithm={algorithm!r} "
            f"open={self.is_open} selected={self.is_selected} dirty={self.is_dirty}>"
        )


@dataclass
class Tree:
    """A handle to a tree opened in one :class:`TreeSession`."""

    session: TreeSession
    tree_id: str
    file: Path
    algorithm_id: str
    closed: bool = False

    @property
    def target(self) -> str:
        return self.tree_id

    def _ensure_open(self) -> None:
        if self.closed:
            raise RuntimeError(f"Tree handle {self.tree_id!r} is closed")

    def select(self) -> CommandResult:
        self._ensure_open()
        return self.session.select_tree(self.tree_id)

    def status(self) -> CommandResult:
        self.select()
        return self.session.run("tree.status", check=True)

    def get(self) -> CommandResult:
        self.select()
        return self.session.run("tree.get.selected", check=True)

    def find(self, key: Any) -> CommandResult:
        self._ensure_open()
        return self.session.run("tree.find", self.target, key, check=True)

    def insert(self, key: Any) -> CommandResult:
        self._ensure_open()
        return self.session.run("tree.insert", self.target, key, check=True)

    def erase(self, key: Any) -> CommandResult:
        self._ensure_open()
        return self.session.run("tree.erase", self.target, key, check=True)

    def insert_random(self, count: int, *, seed: int | None = None) -> CommandResult:
        self._ensure_open()
        if seed is None:
            return self.session.run("tree.insert-random", self.target, count, check=True)
        return self.session.run(
            "tree.insert-random.seeded", self.target, count, seed, check=True
        )

    def invariants(self) -> CommandResult:
        self._ensure_open()
        return self.session.run("tree.invariants", self.target, check=True)

    def node(self, index: int) -> CommandResult:
        self._ensure_open()
        return self.session.run("tree.node", self.target, index, check=True)

    def commands(self) -> CommandResult:
        self._ensure_open()
        return self.session.run("tree.commands", self.target, check=True)

    def custom(self, command_id: str, *arguments: Any) -> CommandResult:
        self._ensure_open()
        encoded = json.dumps(arguments, ensure_ascii=False, separators=(",", ":"))
        return self.session.run(
            "tree.custom", self.target, command_id, encoded, check=True
        )

    def merge(self, source: Tree | str | os.PathLike[str]) -> CommandResult:
        self._ensure_open()
        if isinstance(source, Tree) and source.session is not self.session:
            raise ValueError("Both trees must belong to the same CLI session")
        source_target = source.target if isinstance(source, Tree) else source
        return self.session.run("tree.merge", self.target, source_target, check=True)

    def split(
        self,
        pivot: Any,
        left_file: str | os.PathLike[str],
        right_file: str | os.PathLike[str],
    ) -> tuple[Tree, Tree]:
        self._ensure_open()
        result = self.session.run(
            "tree.split", self.target, pivot, left_file, right_file, check=True
        )
        left_id = result.value("left_tree_id")
        right_id = result.value("right_tree_id")
        if not left_id or not right_id:
            raise MNGLProtocolError("tree.split succeeded without both tree IDs")
        return (
            Tree(self.session, left_id, Path(left_file), self.algorithm_id),
            Tree(self.session, right_id, Path(right_file), self.algorithm_id),
        )

    def save_as(self, destination: str | os.PathLike[str]) -> Tree:
        self._ensure_open()
        result = self.session.run("tree.save-as", self.target, destination, check=True)
        tree_id = result.value("tree_id")
        if not tree_id:
            raise MNGLProtocolError("tree.save-as succeeded without returning tree_id")
        return Tree(self.session, tree_id, Path(destination), self.algorithm_id)

    def rename(self, destination: str | os.PathLike[str]) -> CommandResult:
        self._ensure_open()
        result = self.session.run("tree.rename", self.target, destination, check=True)
        self.file = Path(destination)
        return result

    def delete(self) -> CommandResult:
        self._ensure_open()
        result = self.session.run("tree.delete", self.target, check=True)
        self.closed = True
        return result

    def close(self) -> CommandResult:
        self._ensure_open()
        result = self.session.run("tree.close", self.target, check=True)
        self.closed = True
        return result


def connect_gui(
    executable: str | os.PathLike[str] | None = None,
    *,
    cwd: str | os.PathLike[str] | None = None,
    timeout: float = 30.0,
    session_id: str | None = None,
) -> GuiSession:
    """Connect to a running GUI for simple scripts and interactive use.

    Example::

        with connect_gui() as MNGL:
            tree = MNGL.selected_tree()
            if tree is not None:
                print(42 in tree)
    """
    client = MNGL(executable, cwd=cwd, timeout=timeout)
    return client.connect_gui(session_id=session_id)
