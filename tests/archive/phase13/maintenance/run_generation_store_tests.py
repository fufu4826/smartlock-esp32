#!/usr/bin/env python3
"""Compile production GenerationStore/AtomicFileStore/ConfigStore with host fakes."""
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
    sources = [TESTS / "generation_store_test.cpp", TESTS / "generation_store_test_fakes.cpp",
               ROOT / "src" / "storage" / "GenerationStore.cpp",
               ROOT / "src" / "storage" / "AtomicFileStore.cpp",
               ROOT / "src" / "storage" / "ConfigStore.cpp",
               ROOT / "src" / "storage" / "RecordCodec.cpp"]
    include_dirs = [TESTS / "mocks", ROOT / "tests" / "identity" / "mocks", TESTS,
                    ROOT / "src" / "storage", ROOT / "src" / "security", ROOT / "src" / "maintenance"]
    with tempfile.TemporaryDirectory(prefix="generation-store-tests-") as temp_name:
        temp = Path(temp_name)
        output = temp / ("generation_store_tests.exe" if os.name == "nt" else "generation_store_tests")
        if compiler:
            command = [compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror",
                       "-include", str(ROOT / "tests" / "identity" / "mocks" / "Arduino.h"),
                       *(f"-I{directory}" for directory in include_dirs),
                       *(str(source) for source in sources), "-o", str(output)]
            subprocess.run(command, cwd=ROOT, check=True)
        else:
            msvc = find_msvc_environment()
            if not msvc:
                print("No host C++ compiler found (tried CXX, g++, clang++, and Visual Studio).", file=sys.stderr)
                return 2
            build = temp / "build_tests.cmd"
            includes = " ".join(f'/I"{directory}"' for directory in include_dirs)
            inputs = " ".join(f'"{source}"' for source in sources)
            build.write_text(f'call "{msvc}" -arch=x64 -host_arch=x64\n'
                             "if errorlevel 1 exit /b %errorlevel%\n"
                             f'cl.exe /nologo /std:c++17 /EHsc /W4 /WX /utf-8 /wd4100 /wd4996 /wd4267 '
                             f'/FI"{ROOT / "tests" / "identity" / "mocks" / "Arduino.h"}" '
                             f'{includes} {inputs} /Fe:"{output}"\n', encoding="utf-8")
            windows = Path(os.environ.get("SystemRoot", r"C:\Windows"))
            env = os.environ.copy()
            env["PATH"] = ";".join(str(windows / suffix) for suffix in
                                    ("System32", "System32\\Wbem", "System32\\WindowsPowerShell\\v1.0"))
            subprocess.run(["cmd.exe", "/d", "/c", str(build)], cwd=temp, check=True, env=env)
        subprocess.run([str(output)], cwd=ROOT, check=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
