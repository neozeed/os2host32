#include "os2_api_catalog.h"

#include <ctype.h>
#include <string.h>

#define OS2_API(module, ordinal, name, argbytes, route_kind) \
    { module, ordinal, name, argbytes, route_kind },

static const struct Os2ApiDescriptor api_catalog[] = {
#include "os2_api_catalog.inc"
};

#undef OS2_API

static int ascii_case_equal(const char *left, const char *right)
{
    unsigned char a;
    unsigned char b;
    if (left == NULL || right == NULL)
        return 0;
    while (*left != '\0' && *right != '\0') {
        a = (unsigned char)*left++;
        b = (unsigned char)*right++;
        if (tolower(a) != tolower(b))
            return 0;
    }
    return *left == '\0' && *right == '\0';
}

const struct Os2ApiDescriptor *os2_api_catalog(size_t *count)
{
    if (count != NULL)
        *count = sizeof(api_catalog) / sizeof(api_catalog[0]);
    return api_catalog;
}

const struct Os2ApiDescriptor *os2_api_lookup(const char *module_name,
                                               uint32_t ordinal)
{
    size_t i;
    for (i = 0; i < sizeof(api_catalog) / sizeof(api_catalog[0]); ++i) {
        if (api_catalog[i].ordinal == ordinal &&
            ascii_case_equal(api_catalog[i].module_name, module_name))
            return &api_catalog[i];
    }
    return NULL;
}

const char *os2_api_name(const char *module_name, uint32_t ordinal)
{
    const struct Os2ApiDescriptor *api;
    api = os2_api_lookup(module_name, ordinal);
    return api != NULL ? api->api_name : NULL;
}

const char *os2_api_route_name(enum Os2ApiRoute route)
{
    switch (route) {
    case OS2_API_ROUTE_SHARED:
        return "shared";
    case OS2_API_ROUTE_BACKEND:
        return "backend";
    case OS2_API_ROUTE_INTRINSIC:
        return "intrinsic";
    case OS2_API_ROUTE_NATIVE_ONLY:
        return "native-only";
    default:
        return "unknown";
    }
}
