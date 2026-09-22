/*
 * cmdparse.c - Milestone 23 clex.c/cparse.c semantic lift.
 *
 * Recovered Dec-1991 NT CMD facts used here:
 *
 *   clex.c:   InitLex, Lex, TextCheck, GetByte, UnGetByte, FillBuf,
 *             LexCopy, PrintPrompt, IsData, SubVar, MSEnvVar
 *
 *   cparse.c: Parser, ParseStatement, ParseFor, ParseIf, ParseDetach,
 *             ParseRem, ParseS0..ParseS5, ParseCond, ParseArgEqArg,
 *             ParseCmd, ParseRedir, BinaryOperator, BuildArgList,
 *             GetCheckStr, GeTexTok, GeToken, LoadNodeTC, PError,
 *             PSError, SpaceCat
 *
 * The disassembly establishes the operator precedence unambiguously:
 *
 *     ParseS0  &
 *     ParseS1  ||
 *     ParseS2  &&
 *     ParseS3  |
 *     ParseS4  redirection
 *     ParseS5  command / (...) / @ / special statement forms
 *
 * This file preserves that architecture in portable C89.  It reconstructs
 * the interactive command-language subset needed by CMD32OS2 and deliberately
 * leaves FOR/IF/batch-specific statement payloads for the next milestone.
 */

#include "cmdparse.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define LEX_BUFFER_MAX 8192
#define TOKEN_TEXT_MAX 4096

#define TOK_EOL      0
#define TOK_TEXT     0x4000
#define TOK_ANDAND   0x4001
#define TOK_OROR     0x4002
#define TOK_APPEND   0x4003
#define TOK_AMP      '&'
#define TOK_PIPE     '|'
#define TOK_LT       '<'
#define TOK_GT       '>'
#define TOK_LPAREN   '('
#define TOK_RPAREN   ')'
#define TOK_AT       '@'

struct LexToken {
    int kind;
    char text[TOKEN_TEXT_MAX];
    int offset;
};

struct LexState {
    char buffer[LEX_BUFFER_MAX];
    int length;
    int pos;
    int pushed;
    int pushed_char;
};

struct ParserState {
    struct LexState lex;
    struct LexToken tok;
    int failed;
    int error_offset;
    char error[160];
};

static char *CmdDup(const char *s)
{
    size_t n;
    char *p;
    if (s == NULL)
        return NULL;
    n = strlen(s) + 1;
    p = (char *)malloc(n);
    if (p != NULL)
        memcpy(p, s, n);
    return p;
}

static int ci_starts_word(const char *s, const char *word)
{
    unsigned char a;
    unsigned char b;
    const char *p;
    const char *q;
    p = s;
    q = word;
    while (*q != '\0') {
        if (*p == '\0')
            return 0;
        a = (unsigned char)toupper((unsigned char)*p);
        b = (unsigned char)toupper((unsigned char)*q);
        if (a != b)
            return 0;
        ++p;
        ++q;
    }
    return *p == '\0' || *p == ' ' || *p == '\t';
}

static void trim_copy(char *dst, size_t cap, const char *src, size_t n)
{
    size_t first;
    size_t last;
    size_t outn;
    first = 0;
    while (first < n && (src[first] == ' ' || src[first] == '\t'))
        ++first;
    last = n;
    while (last > first && (src[last - 1] == ' ' || src[last - 1] == '\t'))
        --last;
    outn = last - first;
    if (outn >= cap)
        outn = cap - 1;
    if (outn != 0)
        memcpy(dst, src + first, outn);
    dst[outn] = '\0';
}

/* ---------------------------------------------------------------------- */
/* clex.c-shaped layer                                                     */
/* ---------------------------------------------------------------------- */

/* MSEnvVar/SubVar are reconstructed around the percent-variable behavior
 * visible in the lexer family.  Unknown %NAME% expands to an empty string,
 * while %% produces a literal percent.  Batch %0..%9 is intentionally left
 * untouched until cbatch.c is lifted. */
static int DefaultEnvLookup(void *ctx, const char *name, char *dst, size_t cap)
{
    const char *v;
    size_t n;
    (void)ctx;
    if (cap == 0)
        return 0;
    v = getenv(name);
    if (v == NULL)
        v = "";
    n = strlen(v);
    if (n + 1 > cap)
        return 0;
    memcpy(dst, v, n + 1);
    return 1;
}

