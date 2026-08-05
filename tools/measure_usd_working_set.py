import argparse
import ctypes
import json
import os
from pathlib import Path
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
psapi.GetProcessMemoryInfo.argtypes = [
    ctypes.c_void_p,
    ctypes.POINTER(ProcessMemoryCountersEx),
    ctypes.c_ulong,
]
psapi.GetProcessMemoryInfo.restype = ctypes.c_int

TH32CS_SNAPPROCESS = 0x00000002
PROCESS_QUERY_INFORMATION = 0x0400
PROCESS_VM_READ = 0x0010


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


def parse_args():
    parser = argparse.ArgumentParser(
        description="Measure the Windows working set of an ost plugin view session"
    )
    parser.add_argument("--bundle", required=True)
    parser.add_argument("--with-bundle", action="append", default=[])
    parser.add_argument("--fixture", required=True)
    parser.add_argument("--mode", choices=("view", "record"), default="view")
    parser.add_argument("--renderer", choices=("Storm", "GL"), default="Storm")
    parser.add_argument("--output")
    parser.add_argument("--interval-ms", type=int, default=50)
    return parser.parse_args()


def build_command(options):
    ost = shutil.which("ost.exe") or shutil.which("ost")
    if ost is None:
        raise SystemExit("ost was not found on PATH")

    tool = "usdview.cmd" if options.mode == "view" else "usdrecord.cmd"
    if options.mode == "view":
        tool_arguments = [
            "--defaultsettings",
            "--renderer",
            options.renderer,
            "--timing",
            "--quitAfterStartup",
            options.fixture,
        ]
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
    command = build_command(options)
    with tempfile.TemporaryFile() as output_file:
        process = subprocess.Popen(
            command,
            stdout=output_file,
            stderr=subprocess.STDOUT,
        )
        baseline_total, _, _ = sample_tree(process.pid)
        peak_total = baseline_total
        peak_single = 0
        peak_count = 0
        samples = 0
        while process.poll() is None:
            total, single, count = sample_tree(process.pid)
            peak_total = max(peak_total, total)
            peak_single = max(peak_single, single)
            peak_count = max(peak_count, count)
            samples += 1
            time.sleep(options.interval_ms / 1000.0)
        process.wait()
        output_file.seek(0)
        output = output_file.read().decode(errors="replace")
        total, single, count = sample_tree(process.pid)
        peak_total = max(peak_total, total)
        peak_single = max(peak_single, single)
        peak_count = max(peak_count, count)
    result = {
        "command": command,
        "mode": options.mode,
        "renderer": options.renderer,
        "fixture": str(Path(options.fixture).resolve()),
        "returncode": process.returncode,
        "baseline_process_tree_working_set_bytes": baseline_total,
        "peak_process_tree_working_set_bytes": peak_total,
        "peak_process_working_set_bytes": peak_single,
        "peak_process_count": peak_count,
        "samples": samples,
        "child_output": output,
    }
    print(json.dumps(result, indent=2))
    return process.returncode


if __name__ == "__main__":
    sys.exit(main())