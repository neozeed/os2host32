;/*
; *   File Dialog Library
; *   Created by Microsoft Corporation, 1989
; */

	.386p

DATA	SEGMENT	WORD PUBLIC 'DATA'

	EXTRN	_vhModule:far
	PUBLIC	__acrtused
__acrtused	dw	0
DATA	ENDS


TEXT	SEGMENT	BYTE PUBLIC 'CODE'
; The following trick is to avoid any need for a DS where a constant
;   string is needed, any (PSZ)"foo" is replaced by (PSZ)szfoo with
;   szfoo created in the Code Selector. As we can't do that with C,
;   we do it with MASM.
;
	PUBLIC	_szStar
_szStar	LABEL	BYTE
	DB	'*',0
        PUBLIC  _szDot
_szDot   LABEL   BYTE
        DB      '.',0

TEXT	ENDS

INIT	SEGMENT	BYTE PUBLIC 'CODE'
	EXTRN	InitLibrary:near

	assume CS:FLAT, DS:FLAT, ES:FLAT, SS:FLAT

INIT	ENDS
	END	InitLibrary