static int MSEnvVar(const char *name, char *dst, size_t cap,
                    CmdEnvLookupFn lookup, void *lookup_ctx)
{
    if (lookup == NULL)
        lookup = DefaultEnvLookup;
    return lookup(lookup_ctx, name, dst, cap);
}

/* M29O1: remember the single-character variables declared by FOR statements
 * in this command line.  Environment expansion runs before ParseFor, so a
 * nested expression such as "%i-%j" must not be mistaken for one giant
 * environment reference named "i-".  The previous delimiter heuristic only
 * protected %i when it was followed by whitespace/punctuation from a small
 * allow-list and therefore collapsed the real nested-FOR regression to "j".
 *
 * This scan is intentionally lexical and conservative: it only recognizes a
 * FOR word outside double quotes, followed by whitespace and a %<char> loop
 * variable.  Once declared, every occurrence of that exact variable in the
 * same command is left for cbatch.c/eFor to substitute. */
static void MarkForVariables(const char *src, unsigned char vars[256])
{
    size_t i;
    int quoted;

    memset(vars, 0, 256);
    quoted = 0;
    i = 0;
    while (src[i] != '\0') {
        if (src[i] == '"') {
            quoted = !quoted;
            ++i;
            continue;
        }
        if (!quoted &&
            (i == 0 || src[i - 1] == ' ' || src[i - 1] == '\t' ||
             src[i - 1] == '&' || src[i - 1] == '|' || src[i - 1] == '(') &&
            src[i + 1] != '\0' && src[i + 2] != '\0' &&
            src[i + 3] != '\0' &&
            toupper((unsigned char)src[i]) == 'F' &&
            toupper((unsigned char)src[i + 1]) == 'O' &&
            toupper((unsigned char)src[i + 2]) == 'R' &&
            (src[i + 3] == ' ' || src[i + 3] == '\t')) {
            size_t j;
            j = i + 3;
            while (src[j] == ' ' || src[j] == '\t')
                ++j;
            if (src[j] == '%' && src[j + 1] != '\0' &&
                src[j + 1] != '%')
                vars[(unsigned char)src[j + 1]] = 1;
        }
        ++i;
    }
}

static int SubVar(const char *src, char *dst, size_t cap,
                  CmdEnvLookupFn lookup, void *lookup_ctx)
{
    size_t si;
    size_t di;
    size_t j;
    char name[256];
    char *value;
    size_t vn;
    int ok;
    unsigned char for_vars[256];

    /* M29K1: nested IF/FOR execution reparses command text.  Keeping an
     * 8 KB environment-expansion scratch buffer on every C/386 stack frame
     * quickly exhausts a historically sized OS/2 stack.  Make the scratch
     * buffer heap-resident instead; the lexical ABI is unchanged. */
    value = (char *)malloc(LEX_BUFFER_MAX);
    if (value == NULL)
        return 0;

    MarkForVariables(src, for_vars);
    ok = 0;
    si = 0;
    di = 0;
    while (src[si] != '\0') {
        if (src[si] != '%') {
            if (di + 1 >= cap)
                goto done;
            dst[di++] = src[si++];
            continue;
        }
        if (src[si + 1] == '%') {
            if (di + 1 >= cap)
                goto done;
            dst[di++] = '%';
            si += 2;
            continue;
        }
        /* If this command declares the character as a FOR variable, preserve
         * it regardless of the following character.  This is what keeps both
         * variables intact in nested bodies such as %%i-%%j after batch
         * percent reduction. */
        if (src[si + 1] != '\0' &&
            for_vars[(unsigned char)src[si + 1]]) {
            if (di + 1 >= cap)
                goto done;
            dst[di++] = src[si++];
            continue;
        }

        /* A single-character batch/FOR variable is not an environment
         * reference.  In particular, do not let the first %i in
         * "FOR %i IN (...) DO ... %i" consume everything up to the second
         * percent sign as one giant environment-variable name. */
        if (src[si + 1] != '\0' &&
            (isalnum((unsigned char)src[si + 1]) || src[si + 1] == '_') &&
            (src[si + 2] == '\0' || src[si + 2] == ' ' ||
             src[si + 2] == '\t' || src[si + 2] == ')' ||
             src[si + 2] == '(' || src[si + 2] == ',' ||
             src[si + 2] == '&' || src[si + 2] == '|' ||
             src[si + 2] == '<' || src[si + 2] == '>')) {
            if (di + 1 >= cap)
                goto done;
            dst[di++] = src[si++];
            continue;
        }
        /* Preserve batch-style %0..%9/%A forms without a closing %. */
        j = si + 1;
        while (src[j] != '\0' && src[j] != '%')
            ++j;
        if (src[j] != '%') {
            if (di + 1 >= cap)
                goto done;
            dst[di++] = src[si++];
            continue;
        }
        if (j == si + 1) {
            if (di + 1 >= cap)
                goto done;
            dst[di++] = '%';
            si = j + 1;
            continue;
        }
        if (j - (si + 1) >= sizeof(name))
            goto done;
        memcpy(name, src + si + 1, j - (si + 1));
        name[j - (si + 1)] = '\0';
        if (!MSEnvVar(name, value, LEX_BUFFER_MAX, lookup, lookup_ctx))
            goto done;
        vn = strlen(value);
        if (di + vn + 1 > cap)
            goto done;
        memcpy(dst + di, value, vn);
        di += vn;
        si = j + 1;
    }
    dst[di] = '\0';
    ok = 1;

done:
    free(value);
    return ok;
}

