# Soft386 I386-H2H handoff — async network generation fence

Base: I386-H2G (`cycle-limit teardown`).

## Goal

Stabilize the generic asynchronous SO32DLL/TCP32DLL bridge before further PM
work. The live TelnetPM symptom was: connect reports/hangs, Cancel reports
failure, then an apparently late connection succeeds. H2H does not add any
TELNETPM application checks or socket semantic special cases.

## Root cause addressed

Soft386 network jobs were bound to guest TID/call frame, but not to the
lifetime of the guest socket descriptor. A CONNECT/RECV/etc. native worker
could therefore finish after another guest thread had initiated SOCLOSE and
its old completion was still eligible to commit into the guest.

## Changes

- Added a generation counter to each tracked guest socket lifetime.
- Every asynchronous operation on a socket snapshots that generation.
- Starting SO32DLL.17 SOCLOSE advances the generation immediately, fencing
  operations already in flight.
- A late completion whose socket vanished or whose generation changed is
  discarded before any guest buffer/result is committed.
- The owner guest thread receives failure with IBM SOCEINTR (10004) for that
  stale completion.
- Successful close still forgets the socket; later operations continue to
  receive SOCE_NOTSOCK as before.
- `--trace-native` now reports network operation start/completion, close
  generation fences, and discarded stale completions. This is generic
  instrumentation for all socket users.

Representative trace expected on the race:

```
soft386: net start SO32DLL.3 CONNECT TID=... socket=... gen=...
soft386: net cancel fence SO32DLL.17 SOCLOSE TID=... socket=... new_gen=...
soft386: net stale completion SO32DLL.3 CONNECT ... discarded
```

## Validation

- `make -C soft386 check-quick` PASS.
- `net-bridge-check` PASS.
- New regression holds a receive worker in flight, closes the same socket from
  another guest slot, then releases the old worker. Its stale completion is
  rejected with SOCEINTR and cannot modify guest memory.
- The subsequent operation on the closed descriptor still fails as stale.
- Existing loopback binary I/O, concurrent sender, resolver graphs, per-thread
  errno, SELECT, bounds and stale-socket checks remain passing.

## Windows acceptance target

Run the original 1993 TELNETPM under:

```
soft386_os2.exe --max-cycles 0 --trace-native --run demos\telnetpm\telnetpm.exe
```

Reproduce Connect then Cancel if necessary. Capture stderr. H2H is proven if a
cancelled/closed socket cannot later commit an older CONNECT success. If the UI
still reports failure then later establishes a session without any SOCLOSE
fence/stale completion in the trace, the next investigation is the application's
SELECT/nonblocking completion protocol rather than socket-lifetime ordering.

## Non-goals

- no TelnetPM-specific behavior
- no changes to TCP protocol semantics
- no Neko work
- no PM changes
- no new socket API surface
