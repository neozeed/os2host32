#!/usr/bin/env python3
import subprocess, sys
p=subprocess.run([sys.argv[1],'--run',sys.argv[2]],capture_output=True,text=True,input="",timeout=30)
s=p.stdout+p.stderr
required=['Microsoft (R) Library Manager  Version 3.17.000','Library name:','LIB : fatal error U1151','termination=Dos16Exit(EXIT_PROCESS) rc=2']
assert p.returncode==2,(p.returncode,s)
assert all(x in s for x in required),s
assert 'stack overflow' not in s and 'R6000' not in s,s
assert 'unsupported DOS16.' not in s,s
print('LIB NE startup regression PASS (CRT/banner reached, expected no-argument rc=2; file APIs pending)')
