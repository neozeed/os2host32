#ifndef CMDPARSE_H
#define CMDPARSE_H

#include <stddef.h>

/*
 * cmdparse.h - M23 semantic reconstruction of the Dec-1991 NT CMD
 * clex.c/cparse.c boundary.
 *
 * The public shape is intentionally small and ANSI C89-friendly.  The parser
 * implementation preserves the recovered ParseS0..ParseS5 precedence ladder
 * and the recovered lexer helper names, but it is a clean-room semantic port,
 * not claimed source-identical Microsoft code.
 */

enum CmdNodeType {
    CMD_NODE_SIMPLE = 1,
    CMD_NODE_SEQUENCE,
    CMD_NODE_OR,
    CMD_NODE_AND,
    CMD_NODE_PIPE,
    CMD_NODE_GROUP
};

enum CmdRedirType {
    CMD_REDIR_INPUT = 1,
    CMD_REDIR_OUTPUT,
    CMD_REDIR_APPEND,
    CMD_REDIR_DUP
};

struct CmdRedir {
    int type;
    int dest_fd;
    int source_fd;
    char *target;
    struct CmdRedir *next;
};

struct CmdNode {
    int type;
    int quiet;
    char *text;
    struct CmdRedir *redirs;
    struct CmdNode *left;
    struct CmdNode *right;
};

typedef int (*CmdEnvLookupFn)(void *ctx, const char *name,
                              char *dst, size_t cap);

struct CmdParseResult {
    struct CmdNode *root;
    int error_offset;
    char error[160];
};

int CmdParserEx(const char *line, struct CmdParseResult *result,
                CmdEnvLookupFn lookup, void *lookup_ctx);
int CmdParser(const char *line, struct CmdParseResult *result);
void CmdFreeTree(struct CmdNode *node);
void CmdDumpTree(const struct CmdNode *node, int depth);

#endif
