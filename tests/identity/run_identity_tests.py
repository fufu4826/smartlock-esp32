#!/usr/bin/env python3
"""Compile production identity/storage and controller code against isolated host fakes."""
from __future__ import annotations
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

ROOT=Path(__file__).resolve().parents[2]
TESTS=ROOT/"tests"/"identity"

def find_msvc_environment() -> Path|None:
    program_files=Path(os.environ.get("ProgramFiles(x86)",r"C:\Program Files (x86)"))
    vswhere=program_files/"Microsoft Visual Studio"/"Installer"/"vswhere.exe"
    if not vswhere.exists():return None
    found=subprocess.run([str(vswhere),"-latest","-products","*","-property","installationPath"],capture_output=True,text=True,check=False)
    if found.returncode or not found.stdout.strip():return None
    install=Path(found.stdout.strip())
    for rel in (Path("Common7/Tools/VsDevCmd.bat"),Path("VC/Auxiliary/Build/vcvars64.bat")):
        path=install/rel
        if path.exists():return path
    return None

def main()->int:
    loop=(ROOT/"src"/"main.cpp").read_text(encoding="utf-8-sig")
    assert "sessions.expireSessions(millis());" in loop, "Web-created sessions require a fresh expiry timestamp"
    compiler_name=os.environ.get("CXX")
    compiler=shutil.which(compiler_name) if compiler_name else None
    if compiler is None:compiler=shutil.which("g++") or shutil.which("clang++")
    with tempfile.TemporaryDirectory(prefix="identity-host-tests-") as temp_name:
        temp=Path(temp_name)
        staged=[]
        for name in ("EnrollmentManager","AccessController"):
            original=ROOT/"src"/"app"/f"{name}.cpp"
            text=original.read_text(encoding="utf-8")
            # Keep production method bodies and redirect only dependencies to host support.
            text=re.sub(r"^\s*#include\s+.*$",'#include "identity_test_support.h"',text,flags=re.MULTILINE)
            target=temp/f"{name.lower()}_production.cpp"
            target.write_text(text,encoding="utf-8")
            staged.append(target)
        if "--session-display" in sys.argv:
            source=(ROOT/"src"/"main.cpp").read_text(encoding="utf-8-sig")
            signature="void renderCurrentState(uint32_t) {"
            start=source.index(signature)
            brace=source.index("{",start);depth=0;end=brace
            while end<len(source):
                if source[end]=="{":depth+=1
                elif source[end]=="}":
                    depth-=1
                    if depth==0:
                        end+=1;break
                end+=1
            target=temp/"render_current_state_production.cpp"
            target.write_text('#include "session_display_test_support.h"\n'+source[start:end],encoding="utf-8")
            staged.append(target)
        include_dirs=[TESTS/"mocks",TESTS,ROOT/"src"/"storage",ROOT/"src"/"app",ROOT/"src"/"qr",ROOT/"src"/"events",ROOT/"src"/"network"]
        test_source=TESTS/("session_display_test.cpp" if "--session-display" in sys.argv else
                          "management_auth_expiry_test.cpp" if "--management-expiry" in sys.argv else
                          "access_attribution_test.cpp" if "--audit-attribution" in sys.argv else
                          "cleanup_smoke.cpp" if "--cleanup-smoke" in sys.argv else "identity_store_test.cpp")
        sources=[*staged,TESTS/"identity_test_fakes.cpp",test_source,
          ROOT/"src"/"storage"/"IdentityStore.cpp",ROOT/"src"/"storage"/"AtomicFileStore.cpp",
          ROOT/"src"/"storage"/"UserStore.cpp",ROOT/"src"/"storage"/"DeviceStore.cpp",
          ROOT/"src"/"storage"/"RecordCodec.cpp",ROOT/"src"/"qr"/"SessionManager.cpp"]
        output=temp/("identity_tests.exe" if os.name=="nt" else "identity_tests")
        if compiler:
            command=[compiler,"-std=c++17","-Wall","-Wextra","-Werror",*(f"-I{d}" for d in include_dirs),*(str(s) for s in sources),"-o",str(output)]
            subprocess.run(command,cwd=ROOT,check=True)
        else:
            msvc_env=find_msvc_environment()
            if not msvc_env:
                print("No host C++ compiler found (tried CXX, g++, clang++, and Visual Studio).",file=sys.stderr)
                return 2
            includes=[f'/I"{d}"' for d in include_dirs]
            command_text=" ".join(["cl.exe","/nologo","/std:c++17","/EHsc","/W4","/WX","/utf-8","/wd4100","/wd4996",*includes,*(f'"{s}"' for s in sources),f'/Fe:"{output}"'])
            build=temp/"build_tests.cmd"
            build.write_text(f'call "{msvc_env}" -arch=x64 -host_arch=x64\nif errorlevel 1 exit /b %errorlevel%\n{command_text}\n',encoding="utf-8")
            windows=Path(os.environ.get("SystemRoot",r"C:\Windows"));env=os.environ.copy()
            env["PATH"]=";".join(str(windows/suffix) for suffix in ("System32","System32\\Wbem","System32\\WindowsPowerShell\\v1.0"))
            subprocess.run(["cmd.exe","/d","/c",str(build)],cwd=temp,check=True,env=env)
        subprocess.run([str(output)],cwd=ROOT,check=True)
    return 0

if __name__=="__main__":raise SystemExit(main())
