#!/usr/bin/env python3
"""Compile unchanged OtaManager method bodies against isolated SDK/storage fakes."""
from __future__ import annotations

import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
TESTS = ROOT / "tests" / "maintenance"
SOURCE = ROOT / "src" / "maintenance" / "OtaManager.cpp"


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


def compile_tests(output: Path, temp: Path) -> None:
    staged = temp / "OtaManager.cpp"
    source = SOURCE.read_text(encoding="utf-8-sig")
    replacements = {
        '#include "../storage/SecureBackup.h"': '#include "ota_manager_mocks/SecureBackup.h"',
        '#include "../storage/RecordCodec.h"': '#include "RecordCodec.h"',
        '#include "../app/EnrollmentManager.h"': '#include "ota_manager_mocks/EnrollmentManager.h"',
    }
    for original, replacement in replacements.items():
        if source.count(original) != 1:
            raise RuntimeError(f"expected exactly one production include to stage: {original}")
        source = source.replace(original, replacement)
    staged.write_text(source, encoding="utf-8")

    include_dirs = [TESTS, TESTS / "ota_manager_mocks", ROOT / "src" / "maintenance", ROOT / "src" / "storage"]
    sources = [staged, TESTS / "ota_manager_test.cpp", ROOT / "src" / "storage" / "RecordCodec.cpp"]
    compiler_name = os.environ.get("CXX")
    compiler = shutil.which(compiler_name) if compiler_name else None
    if compiler is None:
        compiler = shutil.which("g++") or shutil.which("clang++")
    if compiler:
        subprocess.run([compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror",
                        *(f"-I{path}" for path in include_dirs), *(str(path) for path in sources),
                        "-o", str(output)], cwd=ROOT, check=True)
        return

    msvc = find_msvc_environment()
    if not msvc:
        print("No host C++ compiler found (tried CXX, g++, clang++, and Visual Studio).", file=sys.stderr)
        raise SystemExit(2)
    build = temp / "build_tests.cmd"
    # /wd4100 is for the existing RecordCodec verifyCrc parameter; /wd4267 is
    # bounded by the real inactive-partition capacity checked before start.
    command = " ".join(["cl.exe", "/nologo", "/std:c++17", "/EHsc", "/W4", "/WX",
                        "/wd4100", "/wd4267",
                        *(f'/I"{path}"' for path in include_dirs),
                        *(f'"{path}"' for path in sources), f'/Fe:"{output}"'])
    build.write_text(f'call "{msvc}" -arch=x64 -host_arch=x64\n'
                     "if errorlevel 1 exit /b %errorlevel%\n" + command + "\n", encoding="utf-8")
    windows = Path(os.environ.get("SystemRoot", r"C:\Windows"))
    env = os.environ.copy()
    env["PATH"] = ";".join(str(windows / suffix) for suffix in
                            ("System32", "System32\\Wbem", "System32\\WindowsPowerShell\\v1.0"))
    subprocess.run(["cmd.exe", "/d", "/c", str(build)], cwd=temp, check=True, env=env)


def run(output: Path, temp: Path, scenario: str, *, persistent_name: str | None = None,
        extra_env: dict[str, str] | None = None) -> None:
    env = os.environ.copy()
    if persistent_name:
        env["OTA_NVS_FILE"] = str(temp / persistent_name)
    else:
        env["OTA_NVS_FILE"] = str(temp / f"{scenario}.nvs")
    if extra_env:
        env.update(extra_env)
    subprocess.run([str(output), scenario, env["OTA_NVS_FILE"]], cwd=ROOT, check=True, env=env)


def main() -> int:
    with tempfile.TemporaryDirectory(prefix="ota-manager-tests-") as temp_name:
        temp = Path(temp_name)
        output = temp / ("ota_manager_tests.exe" if os.name == "nt" else "ota_manager_tests")
        compile_tests(output, temp)

        for scenario in ("expired-start", "revoked-start", "expired-chunk", "revoked-finish",
                         "expired-during", "revoked-during", "invalid-ticket"):
            run(output, temp, scenario)
        for scenario in ("begin-failure", "write-failure", "end-failure", "receipt-failure", "setboot-failure"):
            run(output, temp, scenario)

        for name, boot_scenario in (("successful-update.nvs", "boot"),
                                    ("changed-snapshot.nvs", "boot-changed"),
                                    ("changed-image.nvs", "boot-image-changed"),
                                    ("offline-timeout.nvs", "boot-offline")):
            run(output, temp, "success", persistent_name=name)
            run(output, temp, boot_scenario, persistent_name=name)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