static int InitLex(struct LexState *lex, const char *line,
                   CmdEnvLookupFn lookup, void *lookup_ctx)
{
    size_t n;

    /* Expand directly into the lexer-owned 8 KB buffer instead of placing
     * another 8 KB temporary on the C/386 stack. */
    if (!SubVar(line, lex->buffer, sizeof(lex->buffer), lookup, lookup_ctx))
        return 0;
    n = strlen(lex->buffer);
    lex->length = (int)n;
    lex->pos = 0;
    lex->pushed = 0;
    lex->pushed_char = 0;
    return 1;
}

static int GetByte(struct LexState *lex)
{
    int c;
    if (lex->pushed) {
        lex->pushed = 0;
        return lex->pushed_char;
    }
    if (lex->pos >= lex->length)
        return 0;
    c = (unsigned char)lex->buffer[lex->pos++];
    return c;
}

static void UnGetByte(struct LexState *lex, int c)
{
    if (c == 0)
        return;
    lex->pushed = 1;
    lex->pushed_char = c;
}

static unsigned char parser_upper(unsigned char c)
{
    if (c >= (unsigned char)'a' && c <= (unsigned char)'z')
        return (unsigned char)(c - (unsigned char)'a' + (unsigned char)'A');
    return c;
}

static int text_starts_set_command(const char *s)
{
    unsigned char a;
    unsigned char b;
    unsigned char c;

    if (s == 0)
        return 0;
    a = parser_upper((unsigned char)s[0]);
    b = parser_upper((unsigned char)s[1]);
    c = parser_upper((unsigned char)s[2]);
    if (a != (unsigned char)'S' || b != (unsigned char)'E' ||
        c != (unsigned char)'T')
        return 0;
    return s[3] == '\0' || s[3] == ' ' || s[3] == '\t';
}

/* TextCheck isolates metacharacters only outside double quotes.  M29N1 keeps
 * parentheses literal in SET assignment text.  Host PATH values commonly
 * contain e.g. "Program Files (x86)"; those parentheses are data, not CMD
 * grouping operators.  Other metacharacters retain their normal meaning. */
static int TextCheck(int c, int quoted, int literal_parens)
{
    if (quoted)
        return 0;
    if (literal_parens && (c == '(' || c == ')'))
        return 0;
    return c == '&' || c == '|' || c == '<' || c == '>' ||
           c == '(' || c == ')' || c == '@';
}

static void LexCopy(char *dst, size_t cap, const char *src, size_t n)
{
    trim_copy(dst, cap, src, n);
}

static int IsData(const struct LexState *lex)
{
    return lex->pushed || lex->pos < lex->length;
}

/* FillBuf and PrintPrompt are preserved as architectural names.  Interactive
 * buffering/prompting lives in cmd32os2.c in this host arrangement. */
