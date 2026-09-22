#ifndef CMDFILE_H
#define CMDFILE_H

/*
 * cmdfile.h - C89-oriented cfile.c/cpwork.c semantic boundary for CMD32OS2.
 *
 * These entry points intentionally mirror the recovered Dec-1991 NT CMD
 * command family.  They return a command ERRORLEVEL-style result code.
 */
int CmdFileCopy(const char *tail);
int CmdFileDelete(const char *tail);
int CmdFileRename(const char *tail);
int CmdFileMove(const char *tail);

#endif
