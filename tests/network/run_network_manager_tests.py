#!/usr/bin/env python3
"""Compile the production NetworkManager.cpp against isolated host fakes."""

from __future__ import annotations

import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parents[2]
TESTS = ROOT / "tests" / "network"
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


def sources() -> list[Path]:
    return [
        ROOT / "src" / "network" / "NetworkManager.cpp",
        TESTS / "network_manager_test.cpp",
        TESTS / "network_manager_test_fakes.cpp",
    ]


def main() -> int:
    with tempfile.TemporaryDirectory(prefix="network-manager-tests-") as temp:
        output = Path(temp) / ("network_manager_tests.exe" if os.name == "nt" else "network_manager_tests")
        common_includes = [
            f"-I{MOCKS}",
            f"-I{TESTS}",
            f"-I{ROOT / 'src' / 'network'}",
            f"-I{ROOT / 'src' / 'storage'}",
        ]
        cxx = os.environ.get("CXX")
        compiler = shutil.which(cxx) if cxx else None
        if compiler is None:
            compiler = shutil.which("g++") or shutil.which("clang++")

        if compiler:
            command = [
                compiler,
                "-std=c++17",
                "-Wall",
                "-Wextra",
                "-Werror",
                *common_includes,
                *(str(source) for source in sources()),
                "-o",
                str(output),
            ]
            subprocess.run(command, cwd=ROOT, check=True)
        else:
            msvc_env = find_msvc_environment()
            if not msvc_env:
                print("No host C++ compiler found (tried CXX, g++, clang++, and Visual Studio).", file=sys.stderr)
                return 2
            includes = [f'/I"{path[2:]}"' for path in common_includes]
            command_text = " ".join(
                [
                    "cl.exe",
                    "/nologo",
                    "/std:c++17",
                    "/EHsc",
                    "/W4",
                    "/WX",
                    *includes,
                    *(f'"{source}"' for source in sources()),
                    f'/Fe:"{output}"',
                ]
            )
            build_cmd = Path(temp) / "build_tests.cmd"
            build_cmd.write_text(
                f'call "{msvc_env}" -arch=x64 -host_arch=x64\n'
                "if errorlevel 1 exit /b %errorlevel%\n"
                f"{command_text}\n",
                encoding="utf-8",
            )
            # Some host PATH entries contain unescaped parentheses that break
            # VSDevCmd's batch expansion. Start it from a small Windows PATH;
            # the developer command script adds its own compiler/runtime paths.
            build_env = os.environ.copy()
            windows = Path(os.environ.get("SystemRoot", r"C:\Windows"))
            build_env["PATH"] = ";".join(
                str(windows / suffix)
                for suffix in (
                    "System32",
                    "System32\\Wbem",
                    "System32\\WindowsPowerShell\\v1.0",
                )
            )
            subprocess.run(
                ["cmd.exe", "/d", "/c", str(build_cmd)],
                cwd=temp,
                check=True,
                env=build_env,
            )

        subprocess.run([str(output)], cwd=ROOT, check=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
