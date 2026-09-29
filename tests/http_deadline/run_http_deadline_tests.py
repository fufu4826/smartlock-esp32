#!/usr/bin/env python3
"""Compile and run tests against the production request-deadline helper."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
TESTS = ROOT / "tests" / "http_deadline"
PARSER = ROOT / "lib" / "SmartLockWebServer" / "src" / "Parsing.cpp"

def main():
    parser = PARSER.read_text(encoding="utf-8")
    assert 'url.substring(0,hasSearch)=="/api/registration/reconcile"' in parser
    assert "readRequestBody(client, _clientContentLength, plainLength, deadline)" in parser
    compiler = shutil.which(os.environ.get("CXX", "")) if os.environ.get("CXX") else None
    compiler = compiler or shutil.which("g++") or shutil.which("clang++")
    with tempfile.TemporaryDirectory(prefix="http-deadline-tests-") as temp:
        output = Path(temp) / ("deadline.exe" if os.name == "nt" else "deadline")
        if compiler:
            subprocess.run([compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror",
                            f"-I{TESTS}", f"-I{ROOT / 'lib/SmartLockWebServer/src'}",
                            str(TESTS / "request_deadline_test.cpp"), "-o", str(output)],
                           cwd=ROOT, check=True)
        else:
            program_files = Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)"))
            vswhere = program_files / "Microsoft Visual Studio" / "Installer" / "vswhere.exe"
            found = subprocess.run([str(vswhere), "-latest", "-products", "*", "-property", "installationPath"],
                                   capture_output=True, text=True, check=False)
            if found.returncode or not found.stdout.strip(): raise SystemExit("No host C++ compiler found")
            devcmd = Path(found.stdout.strip()) / "Common7/Tools/VsDevCmd.bat"
            build = Path(temp) / "build.cmd"
            includes = f'/I"{TESTS}" /I"{ROOT / "lib/SmartLockWebServer/src"}"'
            source = TESTS / "request_deadline_test.cpp"
            compile_line = f'cl.exe /nologo /std:c++17 /EHsc /W4 /WX {includes} "{source}" /Fe:"{output}"'
            build.write_text(f'call "{devcmd}" -arch=x64 -host_arch=x64\nif errorlevel 1 exit /b %errorlevel%\n{compile_line}\n', encoding="utf-8")
            windows = Path(os.environ.get("SystemRoot", r"C:\Windows"))
            env = os.environ.copy()
            env["PATH"] = ";".join(str(windows / suffix) for suffix in ("System32", "System32\\Wbem", "System32\\WindowsPowerShell\\v1.0"))
            subprocess.run(["cmd.exe", "/d", "/c", str(build)], cwd=temp, env=env, check=True)
        subprocess.run([str(output)], cwd=ROOT, check=True)

if __name__ == "__main__":
    main()
