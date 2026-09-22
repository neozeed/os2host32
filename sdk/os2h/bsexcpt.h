/*	SCCSID = @(#)except32.h        13.5 90/03/27 */

/*************************** START OF SPECIFICATION *****************
 *
 * Source File Name: except32.h
 *
 * Descriptive Name: Thread Exception Constants and Structure Definitions.
 *
 * Copyright: IBM Corp. 1989
 * Copyright: Microsoft Corp. 1989
 *
 * Function: This file provides constants and data structure
 *	definitions required by application programs to use 32 bit
 *	thread exceptions management facility.
 *
 * Notes: None.
 *
 *************************** END OF SPECIFICATION *******************/

/*
 * User Exception Handler Return Codes:
 */
#define NOT_HANDLED		-1	/* exception not handled */
#define HANDLED			0	/* exception handled (= NO_ERROR) */

/*
 * fHandlerFlags values (see ExceptionStructure):
 *
 * The exception manager sets EH_UNWINDING and EH_NESTED_CALL.
 * The user exception handler sets EH_NONCONTINUABLE.
 */
#define EH_UNWINDING	    0x01 /* Set if call is being made for an unwind */
				 /* operation. Otherwise call is being made */
				 /* for an exception			    */
#define EH_NONCONTINUABLE   0x02 /* Set if exception is non-continuable	    */
#define EH_NESTED_CALL	    0x04 /* Set when an exception occurs while an   */
				 /* exception is active for the current	    */
				 /* handler				    */

/*
 * Unwind all exception handlers (see DosUnwindException API)
 */
#define UNWIND_ALL		0

/*
 * ExceptionNum values (see ExceptionStructure):
 *
 * This includes all exceptions and signals supported by 32 bit thread
 * exception management.
 *
 * The exception numbers are distrubuted as follows:
 *
 *	0x00000000-0x7fffffff - reserved for OS/2 use only
 *	0x80000000-0xffffffff - available for application specific use
 */
#define XCPT_INVALID		0	/* invalid exception number */
	/* Supported Exceptions and Signals */
#define XCPT_DIV		1	/* Divide Error */
#define XCPT_OVERFLOW		2	/* Overflow */
#define XCPT_BOUND		3	/* Bound Check */
#define XCPT_OP			4	/* Invalid Opcode */
#define XCPT_GP			5	/* General Protection Fault */
#define XCPT_FPERR		6	/* NPX Error */
#define XCPT_GPF		7	/* Gaurd Page Fault */
#define XCPT_GPAF		8	/* Guard Page Alloc Failure */
#define XCPT_PTERM		9	/* Process Termination (sync) */
#define XCPT_BREAK		10	/* Control Break */
#define XCPT_INTR		11	/* Control C */
#define XCPT_TERM		12	/* Kill Process */
#define XCPT_APTERM		13	/* Process Termination (async) */


/*
 * Bit mask to indicate which type of error code is in the rest of
 * the ExceptionInfo field when the ExceptionNum is set to XCPT_GP.
 * The high bit is clear for general protection (GP) faults and set
 * for page faults.  The EC_CR2 field is validate when this bit is set.
 */
#define XCPTF_PF    0x80000000	    /* if set, page fault */
				    /* if clear, GP fault */


/*
 * Exception Context.
 *
 * This is the machine specific register contents for the thread
 * at the time of the exception.
 */
struct ExceptionContext {	/* XCPTCTX */
	ULONG  EC_edi;		/* general purpose regs */
	ULONG  EC_esi;
	ULONG  EC_ebp;
	ULONG  EC_esp;
	ULONG  EC_ebx;
	ULONG  EC_edx;
	ULONG  EC_ecx;
	ULONG  EC_eax;
	ULONG  EC_eip;
	ULONG  EC_eflags;
	USHORT EC_cs;		/* segment registers */
	USHORT EC_ss;
	USHORT EC_ds;
	USHORT EC_es;
	USHORT EC_fs;
	USHORT EC_gs;
	ULONG  EC_cr0;		/* machine state word (r/o) */
	ULONG  EC_cr2;		/* for GP fault (r/o) */
};

typedef struct ExceptionContext	    ECS;
typedef struct ExceptionContext *   PECS;


/*
 * Exception Structure.
 *
 * A pointer to this structure is passed to the user's thread
 * exception handler.  It contains the entire exception context.
 */

struct ExceptionStructure {	    /* XCPTSTRUCT */

	ULONG	fHandlerFlags;	    /* bit 0, clear for exception	*/
				    /*	      set for unwind		*/
				    /* bit 1, set if not continuable	*/
				    /* bit 2, set if nested call	*/

	struct	ExceptionHandler    *pCurrentEH;

	/* The following values are undefined for unwind operation	*/

	ULONG	ExceptionNum;	    /* exception number			*/

	struct	ExceptionContext    *pExceptionContext;

	ULONG	cbExceptionInfo;    /* Size of Exception Specific Info	*/

	ULONG	ExceptionInfo[1];   /* Exception Specfic Info		*/
};

typedef struct ExceptionStructure   XCPT;
typedef struct ExceptionStructure * PXCPT;


/*
 * Exception Handler Structure.
 *
 * This structure is used by APPs to register and unregister user
 * thread exception handlers.  The APP only needs to set the
 * exception_handler_routine field.  It should never modify the
 * prev_handler field (modified by the OS/2 kernel only). Upon
 * registration the structure remains in user space.  Therefore,
 * once a structure is registered, APPs should not modify any of the
 * fields until it is unregistered.
 *
 * NOTE: Field prev_handler MUST be first field in structure (see code
 * in DosSetExceptionHandler API).
 */
struct ExceptionHandler {	/* XCPTHAND */
			/* prev_handler MUST BE FIRST */
	struct	ExceptionHandler    *prev_handler;

	ULONG	(*exception_handler_routine)(PXCPT);
};

typedef struct ExceptionHandler	    EHS;
typedef struct ExceptionHandler *   PEHS;