static int FillBuf(struct LexState *lex)
{
    return IsData(lex);
}

static void PrintPrompt(void)
{
    /* Intentionally empty here; cmd32os2 owns the host console prompt. */
}

static int Lex(struct LexState *lex, struct LexToken *tok)
{
    int c;
    int c2;
    int quoted;
    int start;
    int last;
    int literal_parens;

    tok->text[0] = '\0';
    tok->offset = lex->pos;

    do {
        c = GetByte(lex);
    } while (c == ' ' || c == '\t' || c == '\r');

    tok->offset = lex->pos - (c != 0 ? 1 : 0);
    if (c == 0 || c == '\n') {
        tok->kind = TOK_EOL;
        return tok->kind;
    }

    if (c == '&') {
        c2 = GetByte(lex);
        if (c2 == '&')
            tok->kind = TOK_ANDAND;
        else {
            UnGetByte(lex, c2);
            tok->kind = TOK_AMP;
        }
        return tok->kind;
    }
    if (c == '|') {
        c2 = GetByte(lex);
        if (c2 == '|')
            tok->kind = TOK_OROR;
        else {
            UnGetByte(lex, c2);
            tok->kind = TOK_PIPE;
        }
        return tok->kind;
    }
    if (c == '>') {
        c2 = GetByte(lex);
        if (c2 == '>')
            tok->kind = TOK_APPEND;
        else {
            UnGetByte(lex, c2);
            tok->kind = TOK_GT;
        }
        return tok->kind;
    }
    if (c == '<') {
        tok->kind = TOK_LT;
        return tok->kind;
    }
    if (c == '(') {
        tok->kind = TOK_LPAREN;
        return tok->kind;
    }
    if (c == ')') {
        tok->kind = TOK_RPAREN;
        return tok->kind;
    }
    if (c == '@') {
        tok->kind = TOK_AT;
        return tok->kind;
    }

    /* A TEXT token is the original command text up to the next unquoted
     * metacharacter.  This corresponds to the LexBuffer/GeTexTok boundary and
     * intentionally preserves quoting for the command handlers. */
    start = lex->pos - 1;
    quoted = (c == '"');
    literal_parens = text_starts_set_command(lex->buffer + start);
    last = lex->pos;
    for (;;) {
        c = GetByte(lex);
        if (c == 0 || c == '\n') {
            last = lex->pos - (c != 0 ? 1 : 0);
            break;
        }
        if (c == '"') {
            quoted = !quoted;
            last = lex->pos;
            continue;
        }
        if (TextCheck(c, quoted, literal_parens)) {
            UnGetByte(lex, c);
            last = lex->pos - 1;
            break;
        }
        last = lex->pos;
    }
    if (last < start)
        last = start;
    LexCopy(tok->text, sizeof(tok->text), lex->buffer + start,
            (size_t)(last - start));
    tok->kind = TOK_TEXT;
    return tok->kind;
}

/* ---------------------------------------------------------------------- */
/* cparse.c-shaped layer                                                   */
/* ---------------------------------------------------------------------- */

static void PError(struct ParserState *p, const char *msg)
{
    if (p->failed)
        return;
    p->failed = 1;
    p->error_offset = p->tok.offset;
    strncpy(p->error, msg, sizeof(p->error) - 1);
    p->error[sizeof(p->error) - 1] = '\0';
}

static void PSError(struct ParserState *p)
{
    PError(p, "The syntax of the command is incorrect.");
}

static int GeToken(struct ParserState *p)
{
    if (p->failed)
        return TOK_EOL;
    return Lex(&p->lex, &p->tok);
}

static struct CmdNode *LoadNodeTC(int type, const char *text)
{
    struct CmdNode *n;
    n = (struct CmdNode *)calloc(1, sizeof(*n));
    if (n == NULL)
        return NULL;
    n->type = type;
    if (text != NULL) {
        n->text = CmdDup(text);
        if (n->text == NULL) {
            free(n);
            return NULL;
        }
    }
    return n;
}

