/*
   BIOrhythm header file
   Created by Microsoft, IBM Corporation, 1990
*/
/* String table constants */
#define IDS_APPNAME      1
#define IDS_KIDNAME      2

/* Resource Ids */
#define ID_BIO          1
#define IDD_ABOUT       2
#define IDD_DATE        3

/* Menu constants */
#define IDM_DATES	0x100
#define IDM_LEGEND	0x101
#define	IDM_COPY	0x102
#define IDM_ABOUT	0x103

/* Bio Dialog Box constants */
#define ID_BDYEAR        301
#define ID_BDMONTH       302
#define ID_BDDAY         303
#define ID_YEAR          304
#define ID_MONTH         305
#define ID_DAY           306
#define ID_OS2INI        307

/* Function prototypes */
BOOL	cdecl	 main	       ( VOID );
MRESULT EXPENTRY BioWndProc    ( HWND, USHORT, MPARAM, MPARAM );
MRESULT EXPENTRY KidWndProc    ( HWND, USHORT, MPARAM, MPARAM );
VOID		 APPPaint      ( HWND );
MRESULT EXPENTRY BioDlg        ( HWND, USHORT, MPARAM, MPARAM );
MRESULT EXPENTRY About	       ( HWND, USHORT, MPARAM, MPARAM );
MRESULT EXPENTRY FrameWndProc  ( HWND, USHORT, MPARAM, MPARAM );

VOID		 BioGetDate    ( HWND hWnd);
VOID		 InitBioDlg    ( HWND hDlg );
VOID		 BioDlgCmd     ( HWND hDlg, MPARAM mp1 );
double		 julian	       ( SHORT, SHORT, double );
void		 calendar      ( double, SHORT *, SHORT *, double * );
long		 trunc4        ( double );
