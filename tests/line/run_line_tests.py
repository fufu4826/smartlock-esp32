from __future__ import annotations

import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
TEST = ROOT / "tests" / "line" / "line_notifications_host.cpp"
FAKES = ROOT / "tests" / "line" / "fakes"


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
    for rel in (Path("Common7/Tools/VsDevCmd.bat"), Path("VC/Auxiliary/Build/vcvars64.bat")):
        candidate = install / rel
        if candidate.exists():
            return candidate
    return None


def main() -> int:
    compiler = os.environ.get("CXX") or shutil.which("g++") or shutil.which("clang++")
    msvc_env = None if compiler else find_msvc_environment()
    if not compiler and not msvc_env:
        print("Host C++ compiler unavailable; set CXX or install Visual Studio C++ tools.", file=sys.stderr)
        return 2
    with tempfile.TemporaryDirectory(prefix="smartlock-line-tests-") as temp:
        exe = Path(temp) / "line_notifications_host.exe"
        if compiler:
            compile_cmd = [compiler, "-std=c++17", "-Wall", "-Wextra", f"-I{FAKES}", str(TEST), "-o", str(exe)]
            subprocess.run(compile_cmd, cwd=ROOT, check=True)
        else:
            build = Path(temp) / "build_line_tests.cmd"
            command = " ".join(["cl.exe", "/nologo", "/std:c++20", "/EHsc", "/W4", "/utf-8", "/wd4100", "/wd4996", "/wd4505",
                                 f'/I"{FAKES}"', f'"{TEST}"', f'/Fe:"{exe}"'])
            build.write_text(f'call "{msvc_env}" -arch=x64 -host_arch=x64\nif errorlevel 1 exit /b %errorlevel%\n{command}\n', encoding="utf-8")
            windows = Path(os.environ.get("SystemRoot", r"C:\Windows"))
            env = os.environ.copy()
            env["PATH"] = ";".join(str(windows / suffix) for suffix in ("System32", "System32\\Wbem", "System32\\WindowsPowerShell\\v1.0"))
            subprocess.run(["cmd.exe", "/d", "/c", str(build)], cwd=temp, check=True, env=env)
        subprocess.run([str(exe)], cwd=ROOT, check=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