static char *SpaceCat(const char *a, const char *b)
{
    size_t na;
    size_t nb;
    char *p;
    na = a != NULL ? strlen(a) : 0;
    nb = b != NULL ? strlen(b) : 0;
    p = (char *)malloc(na + nb + (na && nb ? 2 : 1));
    if (p == NULL)
        return NULL;
    p[0] = '\0';
    if (na)
        strcat(p, a);
    if (na && nb)
        strcat(p, " ");
    if (nb)
        strcat(p, b);
    return p;
}

static struct CmdNode *ParseS0(struct ParserState *p);
static struct CmdNode *ParseS1(struct ParserState *p);
static struct CmdNode *ParseS2(struct ParserState *p);
static struct CmdNode *ParseS3(struct ParserState *p);
static struct CmdNode *ParseS4(struct ParserState *p);
static struct CmdNode *ParseS5(struct ParserState *p);

static struct CmdNode *BinaryOperator(struct ParserState *p,
                                      int token_kind,
                                      int node_type,
                                      struct CmdNode *(*next_level)(struct ParserState *))
{
    struct CmdNode *left;
    struct CmdNode *right;
    struct CmdNode *op;

    left = next_level(p);
    if (left == NULL)
        return NULL;
    while (!p->failed && p->tok.kind == token_kind) {
        GeToken(p);
        right = next_level(p);
        if (right == NULL) {
            CmdFreeTree(left);
            if (!p->failed)
                PSError(p);
            return NULL;
        }
        op = LoadNodeTC(node_type, NULL);
        if (op == NULL) {
            CmdFreeTree(left);
            CmdFreeTree(right);
            PError(p, "Out of memory while parsing command.");
            return NULL;
        }
        op->left = left;
        op->right = right;
        left = op;
    }
    return left;
}

/* Exact recovered precedence chain. */
static struct CmdNode *ParseS0(struct ParserState *p)
{
    return BinaryOperator(p, TOK_AMP, CMD_NODE_SEQUENCE, ParseS1);
}

static struct CmdNode *ParseS1(struct ParserState *p)
{
    return BinaryOperator(p, TOK_OROR, CMD_NODE_OR, ParseS2);
}

static struct CmdNode *ParseS2(struct ParserState *p)
{
    return BinaryOperator(p, TOK_ANDAND, CMD_NODE_AND, ParseS3);
}

static struct CmdNode *ParseS3(struct ParserState *p)
{
    return BinaryOperator(p, TOK_PIPE, CMD_NODE_PIPE, ParseS4);
}

static void trim_text_end(char *text)
{
    size_t n;
    n = strlen(text);
    while (n != 0 && (text[n - 1] == ' ' || text[n - 1] == '\t')) {
        text[n - 1] = '\0';
        --n;
    }
}

/*
 * M29M descriptor prefixes are lexical punctuation in CMD, not part of the
 * command argument.  The recovered lexer intentionally keeps TEXT spans
 * coarse, so "echo hi 2>err" arrives here as node text ending in '2' followed
 * by TOK_GT.  Use the original lexer offset to distinguish the adjacent
 * descriptor digit from an ordinary argument separated by whitespace.
 */
static int take_node_redir_fd(struct ParserState *p, struct CmdNode *node,
                              int *fd)
{
    int off;
    int c;
    size_t n;

    *fd = -1;
    off = p->tok.offset;
    if (off <= 0)
        return 1;
    c = (unsigned char)p->lex.buffer[off - 1];
    if (c < '0' || c > '2')
        return 1;
    if (node->text == NULL)
        return 1;

    n = strlen(node->text);
    if (n == 0 || node->text[n - 1] != (char)c)
        return 1;

    node->text[n - 1] = '\0';
    trim_text_end(node->text);
    *fd = c - '0';
    return 1;
}

/*
 * When another redirection follows, the next descriptor prefix can be part
 * of the current coarse TEXT token: ">out.txt 2>err.txt".  Peel that final
 * adjacent digit from the current target and carry it into the next loop.
 */
static void split_pending_redir_fd(struct ParserState *p, char *text,
                                   int *pending_fd)
{
    int off;
    int c;
    size_t n;

    *pending_fd = -1;
    if (p->tok.kind != TOK_LT && p->tok.kind != TOK_GT &&
        p->tok.kind != TOK_APPEND)
        return;

    off = p->tok.offset;
    if (off <= 0)
        return;
    c = (unsigned char)p->lex.buffer[off - 1];
    if (c < '0' || c > '2')
        return;

    n = strlen(text);
    if (n == 0 || text[n - 1] != (char)c)
        return;
    text[n - 1] = '\0';
    trim_text_end(text);
    *pending_fd = c - '0';
}

