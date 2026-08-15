import argparse
import ctypes
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import time


if os.name != "nt":
    raise SystemExit("This sampler currently supports Windows only")


class ProcessEntry32(ctypes.Structure):
    _fields_ = [
        ("dwSize", ctypes.c_ulong),
        ("cntUsage", ctypes.c_ulong),
        ("th32ProcessID", ctypes.c_ulong),
        ("th32DefaultHeapID", ctypes.c_size_t),
        ("th32ModuleID", ctypes.c_ulong),
        ("cntThreads", ctypes.c_ulong),
        ("th32ParentProcessID", ctypes.c_ulong),
        ("pcPriClassBase", ctypes.c_long),
        ("dwFlags", ctypes.c_ulong),
        ("szExeFile", ctypes.c_wchar * 260),
    ]


class ProcessMemoryCountersEx(ctypes.Structure):
    _fields_ = [
        ("cb", ctypes.c_ulong),
        ("PageFaultCount", ctypes.c_ulong),
        ("PeakWorkingSetSize", ctypes.c_size_t),
        ("WorkingSetSize", ctypes.c_size_t),
        ("QuotaPeakPagedPoolUsage", ctypes.c_size_t),
        ("QuotaPagedPoolUsage", ctypes.c_size_t),
        ("QuotaPeakNonPagedPoolUsage", ctypes.c_size_t),
        ("QuotaNonPagedPoolUsage", ctypes.c_size_t),
        ("PagefileUsage", ctypes.c_size_t),
        ("PeakPagefileUsage", ctypes.c_size_t),
        ("PrivateUsage", ctypes.c_size_t),
    ]


kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
psapi = ctypes.WinDLL("psapi", use_last_error=True)
kernel32.CreateToolhelp32Snapshot.argtypes = [ctypes.c_ulong, ctypes.c_ulong]
kernel32.CreateToolhelp32Snapshot.restype = ctypes.c_void_p
kernel32.Process32FirstW.argtypes = [ctypes.c_void_p, ctypes.POINTER(ProcessEntry32)]
kernel32.Process32FirstW.restype = ctypes.c_int
kernel32.Process32NextW.argtypes = [ctypes.c_void_p, ctypes.POINTER(ProcessEntry32)]
kernel32.Process32NextW.restype = ctypes.c_int
kernel32.CloseHandle.argtypes = [ctypes.c_void_p]
kernel32.CloseHandle.restype = ctypes.c_int
kernel32.OpenProcess.argtypes = [ctypes.c_ulong, ctypes.c_int, ctypes.c_ulong]
kernel32.OpenProcess.restype = ctypes.c_void_p
user32 = ctypes.WinDLL("user32", use_last_error=True)
user32.EnumWindows.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
user32.EnumWindows.restype = ctypes.c_int
user32.IsWindowVisible.argtypes = [ctypes.c_void_p]
user32.IsWindowVisible.restype = ctypes.c_int
user32.IsWindow.argtypes = [ctypes.c_void_p]
user32.IsWindow.restype = ctypes.c_int
user32.GetWindowTextLengthW.argtypes = [ctypes.c_void_p]
user32.GetWindowTextLengthW.restype = ctypes.c_int
user32.GetWindowTextW.argtypes = [ctypes.c_void_p, ctypes.c_wchar_p, ctypes.c_int]
user32.GetWindowTextW.restype = ctypes.c_int
user32.GetWindowThreadProcessId.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_ulong)]
user32.GetWindowThreadProcessId.restype = ctypes.c_ulong
user32.ShowWindow.argtypes = [ctypes.c_void_p, ctypes.c_int]
user32.ShowWindow.restype = ctypes.c_int
user32.PostMessageW.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.c_size_t, ctypes.c_size_t]
user32.PostMessageW.restype = ctypes.c_int
user32.SendMessageTimeoutW.argtypes = [
    ctypes.c_void_p,
    ctypes.c_uint,
    ctypes.c_size_t,
    ctypes.c_size_t,
    ctypes.c_uint,
    ctypes.c_uint,
    ctypes.POINTER(ctypes.c_size_t),
]
user32.SendMessageTimeoutW.restype = ctypes.c_size_t
psapi.GetProcessMemoryInfo.argtypes = [
    ctypes.c_void_p,
    ctypes.POINTER(ProcessMemoryCountersEx),
    ctypes.c_ulong,
]
psapi.GetProcessMemoryInfo.restype = ctypes.c_int

TH32CS_SNAPPROCESS = 0x00000002
PROCESS_QUERY_INFORMATION = 0x0400
PROCESS_VM_READ = 0x0010
SW_RESTORE = 9
WM_CLOSE = 0x0010
WM_NULL = 0x0000
WM_KEYDOWN = 0x0100
WM_KEYUP = 0x0101
SMTO_ABORTIFHUNG = 0x0002
VK_HOME = 0x24
VK_LEFT = 0x25
VK_UP = 0x26
VK_RIGHT = 0x27
VK_DOWN = 0x28


