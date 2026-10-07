# R5 PM bridge catalogue

These are reviewed bridge signatures, not a claim of complete PM behavior.
The existing native DLL export must also be present. Pointer/handle conversions
are explicit; unsupported argument forms fail before the native call.

| Module | Reviewed signatures |
| --- | ---: |
| PMWIN | 84 |
| PMGPI | 47 |
| PMCTLS | 1 |
| MSG | 2 |

Important restrictions: one queue owner; tokenized QMSGs; scalar control
messages only; modeless WinLoadDlg; no WinSubclassWindow or WinDlgBox;
WinCreateWindow accepts null control/presentation data; DevOpenDC accepts
null openData; GpiSetAttrs accepts the native backend's narrow character-color
contract; FILEDLG excludes custom callbacks/templates/type arrays.

GPI bitmap transfers use the selected bitmap's dimensions and checked strides.
MSG replacement tables are copied as host arrays of copied strings. A nonempty
bound message table has the unchanged native MSG backend's limitations.

## PMWIN

| Ordinal | API |
| ---: | --- |
| 701 | WinAlarm |
| 702 | WinBeginEnumWindows |
| 703 | WinBeginPaint |
| 710 | WinCopyRect |
| 715 | WinCreateCursor |
| 716 | WinCreateMsgQueue |
| 725 | WinDestroyCursor |
| 726 | WinDestroyMsgQueue |
| 727 | WinDestroyPointer |
| 728 | WinDestroyWindow |
| 729 | WinDismissDlg |
| 735 | WinEnableWindow |
| 736 | WinEnableWindowUpdate |
| 737 | WinEndEnumWindows |
| 738 | WinEndPaint |
| 743 | WinFillRect |
| 746 | WinFocusChange |
| 752 | WinGetKeyState |
| 753 | WinGetLastError |
| 756 | WinGetNextWindow |
| 757 | WinGetPS |
| 763 | WinInitialize |
| 764 | WinIntersectRect |
| 765 | WinInvalidateRect |
| 766 | WinInvalidateRegion |
| 767 | WinInvertRect |
| 773 | WinIsWindowEnabled |
| 775 | WinIsWindowVisible |
| 778 | WinLoadMenu |
| 780 | WinLoadPointer |
| 781 | WinLoadString |
| 788 | WinMapWindowPoints |
| 789 | WinMessageBox |
| 794 | WinOpenWindowDC |
| 797 | WinPtInRect |
| 804 | WinQueryCapture |
| 815 | WinQueryDlgItemText |
| 817 | WinQueryFocus |
| 821 | WinQueryPointer |
| 823 | WinQueryPointerPos |
| 828 | WinQuerySysPointer |
| 829 | WinQuerySysValue |
| 833 | WinQueryVersion |
| 834 | WinQueryWindow |
| 837 | WinQueryWindowPos |
| 840 | WinQueryWindowRect |
| 841 | WinQueryWindowText |
| 842 | WinQueryWindowTextLength |
| 843 | WinQueryWindowULong |
| 844 | WinQueryWindowUShort |
| 848 | WinReleasePS |
| 852 | WinSetCapture |
| 859 | WinSetDlgItemText |
| 860 | WinSetFocus |
| 865 | WinSetParent |
| 866 | WinSetPointer |
| 868 | WinSetRect |
| 875 | WinSetWindowPos |
| 877 | WinSetWindowText |
| 878 | WinSetWindowULong |
| 880 | WinShowCursor |
| 881 | WinShowPointer |
| 883 | WinShowWindow |
| 884 | WinStartTimer |
| 885 | WinStopTimer |
| 888 | WinTerminate |
| 891 | WinUnionRect |
| 892 | WinUpdateWindow |
| 895 | WinValidateRect |
| 896 | WinValidateRegion |
| 899 | WinWindowFromID |
| 902 | WinPostQueueMsg |
| 908 | WinCreateStdWindow |
| 909 | WinCreateWindow |
| 910 | WinDefDlgProc |
| 911 | WinDefWindowProc |
| 912 | WinDispatchMsg |
| 913 | WinDrawText |
| 915 | WinGetMsg |
| 918 | WinPeekMsg |
| 919 | WinPostMsg |
| 920 | WinSendMsg |
| 924 | WinLoadDlg |
| 926 | WinRegisterClass |

## PMGPI

| Ordinal | API |
| ---: | --- |
| 351 | GpiAssociate |
| 354 | GpiBeginPath |
| 355 | GpiBitBlt |
| 356 | GpiBox |
| 359 | GpiCharStringAt |
| 362 | GpiCombineRegion |
| 364 | GpiConvert |
| 368 | GpiCreateLogFont |
| 369 | GpiCreatePS |
| 370 | GpiCreateRegion |
| 371 | GpiDeleteBitmap |
| 378 | GpiDeleteSetId |
| 379 | GpiDestroyPS |
| 387 | GpiEndPath |
| 398 | GpiLine |
| 399 | GpiLoadBitmap |
| 404 | GpiMove |
| 409 | GpiPaintRegion |
| 417 | GpiPolySpline |
| 443 | GpiQueryDefaultViewMatrix |
| 453 | GpiQueryFontMetrics |
| 476 | GpiQueryPel |
| 481 | GpiQueryRegionBox |
| 489 | GpiQueryTextBox |
| 492 | GpiQueryWidthTable |
| 503 | GpiSetAttrMode |
| 504 | GpiSetBackColor |
| 505 | GpiSetBackMix |
| 506 | GpiSetBitmap |
| 513 | GpiSetCharSet |
| 515 | GpiSetClipPath |
| 516 | GpiSetClipRegion |
| 517 | GpiSetColor |
| 519 | GpiSetCurrentPosition |
| 520 | GpiSetDefaultViewMatrix |
| 530 | GpiSetLineType |
| 544 | GpiSetPel |
| 546 | GpiSetRegion |
| 588 | GpiSetAttrs |
| 592 | GpiCreateLogColorTable |
| 598 | GpiCreateBitmap |
| 599 | GpiQueryBitmapBits |
| 601 | GpiQueryBitmapInfoHeader |
| 602 | GpiSetBitmapBits |
| 604 | DevCloseDC |
| 610 | DevOpenDC |
| 611 | GpiDestroyRegion |

## PMCTLS

| Ordinal | API |
| ---: | --- |
| 4 | WinFileDlg |

## MSG

| Ordinal | API |
| ---: | --- |
| 4 | DosInsertMessage |
| 6 | DosTrueGetMessage |
