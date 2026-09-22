#include <stdio.h>
#include <string.h>

#define main cmd32_personality_main
#include "cmd32os2.c"
#undef main

struct MockEntry {
    const char *name;
    unsigned long attr;
};

static const struct MockEntry table_entries[] = {
    { "table1.txt", 0UL },
    { "table2.txt", 0UL },
    { "table3.txt", 0UL }
};
static const struct MockEntry unique_entries[] = {
    { "foobar", CMDO2_ATTR_DIRECTORY }
};
static const struct MockEntry bin_entries[] = {
    { "cmd2.exe", 0UL },
    { "readme.md", 0UL }
};
static const struct MockEntry *mock_entries;
static unsigned long mock_count;
static unsigned long mock_index;
static char last_find_pattern[MAX_PATH + 4];
static unsigned long query_attr_calls;

static void mock_fill(struct CmdO2FindData *data,
                      const struct MockEntry *entry)
{
    memset(data, 0, sizeof(*data));
    strncpy(data->name, entry->name, sizeof(data->name) - 1U);
    data->name[sizeof(data->name) - 1U] = '\0';
    data->attr = entry->attr;
}

CmdO2Rc CmdO2QueryPathAttr(const char *path, unsigned long *attr)
{
    ++query_attr_calls;
    if (strcmp(path, "bin") == 0) {
        *attr = CMDO2_ATTR_DIRECTORY;
        return 0UL;
    }
    return CMDO2_ERROR_FILE_NOT_FOUND;
}

CmdO2Rc CmdO2QueryCurrentDir(char *buffer, unsigned long cb)
{
    static const char cwd[] = "C:\\OS2";
    if (cb < (unsigned long)sizeof(cwd))
        return CMDO2_ERROR_BUFFER_OVERFLOW;
    strcpy(buffer, cwd);
    return 0UL;
}

CmdO2Rc CmdO2FindFirst(const char *pattern, CmdO2FindHandle *handle,
                       struct CmdO2FindData *data)
{
    strncpy(last_find_pattern, pattern, sizeof(last_find_pattern) - 1U);
    last_find_pattern[sizeof(last_find_pattern) - 1U] = '\0';
    if (strcmp(pattern, "t*") == 0 ||
        strcmp(pattern, "C:\\TEMP\\ta*") == 0) {
        mock_entries = table_entries;
        mock_count = 3UL;
    } else if (strcmp(pattern, "foo*") == 0) {
        mock_entries = unique_entries;
        mock_count = 1UL;
    } else if (strcmp(pattern, "bin\\*") == 0) {
        mock_entries = bin_entries;
        mock_count = 2UL;
    } else {
        return CMDO2_ERROR_NO_MORE_FILES;
    }
    mock_index = 0UL;
    *handle = 1UL;
    mock_fill(data, &mock_entries[0]);
    return 0UL;
}

CmdO2Rc CmdO2FindNext(CmdO2FindHandle handle, struct CmdO2FindData *data)
{
    (void)handle;
    ++mock_index;
    if (mock_index >= mock_count)
        return CMDO2_ERROR_NO_MORE_FILES;
    mock_fill(data, &mock_entries[mock_index]);
    return 0UL;
}

CmdO2Rc CmdO2FindClose(CmdO2FindHandle handle)
{
    (void)handle;
    return 0UL;
}

static int fail(const char *what)
{
    fprintf(stderr, "FAIL: %s\n", what);
    return 1;
}

int main(void)
{
    char line[64];
    size_t start;
    size_t used;
    size_t cursor;
    int quoted;

    strcpy(line, "del t");
    used = strlen(line);
    cursor = used;
    if (!completion_token_prefix(line, cursor, &start, &quoted))
        return fail("token prefix");
    if (start != 4U || quoted || strcmp(g_completion.token, "t") != 0)
        return fail("del t token split");
    if (!completion_prepare_pattern() || strcmp(g_completion.pattern, "t*") != 0)
        return fail("t* pattern");
    if (!completion_scan_matches())
        return fail("table scan");
    if (g_completion.count != 3UL || strcmp(g_completion.common, "table") != 0)
        return fail("table common prefix");
    if (!completion_build_replacement() ||
        strcmp(g_completion.replacement, "table") != 0)
        return fail("table replacement");
    if (!editor_replace_span(line, sizeof(line), &used, &cursor,
                             start, cursor, g_completion.replacement))
        return fail("table edit");
    if (strcmp(line, "del table") != 0 || cursor != 9U)
        return fail("del table result");

    strcpy(line, "copy C:\\TEMP\\ta");
    cursor = strlen(line);
    if (!completion_token_prefix(line, cursor, &start, &quoted) ||
        !completion_prepare_pattern())
        return fail("path token setup");
    if (strcmp(g_completion.pattern, "C:\\TEMP\\ta*") != 0 ||
        strcmp(g_completion.dir_prefix, "C:\\TEMP\\") != 0 ||
        strcmp(g_completion.base_prefix, "ta") != 0)
        return fail("path prefix split");
    if (!completion_scan_matches() ||
        !completion_build_replacement() ||
        strcmp(g_completion.replacement, "C:\\TEMP\\table") != 0)
        return fail("path common replacement");

    strcpy(line, "cd foo");
    cursor = strlen(line);
    if (!completion_token_prefix(line, cursor, &start, &quoted) ||
        !completion_prepare_pattern() || !completion_scan_matches() ||
        !completion_build_replacement())
        return fail("unique directory setup");
    if (g_completion.count != 1UL ||
        strcmp(g_completion.replacement, "foobar\\") != 0)
        return fail("unique directory slash");

    strcpy(line, "del z");
    cursor = strlen(line);
    if (!completion_token_prefix(line, cursor, &start, &quoted) ||
        !completion_prepare_pattern())
        return fail("no-match setup");
    if (completion_scan_matches())
        return fail("no-match should fail");

    {
        char dir_tail[] = "bin\\";
        int want_exit;
        want_exit = 0;
        query_attr_calls = 0UL;
        last_find_pattern[0] = '\0';
        if (eDirectory(NULL, dir_tail, &want_exit) != 0)
            return fail("DIR bin\\ should enumerate directory");
        if (query_attr_calls != 0UL)
            return fail("DIR trailing slash should not require path-attr probe");
        if (strcmp(last_find_pattern, "bin\\*") != 0)
            return fail("DIR bin\\ must enumerate bin\\*");
    }

    puts("CMD32 tab-completion host core PASS");
    puts("  ambiguous t* -> table (table1/table2/table3)");
    puts("  directory prefixes are preserved");
    puts("  unique directories receive a trailing backslash");
    puts("  no-match completion leaves the editor unchanged");
    puts("  DIR path\\ enumerates path\\* without a failing attr probe");
    return 0;
}