INTERACTION_KEYS = (
    ("home", VK_HOME),
    ("left", VK_LEFT),
    ("right", VK_RIGHT),
    ("up", VK_UP),
    ("down", VK_DOWN),
)


def report(message):
    print(f"[measure] {message}", file=sys.stderr, flush=True)


def process_parents():
    snapshot = kernel32.CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)
    if snapshot == ctypes.c_void_p(-1).value:
        return {}
    try:
        entry = ProcessEntry32()
        entry.dwSize = ctypes.sizeof(ProcessEntry32)
        parents = {}
        if kernel32.Process32FirstW(snapshot, ctypes.byref(entry)):
            while True:
                parents[entry.th32ProcessID] = entry.th32ParentProcessID
                if not kernel32.Process32NextW(snapshot, ctypes.byref(entry)):
                    break
        return parents
    finally:
        kernel32.CloseHandle(snapshot)


def descendant_processes(root_pid):
    parents = process_parents()
    descendants = {root_pid}
    changed = True
    while changed:
        changed = False
        for process_id, parent_id in parents.items():
            if parent_id in descendants and process_id not in descendants:
                descendants.add(process_id)
                changed = True
    return descendants


def working_set(process_id):
    handle = kernel32.OpenProcess(
        PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, 0, process_id
    )
    if not handle:
        return 0
    try:
        counters = ProcessMemoryCountersEx()
        counters.cb = ctypes.sizeof(counters)
        if not psapi.GetProcessMemoryInfo(
            handle, ctypes.byref(counters), ctypes.sizeof(counters)
        ):
            return 0
        return counters.WorkingSetSize
    finally:
        kernel32.CloseHandle(handle)


def sample_tree(root_pid):
    values = [working_set(process_id) for process_id in descendant_processes(root_pid)]
    values = [value for value in values if value]
    return sum(values), max(values, default=0), len(values)


def window_title(window):
    title_length = user32.GetWindowTextLengthW(window)
    title = ctypes.create_unicode_buffer(title_length + 1)
    user32.GetWindowTextW(window, title, len(title))
    return title.value


def find_visible_window(root_pid, timeout_seconds):
    deadline = time.perf_counter() + timeout_seconds
    while time.perf_counter() < deadline:
        process_ids = descendant_processes(root_pid)
        windows = []

        @ctypes.WINFUNCTYPE(ctypes.c_int, ctypes.c_void_p, ctypes.c_void_p)
        def collect_window(window, _context):
            if not user32.IsWindowVisible(window):
                return 1
            process_id = ctypes.c_ulong()
            user32.GetWindowThreadProcessId(window, ctypes.byref(process_id))
            if process_id.value in process_ids:
                windows.append((window, bool(window_title(window).strip())))
            return 1

        user32.EnumWindows(collect_window, 0)
        if windows:
            windows.sort(key=lambda item: item[1], reverse=True)
            return windows[0][0]
        time.sleep(0.05)
    return None


def send_key(window, virtual_key, timeout_ms):
    if not user32.IsWindow(window):
        raise RuntimeError("usdview window was destroyed before input dispatch")
    started_at = time.perf_counter()
    result = ctypes.c_size_t()
    if not user32.SendMessageTimeoutW(
        window,
        WM_NULL,
        0,
        0,
        SMTO_ABORTIFHUNG,
        timeout_ms,
        ctypes.byref(result),
    ):
        error = ctypes.get_last_error()
        if error == 1460:
            raise TimeoutError(f"window dispatch timed out after {timeout_ms} ms")
        raise OSError(error, "SendMessageTimeoutW failed")
    for message, lparam in (
        (WM_KEYDOWN, 0),
        (WM_KEYUP, 0xC0000001),
    ):
        if not user32.PostMessageW(window, message, virtual_key, lparam):
            error = ctypes.get_last_error()
            raise OSError(error, "PostMessageW failed")
    return time.perf_counter() - started_at


