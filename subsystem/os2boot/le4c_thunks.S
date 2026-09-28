/* os2boot/le4c_thunks.S
 *
 * i386 MinGW/PE assembly thunks for entering LE code and crossing
 * from untouched OS/2 code back to the native OS2BOOT stack.
 */

    .text

/*
 * 32-bit MinGW COFF C symbols have a leading underscore.
 */

/*
 * Entry from untouched LE code.
 *
 * EAX = DOSCALLS ordinal
 * ESP = OS/2/C386 stack
 *
 * Save the OS/2 machine state, switch to the preserved native stack,
 * call:
 *
 *     Le4cGatewayDispatch(ordinal, os2_esp)
 *
 * then restore the OS/2 stack/state and RET through the untouched
 * LE CALL return address.
 */
    .globl _Le4cDoscallGatewayEntry
    .def   _Le4cDoscallGatewayEntry
    .scl   2
    .type  32
    .endef

_Le4cDoscallGatewayEntry:
    movl    %esp, _gLe4cOs2Esp
    movl    %ebx, _gLe4cSavedEbx
    movl    %esi, _gLe4cSavedEsi
    movl    %edi, _gLe4cSavedEdi
    movl    %ebp, _gLe4cSavedEbp
    movl    %eax, _gLe4cLastOrdinal

    movl    _gLe4cNativeEsp, %edx
    movl    %edx, %esp

    /*
     * cdecl:
     *
     * Le4cGatewayDispatch(
     *     ULONG Ordinal,
     *     ULONG Os2Esp);
     *
     * arguments pushed right-to-left.
     */
    pushl   _gLe4cOs2Esp
    pushl   %eax
    call    _Le4cGatewayDispatch
    addl    $8, %esp

    movl    %eax, _gLe4cLastReturnCode

    movl    _gLe4cOs2Esp, %edx
    movl    %edx, %esp

    movl    _gLe4cSavedEbx, %ebx
    movl    _gLe4cSavedEsi, %esi
    movl    _gLe4cSavedEdi, %edi
    movl    _gLe4cSavedEbp, %ebp

    movl    _gLe4cLastReturnCode, %eax
    ret


/*
 * VOID __cdecl
 * Le4cEnterLe(ULONG Entry, ULONG Os2Esp)
 *
 * One-way transition into untouched LE code.
 *
 * Exact initial state:
 *     EAX = Entry
 *     EBX/ECX/EDX/ESI/EDI/EBP = 0
 *     ESP = supplied OS/2 stack
 */
    .globl _Le4cEnterLe
    .def   _Le4cEnterLe
    .scl   2
    .type  32
    .endef

_Le4cEnterLe:
    movl    4(%esp), %eax
    movl    %eax, _gLe4cEntryVa

    movl    8(%esp), %edx

    /*
     * Preserve this native ESP. Le4cEnterLe never returns, so its
     * caller's native stack frame remains available as our gateway
     * stack anchor for the lifetime of the LE image.
     */
    movl    %esp, _gLe4cNativeEsp

    movl    %edx, %esp

    xorl    %ebx, %ebx
    xorl    %ecx, %ecx
    xorl    %edx, %edx
    xorl    %esi, %esi
    xorl    %edi, %edi
    xorl    %ebp, %ebp

    movl    _gLe4cEntryVa, %eax
    jmp     *%eax
