#!/usr/bin/env python3
"""Compile EventLog.cpp against an isolated in-memory SD fake."""

from __future__ import annotations

import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parents[2]
TESTS = ROOT / "tests" / "events"
MOCKS = TESTS / "mocks"


def find_msvc_environment() -> Path | None:
    vswhere = Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)"))
    vswhere = vswhere / "Microsoft Visual Studio" / "Installer" / "vswhere.exe"
    if not vswhere.exists():
        return None
    found = subprocess.run(
        [str(vswhere), "-latest", "-products", "*", "-property", "installationPath"],
        check=False,
        capture_output=True,
        text=True,
    )
    if found.returncode != 0 or not found.stdout.strip():
        return None
    install = Path(found.stdout.strip())
    for relative in (
        Path("Common7/Tools/VsDevCmd.bat"),
        Path("VC/Auxiliary/Build/vcvars64.bat"),
    ):
        candidate = install / relative
        if candidate.exists():
            return candidate
    return None


def main() -> int:
    compiler = shutil.which(os.environ.get("CXX", "")) if os.environ.get("CXX") else None
    if compiler is None:
        compiler = shutil.which("g++") or shutil.which("clang++")
    with tempfile.TemporaryDirectory(prefix="event-log-tests-") as temp:
        executable_suffix = ".exe" if os.name == "nt" else ""
        cases = [
            ("event_log_tests", [ROOT / "src" / "events" / "EventLog.cpp", TESTS / "event_log_test.cpp"]),
            ("audit_tests", [ROOT / "src" / "events" / "EventLog.cpp", ROOT / "src" / "events" / "Audit.cpp", TESTS / "audit_test.cpp"]),
            ("installation_metadata_tests", [ROOT / "src" / "storage" / "InstallationMetadata.cpp", TESTS / "installation_metadata_test.cpp"]),
        ]
        includes = [MOCKS, ROOT / "src" / "events", ROOT / "src" / "storage"]
        if compiler is None:
            msvc_env = find_msvc_environment()
            if not msvc_env:
                print("No host C++ compiler found (tried CXX, g++, clang++, and Visual Studio).", file=sys.stderr)
                return 2
            windows = Path(os.environ.get("SystemRoot", r"C:\Windows"))
            build_env = os.environ.copy()
            build_env["PATH"] = ";".join(
                str(windows / suffix)
                for suffix in (
                    "System32",
                    "System32\\Wbem",
                    "System32\\WindowsPowerShell\\v1.0",
                )
            )
        for name, sources in cases:
            output = Path(temp) / (name + executable_suffix)
            if compiler:
                command = [compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror",
                           *(f"-I{path}" for path in includes), *(str(source) for source in sources),
                           "-o", str(output)]
                subprocess.run(command, cwd=ROOT, check=True)
            else:
                include_args = [f'/I"{path}"' for path in includes]
                command_text = " ".join(["cl.exe", "/nologo", "/std:c++17", "/EHsc", "/W4", "/WX",
                    *include_args, *(f'"{source}"' for source in sources), f'/Fe:"{output}"'])
                build_cmd = Path(temp) / f"build_{name}.cmd"
                build_cmd.write_text(f'call "{msvc_env}" -arch=x64 -host_arch=x64\n'
                    "if errorlevel 1 exit /b %errorlevel%\n" f"{command_text}\n", encoding="utf-8")
                subprocess.run(["cmd.exe", "/d", "/c", str(build_cmd)], cwd=temp, check=True, env=build_env)
            subprocess.run([str(output)], cwd=ROOT, check=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