def run_interaction(
    root_pid,
    interval_seconds,
    sample_interval_seconds,
    window_timeout_seconds,
    key_timeout_ms,
):
    window = find_visible_window(root_pid, window_timeout_seconds)
    if window is None:
        raise RuntimeError("usdview window was not found")
    if not user32.IsWindow(window):
        window = find_visible_window(root_pid, window_timeout_seconds)
    if window is None or not user32.IsWindow(window):
        raise RuntimeError("usdview window was destroyed before interaction")
    window_process_id = ctypes.c_ulong()
    user32.GetWindowThreadProcessId(window, ctypes.byref(window_process_id))
    report(
        f"using window 0x{window:x} pid {window_process_id.value} "
        f"title={window_title(window)!r}"
    )
    user32.ShowWindow(window, SW_RESTORE)

    actions = []
    samples = []
    started_at = time.perf_counter()
    for name, virtual_key in INTERACTION_KEYS:
        action_started_at = time.perf_counter()
        for attempt in range(3):
            if not user32.IsWindow(window):
                window = find_visible_window(root_pid, window_timeout_seconds)
            if window is None:
                raise RuntimeError("usdview window disappeared before input dispatch")
            try:
                dispatch_seconds = send_key(window, virtual_key, key_timeout_ms)
                break
            except RuntimeError:
                if attempt == 2:
                    raise
                time.sleep(0.05)
        actions.append(
            {
                "name": name,
                "sent_after_seconds": action_started_at - started_at,
                "dispatch_seconds": dispatch_seconds,
            }
        )
        deadline = time.perf_counter() + interval_seconds
        while time.perf_counter() < deadline:
            samples.append(sample_tree(root_pid))
            remaining = deadline - time.perf_counter()
            time.sleep(min(sample_interval_seconds, max(remaining, 0)))
    elapsed_seconds = time.perf_counter() - started_at
    return window, actions, elapsed_seconds, samples


def terminate_process_tree(process, timeout_seconds):
    if process.poll() is not None:
        return True
    try:
        result = subprocess.run(
            ["taskkill", "/PID", str(process.pid), "/T", "/F"],
            check=False,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            timeout=timeout_seconds,
        )
    except subprocess.TimeoutExpired:
        report("taskkill timed out")
        return False
    if result.returncode != 0:
        report(f"taskkill returned {result.returncode}")
    try:
        process.wait(timeout=timeout_seconds)
    except subprocess.TimeoutExpired:
        report("process tree did not exit after cleanup")
        return False
    return True


def parse_stage_open_seconds(output):
    match = re.search(r"Time to open stage .*: ([0-9]+(?:\.[0-9]+)?)s", output)
    return float(match.group(1)) if match else None


def parse_args():
    parser = argparse.ArgumentParser(
        description=(
            "Measure the Windows working set and elapsed time of an ost "
            "plugin view session"
        )
    )
    parser.add_argument("--bundle", required=True)
    parser.add_argument("--with-bundle", action="append", default=[])
    parser.add_argument("--fixture", required=True)
    parser.add_argument(
        "--mode", choices=("view", "interactive", "record"), default="view"
    )
    parser.add_argument("--renderer", choices=("Storm", "GL"), default="Storm")
    parser.add_argument("--output")
    parser.add_argument("--interval-ms", type=int, default=50)
    parser.add_argument("--interaction-interval-ms", type=int, default=500)
    parser.add_argument("--window-timeout-seconds", type=float, default=30.0)
    parser.add_argument("--max-session-seconds", type=float, default=120.0)
    parser.add_argument("--post-interaction-seconds", type=float, default=3.0)
    parser.add_argument("--key-timeout-ms", type=int, default=1000)
    parser.add_argument("--cleanup-timeout-seconds", type=float, default=5.0)
    return parser.parse_args()


def build_command(options):
    ost = shutil.which("ost.exe") or shutil.which("ost")
    if ost is None:
        raise SystemExit("ost was not found on PATH")

    tool = "usdrecord.cmd" if options.mode == "record" else "usdview.cmd"
    if options.mode in ("view", "interactive"):
        tool_arguments = [
            "--defaultsettings",
            "--renderer",
            options.renderer,
            "--timing",
            options.fixture,
        ]
        if options.mode == "view":
            tool_arguments.insert(-1, "--quitAfterStartup")
    else:
        if not options.output:
            raise SystemExit("--output is required with --mode record")
        tool_arguments = [
            "--renderer",
            options.renderer,
            options.fixture,
            options.output,
        ]

    command_line = subprocess.list2cmdline([tool, *tool_arguments])
    command = [ost, "plugin", "run", options.bundle]
    for bundle in options.with_bundle:
        command.extend(["--with", bundle])
    command.extend(["--", "cmd.exe", "/d", "/c", command_line])
    return command


