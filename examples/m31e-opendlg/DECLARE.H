/*
   DECLARE.H -- File Dialog Library Definitions
   Created by Microsoft, IBM  Corporation 1990
*/

/* from data.c */
extern	HMODULE	vhModule;
extern	PSTR	vrgsz[];

/* From tool.c */
PSZ	NextChar (PSZ);
PSZ	PrevChar (PSZ, PSZ);
PSZ	Upper (PSZ);

/* From tool1.c */
int	EXPENTRY AlertBox (HWND, int, PCH, PCH, USHORT, USHORT);
int	EXPENTRY DlgFile (HWND, PDLF);
void	EXPENTRY SetupDLF( PDLF, int, PHFILE, PSZ, PSZ, PSZ, PSZ);
BOOL	EXPENTRY MergeStrings(PSZ, PSZ, PSZ);

/* from init.c */
ULONG	EXPENTRY	InitLibrary(HMODULE hmod, ULONG hPDLL);

/* from gpi.c */
ULONG	EXPENTRY GetTextExtent (HPS, PCH, int);

/* from file.c */
VOID	DlgOpenName(HWND, PDLF);
VOID	DlgSaveAsName(HWND, PDLF);
VOID	DlgInitOpen (HWND, ULONG);
VOID	DlgInitSaveAs (HWND, ULONG);
VOID	DlgAddSearchExt(PDLF, PSZ);
BOOL	DlgSearchSpec(PSZ);
int	DlgAlertBox(HWND, int, PDLF, USHORT);
VOID	DlgRemoveHlpButton (HWND);
MRESULT	EXPENTRY	DlgOpenWndProc(HWND, USHORT, MPARAM, MPARAM);
MRESULT	EXPENTRY	DlgSaveAsWndProc(HWND, USHORT, MPARAM, MPARAM);
VOID	EXPENTRY	AddExt(PSZ, PSZ);
PSZ	EXPENTRY	FileInPath(PSZ);

/* from file1.c */
int	DlgDirList(HWND, PSZ, int, int, int, USHORT);
int	DlgDirSelect(HWND, PSZ, int);
BOOL	EXPENTRY	OpenFile(PSZ, PHANDLE, PSZ, USHORT);
PSZ	DlgFitPathToBox(HWND, int, PSZ);
int	DlgFillListBoxes(HWND, int, int, USHORT, PSZ);
USHORT	DlgOpenFile(PDLF, HWND);
BOOL	DlgParseFile(PSZ, PSZ, BOOL, BOOL);
BOOL	DlgValidName(PSZ, BOOL);

/* from toola.asm */
extern	char	szStar[];
extern	char	szDot[];
