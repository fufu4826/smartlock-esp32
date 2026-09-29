#!/usr/bin/env python3
"""Compile staged copies of production Phone2 controllers against host fakes."""

from __future__ import annotations

import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parents[2]
TESTS = ROOT / "tests" / "phone2"


def find_msvc_environment() -> Path | None:
    program_files = Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)"))
    vswhere = program_files / "Microsoft Visual Studio" / "Installer" / "vswhere.exe"
    if not vswhere.exists():
        return None
    found = subprocess.run([str(vswhere), "-latest", "-products", "*", "-property", "installationPath"],
                           check=False, capture_output=True, text=True)
    if found.returncode != 0 or not found.stdout.strip():
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
    with tempfile.TemporaryDirectory(prefix="phone2-host-tests-") as temp_name:
        temp = Path(temp_name)
        staged = []
        for name in ("EnrollmentManager", "AccessController"):
            source = ROOT / "src" / "app" / f"{name}.cpp"
            text = source.read_text(encoding="utf-8")
            # The staged translation units preserve production method bodies. Only
            # include directives are redirected to the isolated host interface.
            text = re.sub(r"^\s*#include\s+.*$", '#include "phone2_test_support.h"', text,
                          flags=re.MULTILINE)
            target = temp / f"{name.lower()}_production.cpp"
            target.write_text(text, encoding="utf-8")
            staged.append(target)
            header = ROOT / "src" / "app" / f"{name}.h"
            header_text = header.read_text(encoding="utf-8")
            header_text = re.sub(r"^\s*#include\s+.*$", '#include "phone2_test_base.h"',
                                 header_text, flags=re.MULTILINE)
            (temp / f"{name}.h").write_text(header_text, encoding="utf-8")

        output = temp / ("phone2_tests.exe" if os.name == "nt" else "phone2_tests")
        include_dirs = [
            temp,
            TESTS / "mocks",
            TESTS,
            ROOT / "src" / "app",
            ROOT / "src" / "qr",
            ROOT / "src" / "storage",
            ROOT / "src" / "events",
            ROOT / "tests" / "events" / "mocks",
        ]
        sources = [*staged, TESTS / "phone2_test.cpp", TESTS / "phone2_test_fakes.cpp",
                   ROOT / "src" / "qr" / "SessionManager.cpp",
                   ROOT / "src" / "storage" / "RecordCodec.cpp"]
        if compiler:
            command = [compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror",
                       *(f"-I{directory}" for directory in include_dirs),
                       *(str(path) for path in sources), "-o", str(output)]
            subprocess.run(command, cwd=ROOT, check=True)
        else:
            msvc_env = find_msvc_environment()
            if not msvc_env:
                print("No host C++ compiler found (tried CXX, g++, clang++, and Visual Studio).", file=sys.stderr)
                return 2
            includes = [f'/I"{path}"' for path in include_dirs]
            command_text = " ".join(["cl.exe", "/nologo", "/std:c++17", "/EHsc", "/W4", "/WX", "/utf-8", "/wd4100", "/wd4996",
                                     *includes, *(f'"{source}"' for source in sources),
                                     f'/Fe:"{output}"'])
            build_cmd = temp / "build_tests.cmd"
            build_cmd.write_text(f'call "{msvc_env}" -arch=x64 -host_arch=x64\n'
                                 "if errorlevel 1 exit /b %errorlevel%\n"
                                 f"{command_text}\n", encoding="utf-8")
            windows = Path(os.environ.get("SystemRoot", r"C:\Windows"))
            build_env = os.environ.copy()
            build_env["PATH"] = ";".join(str(windows / suffix) for suffix in
                                          ("System32", "System32\\Wbem", "System32\\WindowsPowerShell\\v1.0"))
            subprocess.run(["cmd.exe", "/d", "/c", str(build_cmd)], cwd=temp,
                           check=True, env=build_env)
        subprocess.run([str(output)], cwd=ROOT, check=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
