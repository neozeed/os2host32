/***************************************************************************\
* wmchar.h -- Define identifiers connecting resources to the code
*
* Created by Microsoft, IBM Corporation 1990
\***************************************************************************/


#define  ID_RESOURCES       1


#define  IDM_ACTIONS        0x100
#define  IDM_CLEAR          0x101
#define  IDM_KEYUPS         0x102

#define  IDM_DISPLAY        0x110
#define  IDM_NUMBER         0x111
#define  IDM_VIRTUALKEY     0x112
#define  IDM_CHAR           0x113
#define  IDM_SCANCODE       0x114
#define  IDM_REPEAT         0x115
#define  IDM_FLAGS          0x116

#define  IDM_DISPLAYFIRST   IDM_NUMBER
#define  IDM_DISPLAYLAST    IDM_FLAGS
#define  CDISPLAYFIELDS     (IDM_DISPLAYLAST - IDM_DISPLAYFIRST + 1)
