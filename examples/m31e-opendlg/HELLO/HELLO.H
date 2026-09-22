/*
    HELLO header file
    Created by Microsoft, IBM Corporation 1990
*/
#define ID_RESOURCE 1
#define IDM_FILE    10
#define IDM_OPEN    100
#define IDM_SAVE    101
#define IDM_ABOUT   102
#define IDD_ABOUT   1

MRESULT EXPENTRY HelloWndProc(HWND, USHORT, MPARAM, MPARAM);
MRESULT EXPENTRY AboutDlgProc(HWND, USHORT, MPARAM, MPARAM);
int main(void);
