from pathlib import Path
import importlib.util,subprocess,tempfile,os
root=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('runner',root/'tests/network/run_network_manager_tests.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
with tempfile.TemporaryDirectory(prefix='touch-state-') as temp:
 p=Path(temp); exe=p/'test.exe'
 includes=[root/'tests/touch/mocks',root/'tests/identity/mocks',root/'src/hardware',root/'src/app']
 sources=[root/'tests/touch/touch_state_test.cpp',root/'src/hardware/TouchManager.cpp',root/'src/app/AppStateMachine.cpp']
 command='cl /nologo /std:c++17 /EHsc /W4 /WX /utf-8 '+' '.join('/I"'+str(x)+'"' for x in includes)+' '+' '.join('"'+str(x)+'"' for x in sources)+' /Fe:"'+str(exe)+'"'
 script=p/'build.cmd';script.write_text('call "'+str(m.find_msvc_environment())+'" -arch=x64 -host_arch=x64\n'+command+'\n',encoding='utf-8')
 env=os.environ.copy(); win=Path(os.environ.get('SystemRoot',r'C:\Windows')); env['PATH']=';'.join(str(win/x) for x in ['System32','System32/Wbem','System32/WindowsPowerShell/v1.0'])
 subprocess.run(['cmd','/d','/c',str(script)],cwd=p,check=True,env=env)
 subprocess.run([str(exe)],check=True)
