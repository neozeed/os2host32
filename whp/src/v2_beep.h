/* DosBeep blocks the guest caller, never the single WHP/message-pump thread.
   Tone workers own only their small job; they never access Runtime/guest RAM. */
struct BeepJob {DWORD frequency,duration;};
static volatile LONG beep_active;
static DWORD WINAPI beep_worker(LPVOID opaque)
{
    struct BeepJob job=*(struct BeepJob *)opaque;
    free(opaque);
    if(!Beep(job.frequency,job.duration))
        fprintf(stderr,"v2: native tone unavailable (Windows error %lu)\n",(unsigned long)GetLastError());
    InterlockedDecrement(&beep_active);return 0;
}
static uint32_t guest_beep(struct Runtime *rt,uint32_t frequency,uint32_t duration)
{
    struct BeepJob *job;HANDLE thread;
    if(frequency<37 || frequency>32767 || duration>60000)return OS2_ERROR_INVALID_PARAMETER;
    if(!duration)return 0;
    if(InterlockedIncrement(&beep_active)>32) {
        InterlockedDecrement(&beep_active);return OS2_ERROR_NOT_ENOUGH_MEMORY;
    }
    job=(struct BeepJob *)malloc(sizeof(*job));
    if(!job){InterlockedDecrement(&beep_active);return OS2_ERROR_NOT_ENOUGH_MEMORY;}
    job->frequency=frequency;job->duration=duration;
    thread=CreateThread(NULL,0,beep_worker,job,0,NULL);
    if(!thread){free(job);InterlockedDecrement(&beep_active);return OS2_ERROR_NOT_ENOUGH_MEMORY;}
    CloseHandle(thread);
    return guest_sleep(rt,duration);
}
