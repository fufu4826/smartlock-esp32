#!/usr/bin/env python3
"""Compile the production request policy and audit exclusive lock call sites."""
from __future__ import annotations

import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
TESTS = ROOT / "tests" / "security"


def find_msvc_environment() -> Path | None:
    program_files = Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)"))
    vswhere = program_files / "Microsoft Visual Studio" / "Installer" / "vswhere.exe"
    if not vswhere.exists():
        return None
    found = subprocess.run([str(vswhere), "-latest", "-products", "*", "-property", "installationPath"],
                           check=False, capture_output=True, text=True)
    if found.returncode or not found.stdout.strip():
        return None
    install = Path(found.stdout.strip())
    for relative in (Path("Common7/Tools/VsDevCmd.bat"), Path("VC/Auxiliary/Build/vcvars64.bat")):
        candidate = install / relative
        if candidate.exists():
            return candidate
    return None


def source_policy() -> None:
    sources = list((ROOT / "src").rglob("*.cpp")) + list((ROOT / "src").rglob("*.h"))
    writes = []
    unlock_calls = []
    for path in sources:
        text = path.read_text(encoding="utf-8-sig")
        for match in re.finditer(r"\bdigitalWrite\s*\(\s*(?:LockController::)?kPin\s*,", text):
            writes.append((path.relative_to(ROOT).as_posix(), match.start()))
        for match in re.finditer(r"\b(?:lock_|lock|lockController)\s*(?:\.|->)\s*unlock\s*\(", text):
            unlock_calls.append((path.relative_to(ROOT).as_posix(), match.start()))
        if re.search(r"\bdigitalWrite\s*\(\s*22\s*,", text):
            raise AssertionError(f"literal GPIO22 write found in {path.relative_to(ROOT)}")

    writer_files = {item[0] for item in writes}
    assert writer_files == {"src/hardware/LockController.cpp"}, f"GPIO22 writer files: {sorted(writer_files)}"
    assert len(writes) == 5, f"expected five controlled GPIO output writes, found {len(writes)}"
    assert len(unlock_calls) == 2, f"expected Access and physical Admin callers, found {unlock_calls}"
    assert {item[0] for item in unlock_calls} == {"src/app/AccessController.cpp","src/main.cpp"}, f"unlock call sites: {unlock_calls}"
    physical=(ROOT/"src/main.cpp").read_text(encoding="utf-8-sig")
    assert "action==PhysicalAdmin::Action::Unlock" in physical and "safeBootInitialized&&storageHealth.available()" in physical

    lock_header = (ROOT / "src" / "hardware" / "LockController.h").read_text(encoding="utf-8-sig")
    lock_source = (ROOT / "src" / "hardware" / "LockController.cpp").read_text(encoding="utf-8-sig")
    assert "kLockedLevel = HIGH" in lock_header and "kUnlockedLevel = LOW" in lock_header, \
        "GPIO22 polarity no longer declares HIGH locked and LOW unlocked"
    assert lock_source.index("digitalWrite(kPin, kLockedLevel);") < lock_source.index("pinMode(kPin, OUTPUT);"), \
        "LockController does not preload locked output before enabling the pin"

    server = (ROOT / "src" / "network" / "WebServerManager.cpp").read_text(encoding="utf-8-sig")
    assert "RequestPolicy::formContentType(server_.header(\"Content-Type\").c_str())" in server, \
        "WebServerManager does not use production exact form media type policy"
    assert re.search(r'if\s*\(\s*!strcmp\(key\s*,\s*"deviceId"\)\s*&&\s*'
                     r'!RequestPolicy::deviceId\(output\)\s*\)\s*return false\s*;', server), \
        "WebServerManager copyArg does not enforce strict IDs for deviceId fields"


def main() -> int:
    compiler_name = os.environ.get("CXX")
    compiler = shutil.which(compiler_name) if compiler_name else None
    if compiler is None:
        compiler = shutil.which("g++") or shutil.which("clang++")
    with tempfile.TemporaryDirectory(prefix="security-policy-tests-") as temp_name:
        temp = Path(temp_name)
        output = temp / ("request_policy_tests.exe" if os.name == "nt" else "request_policy_tests")
        source = TESTS / "request_policy_test.cpp"
        include = ROOT / "src" / "security"
        if compiler:
            command = [compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror",
                       f"-I{include}", str(source), "-o", str(output)]
            subprocess.run(command, check=True, cwd=ROOT)
        else:
            msvc = find_msvc_environment()
            if not msvc:
                print("No host C++ compiler found (tried CXX, g++, clang++, and Visual Studio).", file=sys.stderr)
                return 2
            cmd = temp / "build_tests.cmd"
            cmd.write_text(f'call "{msvc}" -arch=x64 -host_arch=x64\n'
                           "if errorlevel 1 exit /b %errorlevel%\n"
                           f'cl.exe /nologo /std:c++17 /EHsc /W4 /WX /I"{include}" "{source}" /Fe:"{output}"\n',
                           encoding="utf-8")
            windows = Path(os.environ.get("SystemRoot", r"C:\Windows"))
            env = os.environ.copy()
            env["PATH"] = ";".join(str(windows / suffix) for suffix in
                                   ("System32", "System32\\Wbem", "System32\\WindowsPowerShell\\v1.0"))
            subprocess.run(["cmd.exe", "/d", "/c", str(cmd)], cwd=temp, check=True, env=env)
        subprocess.run([str(output)], cwd=ROOT, check=True)
    source_policy()
    print("PASS: source policy confirms LockController GPIO ownership and AccessController-only unlock call")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
