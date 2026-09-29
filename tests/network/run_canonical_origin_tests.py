#!/usr/bin/env python3
"""Compile CanonicalOrigin.cpp against focused mDNS and Arduino host fakes."""

from __future__ import annotations

from pathlib import Path
import shutil
import subprocess
import tempfile
import os


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


ROOT = Path(__file__).resolve().parents[2]
TESTS = ROOT / "tests" / "network"


def main() -> int:
    compiler = shutil.which("g++") or shutil.which("clang++")
    msvc_env = None if compiler else find_msvc_environment()
    if not compiler and not msvc_env:
        print("No host C++ compiler found (tried g++, clang++, and Visual Studio).")
        return 2
    with tempfile.TemporaryDirectory(prefix="canonical-origin-tests-") as temp:
        output = Path(temp) / "canonical_origin_tests"
        sources = [
            str(ROOT / "src" / "network" / "CanonicalOrigin.cpp"),
            str(TESTS / "canonical_origin_test.cpp"),
        ]
        if compiler:
            subprocess.run(
                [
                compiler,
                "-std=c++17",
                "-Wall",
                "-Wextra",
                "-Werror",
                f"-I{TESTS / 'canonical_origin_mocks'}",
                f"-I{ROOT / 'src' / 'network'}",
                *sources,
                "-o",
                str(output),
                ],
                cwd=ROOT,
                check=True,
            )
        else:
            includes = [
                f'/I"{TESTS / "canonical_origin_mocks"}"',
                f'/I"{ROOT / "src" / "network"}"',
            ]
            command_text = " ".join(
                [
                    "cl.exe",
                    "/nologo",
                    "/std:c++17",
                    "/EHsc",
                    "/W4",
                    "/WX",
                    "/D_CRT_SECURE_NO_WARNINGS",
                    *includes,
                    *(f'"{source}"' for source in sources),
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
