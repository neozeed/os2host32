#!/usr/bin/env python3
"""Compile production scheduler helpers against register/clock doubles."""
import pathlib, re, subprocess, tempfile
root=pathlib.Path(__file__).resolve().parents[1]
s=(root/'whp_os2_v2_hi.c').read_text()
def func(name):
    m=re.search(r'^static [^\n]*\b'+name+r'\(',s,re.M)
    assert m,name
    start=m.start(); b=s.index('{',m.end()); level=1; e=b+1
    while level:
        level+=(s[e]=='{')-(s[e]=='}'); e+=1
    return s[start:e]+'\n'
macros='\n'.join(x for x in s.splitlines() if x.startswith('#define ') and not x.endswith('\\'))
structs=s[s.index('struct GuestAlloc {'):s.index('struct GuestModule {')]
helpers=['rd16','rd32','wr32','align_up','guest_range','guest_u32','guest_put_u32','guest_copy_cstr',
         'alloc_guest','find_alloc','hostcall_stub','save_thread_context','load_thread_context',
         'current_guest_thread','find_thread','next_runnable_thread','thread_state_name',
         'wake_thread_waiters','event_make_handle','event_from_handle','set_saved_thread_rc',
         'wake_event_waiters','wake_expired_waiters','next_wait_timeout']
with tempfile.TemporaryDirectory() as tmp:
    p=pathlib.Path(tmp)
    (p/'defs.inc').write_text(macros+'\n'+structs+'\n'+(root/'v2_sync_types.h').read_text())
    (p/'production.inc').write_text('static int console_poll_wait(struct Runtime *,struct GuestThread *);\nstatic int process_poll_wait(struct Runtime *,struct GuestThread *);\n'+(root/'v2_process_boot.h').read_text().split('static int child_attach')[0]+'\n'+(root/'v2_far16.h').read_text().split('static void install_far16')[0]+'\n'+'\n'.join(map(func,helpers))+'\n'+
        '\n'.join((root/n).read_text() for n in ['v2_info.h','v2_callback.h','v2_sync.h','v2_queue.h','v2_pm.h','v2_gpi.h','v2_beep.h','v2_console.h','v2_process.h'])+'\n'+
        '\n'.join(map(func,['create_guest_thread','schedule_next_thread','switch_after_hypercall'])))
    subprocess.run(['gcc','-std=c99','-Wall','-Wextra','-Wno-misleading-indentation','-Werror','-fsanitize=address,undefined',
        '-g','-I'+tmp,str(root/'tests/sync-check.c'),'-o',str(p/'check')],check=True)
    subprocess.run([str(p/'check')],check=True)
