//-------------------------- general definitions -----------------------------
 
extern HAB      habMain;          // main thread anchor block handle
extern HMQ      hmqMain;          // main thread queue handle
extern HWND     hwndFrame;        // frame control handle
extern HWND     hwndClient;       // client area handle
extern HWND     hwndStatus;       // status window handle
extern HWND     hwndZoomScrollBar;// zoom scroll bar handle
extern HWND     hwndMenu;         // Menu handle
extern HWND     hwndStatusOption; // status menu handle
extern HDC      hdcClient;        // window dc handle
extern HPS      hpsClient;        // client area Gpi ps handle
extern SIZEL    sizlMaxClient;    // max client area size
extern HPS      hpsPaint;         // ps for use in Main Thread
extern HRGN     hrgnInvalid;      // handle to the invalid region

extern HAB      habAsync;         // async thread anchor block handle
extern HMQ      hmqAsync;         // async thread queue handle
extern TID      tidAsync;         // async thread id
extern SEL      selStack;         // async thread stack selector
extern SHORT    sPrty;            // async thread priority

extern HWND     hwndHorzScroll;   // horizontal scroll bar window
extern HWND     hwndVertScroll;   // vertical scroll bar window

extern POINTS   ptsScrollPos;
extern POINTS   ptsOldScrollPos;
extern POINTS   ptsScrollMax;
extern POINTS   ptsHalfScrollMax;
extern POINTS   ptsScrollLine;
extern POINTS   ptsScrollPage;

extern MATRIXLF matlfIdentity;
extern LONG     lScale;           // current zoom level
extern POINTL   ptlScaleRef;      // scalefactor, detects size change

extern POINTL   ptlOffset;
extern POINTL   ptlBotLeft;
extern POINTL   ptlTopRight;
extern POINTL   ptlMoveStart;     // model space point at start of move
extern LONG     lLastSegId;       // last segment id assigned to a piece
extern LONG     lPickedSeg;       // seg id of piece selected for drag
extern POINTL   ptlOffStart;      // segment xform xlate at move start
extern RECTL    rclBounds;        // pict bounding box in model coords.
extern POINTL   ptlOldMouse;      // current mouse posn
extern POINTL   ptlMouse;         // current mouse posn
extern BOOL     fButtonDownMain;  // only drag if mouse down
extern BOOL     fButtonDownAsync; // only drag if mouse down

extern POINTL   ptlUpdtRef;
extern POINTL   aptlUpdt[3];
extern BOOL     fUpdtFirst;
extern BOOL     fFirstLoad;

//-------------------------- segment list ------------------------------------

extern PSEGLIST pslHead;          // head of the list
extern PSEGLIST pslTail;          // tail of the list
extern PSEGLIST pslPicked;        // picked segment's list member

//-------------------------- bitmap-related data -----------------------------

extern LOADINFO           li;
extern PLOADINFO          pli;

extern HPS                hpsBitmapFile;      // bitmap straight from the file

extern HDC                hdcBitmapFile;
extern HBITMAP            hbmBitmapFile;

extern BITMAPINFOHEADER2  bmp2BitmapFile;
extern PBITMAPINFOHEADER2 pbmp2BitmapFile;
extern BITMAPINFOHEADER2  bmp2BitmapFileRef;
extern PBITMAPINFOHEADER2 pbmp2BitmapFileRef;

extern HPS                hpsBitmapSize;      // bitmap sized to the current
                                              // size
extern HDC                hdcBitmapSize;
extern HBITMAP            hbmBitmapSize;

extern HPS                hpsBitmapBuff;      // image composed here, copied to
                                              // screen
extern HDC                hdcBitmapBuff;
extern HBITMAP            hbmBitmapBuff;

extern HPS                hpsBitmapSave;      // save part of screen during
                                              // dragging
extern HDC                hdcBitmapSave;
extern HBITMAP            hbmBitmapSave;

extern BITMAPINFOHEADER2  bmp2BitmapSave;
extern PBITMAPINFOHEADER2 pbmp2BitmapSave;

extern DEVOPENSTRUC       dop;

//--------------------------- Miscellaneous ----------------------------------

extern HMTX    hmtxSzFmt;

extern CHAR    szFmt[50];                     // buffer used by sprintf()
extern CHAR    szZoomFact[10];

extern HEV     hevTerminate;
extern HEV     hevDrawOn;
extern HEV     hevMouse;
extern HEV     hevLoadMsg;
extern HEV     hevKillDraw;

extern PSZ     pszStartupMsg;
extern PSZ     pszLoadMsg;
extern PSZ     pszError;
extern PSZ     pszBlankMsg;

extern SWCNTRL swctl;
extern HSWITCH hsw;                           // handle to a switch list entry
extern char    szTitle[80];                   // Title bar text

extern BOOL    fErrMem;                       // set if alloc async stack fails

extern LONG    lByteAlignX, lByteAlignY;      // memory alignment constants
