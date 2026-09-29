#!/usr/bin/env python3
"""Run the production registration recovery method with isolated storage fakes."""
from __future__ import annotations
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
TESTS = ROOT / "tests" / "registration"

def find_msvc_environment() -> Path | None:
    program_files = Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)"))
    vswhere = program_files / "Microsoft Visual Studio" / "Installer" / "vswhere.exe"
    if not vswhere.exists():
        return None
    found = subprocess.run([str(vswhere), "-latest", "-products", "*", "-property", "installationPath"],
                           capture_output=True, text=True, check=False)
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
    include_dirs = [TESTS / "mocks", TESTS, ROOT / "src" / "app", ROOT / "src" / "storage", ROOT / "src" / "security"]
    with tempfile.TemporaryDirectory(prefix="registration-recovery-") as temporary:
        temporary = Path(temporary)
        environment_script = find_msvc_environment() if not compiler else None
        if not compiler and not environment_script:
            print("No host C++ compiler found (tried CXX, g++, clang++, and Visual Studio).", file=sys.stderr)
            return 2

        def build_and_run(sources: list[Path], name: str) -> None:
            output = temporary / (name + (".exe" if os.name == "nt" else ""))
            if compiler:
                command = [compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror",
                           *(f"-I{directory}" for directory in include_dirs),
                           *(str(source) for source in sources), "-o", str(output)]
                subprocess.run(command, cwd=ROOT, check=True)
            else:
                includes = [f'/I"{directory}"' for directory in include_dirs]
                command_text = " ".join(["cl.exe", "/nologo", "/std:c++17", "/EHsc", "/W4", "/WX",
                    "/wd4100", "/wd4458", "/wd4996", *includes,
                    *(f'"{source}"' for source in sources), f'/Fe:"{output}"'])
                build = temporary / (name + "_build.cmd")
                build.write_text(f'call "{environment_script}" -arch=x64 -host_arch=x64\n'
                                 "if errorlevel 1 exit /b %errorlevel%\n" + command_text + "\n", encoding="utf-8")
                windows = Path(os.environ.get("SystemRoot", r"C:\Windows"))
                env = os.environ.copy()
                env["PATH"] = ";".join(str(windows / suffix) for suffix in
                    ("System32", "System32\\Wbem", "System32\\WindowsPowerShell\\v1.0"))
                subprocess.run(["cmd.exe", "/d", "/c", str(build)], cwd=temporary, check=True, env=env)
            subprocess.run([str(output)], cwd=ROOT, check=True)

        build_and_run([ROOT / "src" / "app" / "RegistrationRecovery.cpp", TESTS / "registration_recovery_test.cpp"],
                      "registration_recovery")
        auth_source = (ROOT / "src" / "storage" / "AuthStore.cpp").read_text(encoding="utf-8")
        auth_source = re.sub(r"^\s*#include\s+.*$", '#include "authstore_test_support.h"', auth_source, flags=re.MULTILINE)
        staged_auth = temporary / "authstore_production.cpp"
        staged_auth.write_text(auth_source, encoding="utf-8")
        build_and_run([staged_auth, TESTS / "authstore_lookup_test.cpp"], "authstore_lookup")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