def main():
    options = parse_args()
    if options.interval_ms <= 0:
        raise SystemExit("--interval-ms must be positive")
    if options.interaction_interval_ms <= 0:
        raise SystemExit("--interaction-interval-ms must be positive")
    if options.window_timeout_seconds <= 0:
        raise SystemExit("--window-timeout-seconds must be positive")
    if options.max_session_seconds <= 0:
        raise SystemExit("--max-session-seconds must be positive")
    if options.post_interaction_seconds <= 0:
        raise SystemExit("--post-interaction-seconds must be positive")
    if options.key_timeout_ms <= 0:
        raise SystemExit("--key-timeout-ms must be positive")
    if options.cleanup_timeout_seconds <= 0:
        raise SystemExit("--cleanup-timeout-seconds must be positive")
    command = build_command(options)
    with tempfile.TemporaryFile() as output_file:
        started_at = time.perf_counter()
        report("starting usdview session")
        process = subprocess.Popen(
            command,
            stdout=output_file,
            stderr=subprocess.STDOUT,
        )
        report(f"started process tree at pid {process.pid}")
        baseline_total, _, _ = sample_tree(process.pid)
        peak_total = baseline_total
        peak_single = 0
        peak_count = 0
        samples = 0
        interaction_actions = []
        interaction_elapsed_seconds = None
        interaction_window_found = None
        interaction_completed = False
        forced_cleanup = False
        if options.mode == "interactive":
            try:
                (
                    interaction_window,
                    interaction_actions,
                    interaction_elapsed_seconds,
                    interaction_samples,
                ) = run_interaction(
                    process.pid,
                    options.interaction_interval_ms / 1000.0,
                    options.interval_ms / 1000.0,
                    options.window_timeout_seconds,
                    options.key_timeout_ms,
                )
                interaction_window_found = True
                interaction_completed = True
                report(
                    f"interaction complete ({len(interaction_actions)} actions, "
                    f"{interaction_elapsed_seconds:.2f}s)"
                )
                for total, single, count in interaction_samples:
                    peak_total = max(peak_total, total)
                    peak_single = max(peak_single, single)
                    peak_count = max(peak_count, count)
                samples += len(interaction_samples)
                user32.PostMessageW(interaction_window, WM_CLOSE, 0, 0)
            except (OSError, RuntimeError, TimeoutError) as error:
                interaction_window_found = False
                report(f"interaction failed: {error}")
                terminate_process_tree(
                    process, options.cleanup_timeout_seconds
                )
                raise SystemExit(str(error))
        session_deadline = started_at + options.max_session_seconds
        if interaction_completed:
            session_deadline = min(
                session_deadline,
                time.perf_counter() + options.post_interaction_seconds,
            )
        reported_waiting = False
        cleanup_succeeded = True
        while process.poll() is None:
            if time.perf_counter() >= session_deadline:
                if interaction_completed:
                    report("usdview did not exit after interaction; cleaning up")
                else:
                    report(
                        f"session exceeded {options.max_session_seconds:.1f}s; "
                        "terminating process tree"
                    )
                forced_cleanup = True
                cleanup_succeeded = terminate_process_tree(
                    process, options.cleanup_timeout_seconds
                )
                break
            if not reported_waiting:
                report("sampling until usdview exits")
                reported_waiting = True
            total, single, count = sample_tree(process.pid)
            peak_total = max(peak_total, total)
            peak_single = max(peak_single, single)
            peak_count = max(peak_count, count)
            samples += 1
            time.sleep(options.interval_ms / 1000.0)
        process_returncode = process.returncode
        if process_returncode is None:
            report("measurement process remains after cleanup")
        elapsed_seconds = time.perf_counter() - started_at
        output_file.seek(0)
        output = output_file.read().decode(errors="replace")
        stage_open_seconds = parse_stage_open_seconds(output)
        total, single, count = sample_tree(process.pid)
        peak_total = max(peak_total, total)
        peak_single = max(peak_single, single)
        peak_count = max(peak_count, count)
    result = {
        "command": command,
        "mode": options.mode,
        "renderer": options.renderer,
        "fixture": str(Path(options.fixture).resolve()),
        "returncode": (
            0
            if forced_cleanup and interaction_completed and cleanup_succeeded
            else process_returncode if process_returncode is not None else 1
        ),
        "process_returncode": process_returncode,
        "elapsed_seconds": elapsed_seconds,
        "stage_open_seconds": stage_open_seconds,
        "baseline_process_tree_working_set_bytes": baseline_total,
        "peak_process_tree_working_set_bytes": peak_total,
        "peak_process_working_set_bytes": peak_single,
        "peak_process_count": peak_count,
        "samples": samples,
        "interaction_window_found": interaction_window_found,
        "interaction_actions": interaction_actions,
        "interaction_elapsed_seconds": interaction_elapsed_seconds,
        "max_input_dispatch_seconds": max(
            (action["dispatch_seconds"] for action in interaction_actions),
            default=0.0,
        ),
        "forced_cleanup": forced_cleanup,
        "cleanup_succeeded": cleanup_succeeded,
        "child_output": output,
    }
    print(json.dumps(result, indent=2))
    return result["returncode"]


if __name__ == "__main__":
    sys.exit(main())