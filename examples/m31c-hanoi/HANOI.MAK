hanoi.exe: hanoi.obj hanoi.res hanoi.def
 link386 hanoi,hanoi/codeview ,hanoi/map, os2386 libc/nod, hanoi.def
    rc hanoi.res

hanoi.res: hanoi.rc hanoi.h hanoi.ico
 rc -r $*.rc

hanoi.obj: hanoi.c hanoi.h
 cl386 -Zi -W3 -c -Gsw -B1 C1L_386 -DINCL_32 hanoi.c