static int ParseRedir(struct ParserState *p, struct CmdNode *node)
{
    struct CmdRedir *r;
    struct CmdRedir **link;
    int type;
    int dest_fd;
    int prefix_fd;
    int pending_fd;
    int source_fd;
    char target[TOKEN_TEXT_MAX];

    link = &node->redirs;
    while (*link != NULL)
        link = &(*link)->next;

    pending_fd = -1;
    while (p->tok.kind == TOK_LT || p->tok.kind == TOK_GT ||
           p->tok.kind == TOK_APPEND) {
        if (p->tok.kind == TOK_LT) {
            type = CMD_REDIR_INPUT;
            dest_fd = 0;
        } else if (p->tok.kind == TOK_APPEND) {
            type = CMD_REDIR_APPEND;
            dest_fd = 1;
        } else {
            type = CMD_REDIR_OUTPUT;
            dest_fd = 1;
        }

        if (pending_fd >= 0) {
            dest_fd = pending_fd;
            pending_fd = -1;
        } else {
            prefix_fd = -1;
            if (!take_node_redir_fd(p, node, &prefix_fd))
                return 0;
            if (prefix_fd >= 0)
                dest_fd = prefix_fd;
        }

        if (dest_fd < 0 || dest_fd > 2 ||
            (type == CMD_REDIR_INPUT && dest_fd != 0) ||
            ((type == CMD_REDIR_OUTPUT || type == CMD_REDIR_APPEND) &&
             dest_fd == 0)) {
            PError(p, "Unsupported standard-handle redirection.");
            return 0;
        }

        GeToken(p);

        r = (struct CmdRedir *)calloc(1, sizeof(*r));
        if (r == NULL) {
            PError(p, "Out of memory while parsing redirection.");
            return 0;
        }
        r->type = type;
        r->dest_fd = dest_fd;
        r->source_fd = -1;

        /* n>&m / n<&m duplicates the descriptor as it exists at this point
         * in the left-to-right redirection list. */
        if (p->tok.kind == TOK_AMP) {
            if (type == CMD_REDIR_APPEND) {
                free(r);
                PSError(p);
                return 0;
            }
            GeToken(p);
            if (p->tok.kind != TOK_TEXT) {
                free(r);
                PSError(p);
                return 0;
            }
            if (strlen(p->tok.text) + 1 > sizeof(target)) {
                free(r);
                PError(p, "Redirection token is too long.");
                return 0;
            }
            strcpy(target, p->tok.text);
            GeToken(p);
            split_pending_redir_fd(p, target, &pending_fd);
            if (target[0] < '0' || target[0] > '2' ||
                target[1] != '\0') {
                free(r);
                PError(p, "Unsupported descriptor duplication.");
                return 0;
            }
            source_fd = target[0] - '0';
            r->type = CMD_REDIR_DUP;
            r->source_fd = source_fd;
        } else {
            if (p->tok.kind != TOK_TEXT) {
                free(r);
                PSError(p);
                return 0;
            }
            if (strlen(p->tok.text) + 1 > sizeof(target)) {
                free(r);
                PError(p, "Redirection token is too long.");
                return 0;
            }
            strcpy(target, p->tok.text);
            GeToken(p);
            split_pending_redir_fd(p, target, &pending_fd);
            if (target[0] == '\0') {
                free(r);
                PError(p, "Empty redirection target.");
                return 0;
            }
            r->target = CmdDup(target);
            if (r->target == NULL) {
                free(r);
                PError(p, "Out of memory while parsing redirection.");
                return 0;
            }
        }

        *link = r;
        link = &r->next;
    }
    return 1;
}


static struct CmdNode *ParseS4(struct ParserState *p)
{
    struct CmdNode *n;
    n = ParseS5(p);
    if (n == NULL)
        return NULL;
    if (!ParseRedir(p, n)) {
        CmdFreeTree(n);
        return NULL;
    }
    return n;
}

