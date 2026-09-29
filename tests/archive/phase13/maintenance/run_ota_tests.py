#!/usr/bin/env python3
"""Compile the production OTA transfer kernel against a fault-injectable host updater."""
from __future__ import annotations

import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
TESTS = ROOT / "tests" / "maintenance"


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


def main() -> int:
    compiler_name = os.environ.get("CXX")
    compiler = shutil.which(compiler_name) if compiler_name else None
    if compiler is None:
        compiler = shutil.which("g++") or shutil.which("clang++")
    with tempfile.TemporaryDirectory(prefix="ota-transfer-tests-") as temp_name:
        temp = Path(temp_name)
        output = temp / ("ota_transfer_tests.exe" if os.name == "nt" else "ota_transfer_tests")
        source = TESTS / "ota_transfer_test.cpp"
        include = ROOT / "src" / "maintenance"
        if compiler:
            subprocess.run([compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror",
                            f"-I{include}", str(source), "-o", str(output)], cwd=ROOT, check=True)
        else:
            msvc = find_msvc_environment()
            if not msvc:
                print("No host C++ compiler found (tried CXX, g++, clang++, and Visual Studio).", file=sys.stderr)
                return 2
            build = temp / "build_tests.cmd"
            build.write_text(f'call "{msvc}" -arch=x64 -host_arch=x64\n'
                             "if errorlevel 1 exit /b %errorlevel%\n"
                             f'cl.exe /nologo /std:c++17 /EHsc /W4 /WX /I"{include}" "{source}" /Fe:"{output}"\n',
                             encoding="utf-8")
            windows = Path(os.environ.get("SystemRoot", r"C:\Windows"))
            env = os.environ.copy()
            env["PATH"] = ";".join(str(windows / suffix) for suffix in
                                   ("System32", "System32\\Wbem", "System32\\WindowsPowerShell\\v1.0"))
            subprocess.run(["cmd.exe", "/d", "/c", str(build)], cwd=temp, check=True, env=env)
        subprocess.run([str(output)], cwd=ROOT, check=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
