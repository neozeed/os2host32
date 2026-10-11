# Optional DOS environment sampler. Run DOSLOW.COM in DOS, never on the host.
# Writes physical 00000..0407F to DOSLOW.BIN (16512 bytes).
# No patches to the game, TSR, interrupt vectors, or hardware.
.code16
.text
.global _start
_start:
 push %cs
 pop %ds
 mov $filename,%dx
 xor %cx,%cx
 mov $0x3c00,%ax
 int $0x21
 jc failed
 mov %ax,%bx
 xor %ax,%ax
 mov %ax,%ds
 xor %dx,%dx
 mov $0x4080,%cx
 mov $0x4000,%ax
 int $0x21
 jc close_failed
 cmp $0x4080,%ax
 jne close_failed
 mov $0x3e00,%ax
 int $0x21
 jc failed
 mov $0x4c00,%ax
 int $0x21
close_failed:
 mov $0x3e00,%ax
 int $0x21
failed:
 mov $0x4c01,%ax
 int $0x21
filename:
 .asciz "DOSLOW.BIN"