static struct CmdNode *ParseRem(struct ParserState *p)
{
    struct CmdNode *n;
    n = LoadNodeTC(CMD_NODE_SIMPLE, "REM");
    if (n == NULL) {
        PError(p, "Out of memory while parsing REM.");
        return NULL;
    }
    /* Historical ParseRem consumes the comment payload rather than treating
     * metacharacters inside it as executable operators. */
    while (p->tok.kind != TOK_EOL && p->tok.kind != TOK_RPAREN)
        GeToken(p);
    return n;
}

static struct CmdNode *ParseCmd(struct ParserState *p)
{
    struct CmdNode *n;
    char text[TOKEN_TEXT_MAX];
    char *joined;

    if (p->tok.kind != TOK_TEXT) {
        PSError(p);
        return NULL;
    }
    strcpy(text, p->tok.text);
    GeToken(p);

    /* The lexer normally gives us one complete textual command span.  Keep
     * this concatenation loop because it mirrors BuildArgList/SpaceCat and
     * makes the parser robust if a future Lex split produces adjacent text. */
    while (p->tok.kind == TOK_TEXT) {
        joined = SpaceCat(text, p->tok.text);
        if (joined == NULL) {
            PError(p, "Out of memory while building command text.");
            return NULL;
        }
        if (strlen(joined) + 1 > sizeof(text)) {
            free(joined);
            PError(p, "Command text is too long.");
            return NULL;
        }
        strcpy(text, joined);
        free(joined);
        GeToken(p);
    }

    if (ci_starts_word(text, "REM"))
        return LoadNodeTC(CMD_NODE_SIMPLE, text);

    n = LoadNodeTC(CMD_NODE_SIMPLE, text);
    if (n == NULL)
        PError(p, "Out of memory while parsing command.");
    return n;
}

/* ParseFor/ParseIf are explicit recovered cparse.c boundaries.  M23 keeps
 * their full text as a command node; cbatch.c semantic lifting comes next. */
static struct CmdNode *ParseFor(struct ParserState *p)
{
    struct CmdNode *n;
    char text[TOKEN_TEXT_MAX];
    size_t start;
    size_t len;

    /* The FOR set parentheses are grammar, not command-group parentheses.
     * M23's placeholder ParseCmd stopped at the first '(' and therefore could
     * never hand a normal "FOR %i IN (...) DO ..." statement to cbatch.c.
     * The recovered cparse.c has a dedicated ParseFor boundary; for M24 we
     * preserve that boundary by handing the complete expanded statement to
     * eFor/CmdBatchFor.  A later lift can split the DO body into a child AST. */
    start = (size_t)p->tok.offset;
    if (start > (size_t)p->lex.length)
        start = (size_t)p->lex.length;
    len = (size_t)p->lex.length - start;
    trim_copy(text, sizeof(text), p->lex.buffer + start, len);
    p->lex.pos = p->lex.length;
    p->lex.pushed = 0;
    GeToken(p);
    n = LoadNodeTC(CMD_NODE_SIMPLE, text);
    if (n == NULL)
        PError(p, "Out of memory while parsing FOR.");
    return n;
}

static struct CmdNode *ParseIf(struct ParserState *p)
{
    return ParseCmd(p);
}

static struct CmdNode *ParseDetach(struct ParserState *p)
{
    return ParseCmd(p);
}

static struct CmdNode *ParseS5(struct ParserState *p)
{
    struct CmdNode *child;
    struct CmdNode *group;
    int quiet;

    quiet = 0;
    while (p->tok.kind == TOK_AT) {
        quiet = 1;
        GeToken(p);
    }

    if (p->tok.kind == TOK_LPAREN) {
        GeToken(p);
        child = ParseS0(p);
        if (child == NULL)
            return NULL;
        if (p->tok.kind != TOK_RPAREN) {
            CmdFreeTree(child);
            PSError(p);
            return NULL;
        }
        GeToken(p);
        group = LoadNodeTC(CMD_NODE_GROUP, NULL);
        if (group == NULL) {
            CmdFreeTree(child);
            PError(p, "Out of memory while parsing group.");
            return NULL;
        }
        group->left = child;
        group->quiet = quiet;
        return group;
    }

