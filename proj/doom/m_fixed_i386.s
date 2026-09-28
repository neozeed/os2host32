.text
	.align 4
.globl _FixedMul
_FixedMul:	
	pushl %ebp
	movl %esp,%ebp
	movl 8(%ebp),%eax
	imull 12(%ebp)
	shrdl $16,%edx,%eax
	popl %ebp
	ret


	.align 4
.globl _FixedDiv
_FixedDiv:
	pushl %ebp
	movl %esp,%ebp
	movl 8(%ebp),%eax
	cdq
	shldl $16,%eax,%edx
	sall	$16,%eax
	idivl	12(%ebp)
	popl %ebp
	ret