    if (p->tok.kind != TOK_TEXT) {
        PSError(p);
        return NULL;
    }

    if (ci_starts_word(p->tok.text, "REM")) {
        child = ParseRem(p);
    } else if (ci_starts_word(p->tok.text, "FOR")) {
        child = ParseFor(p);
    } else if (ci_starts_word(p->tok.text, "IF")) {
        child = ParseIf(p);
    } else if (ci_starts_word(p->tok.text, "DETACH")) {
        child = ParseDetach(p);
    } else {
        child = ParseCmd(p);
    }
    if (child != NULL)
        child->quiet = quiet;
    return child;
}

static struct CmdNode *ParseStatement(struct ParserState *p)
{
    return ParseS0(p);
}

static struct CmdNode *Parser(struct ParserState *p)
{
    struct CmdNode *root;
    GeToken(p);
    if (p->tok.kind == TOK_EOL)
        return NULL;
    root = ParseStatement(p);
    if (root == NULL)
        return NULL;
    if (!p->failed && p->tok.kind != TOK_EOL) {
        CmdFreeTree(root);
        PSError(p);
        return NULL;
    }
    return root;
}

int CmdParserEx(const char *line, struct CmdParseResult *result,
                CmdEnvLookupFn lookup, void *lookup_ctx)
{
    struct ParserState *p;
    int ok;

    memset(result, 0, sizeof(*result));
    p = (struct ParserState *)calloc(1, sizeof(*p));
    if (p == NULL) {
        strcpy(result->error, "Not enough memory for command parser.");
        return 0;
    }

    ok = 0;
    if (!InitLex(&p->lex, line, lookup, lookup_ctx)) {
        strcpy(result->error, "Command line is too long after variable expansion.");
        goto done;
    }
    /* Retain recovered architectural helpers as live code paths. */
    FillBuf(&p->lex);
    PrintPrompt();

    result->root = Parser(p);
    if (p->failed) {
        result->error_offset = p->error_offset;
        strcpy(result->error, p->error);
        if (result->root != NULL) {
            CmdFreeTree(result->root);
            result->root = NULL;
        }
        goto done;
    }
    ok = 1;

done:
    free(p);
    return ok;
}

int CmdParser(const char *line, struct CmdParseResult *result)
{
    return CmdParserEx(line, result, DefaultEnvLookup, (void *)0);
}

void CmdFreeTree(struct CmdNode *node)
{
    struct CmdRedir *r;
    struct CmdRedir *next;
    if (node == NULL)
        return;
    CmdFreeTree(node->left);
    CmdFreeTree(node->right);
    r = node->redirs;
    while (r != NULL) {
        next = r->next;
        free(r->target);
        free(r);
        r = next;
    }
    free(node->text);
    free(node);
}

static const char *node_name(int type)
{
    switch (type) {
    case CMD_NODE_SIMPLE:   return "CMD";
    case CMD_NODE_SEQUENCE: return "&";
    case CMD_NODE_OR:       return "||";
    case CMD_NODE_AND:      return "&&";
    case CMD_NODE_PIPE:     return "|";
    case CMD_NODE_GROUP:    return "GROUP";
    default:                return "?";
    }
}

void CmdDumpTree(const struct CmdNode *node, int depth)
{
    const struct CmdRedir *r;
    int i;
    if (node == NULL)
        return;
    for (i = 0; i < depth; ++i)
        fputs("  ", stdout);
    printf("%s", node_name(node->type));
    if (node->quiet)
        fputs(" [@]", stdout);
    if (node->text != NULL)
        printf(" [%s]", node->text);
    for (r = node->redirs; r != NULL; r = r->next) {
        if (r->type == CMD_REDIR_DUP) {
            printf(" %d>&%d", r->dest_fd, r->source_fd);
        } else if (r->type == CMD_REDIR_INPUT) {
            printf(" %d< [%s]", r->dest_fd, r->target);
        } else if (r->type == CMD_REDIR_APPEND) {
            printf(" %d>> [%s]", r->dest_fd, r->target);
        } else {
            printf(" %d> [%s]", r->dest_fd, r->target);
        }
    }
    fputc('\n', stdout);
    CmdDumpTree(node->left, depth + 1);
    CmdDumpTree(node->right, depth + 1);
}
