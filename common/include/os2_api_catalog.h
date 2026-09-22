#ifndef OS2_API_CATALOG_H
#define OS2_API_CATALOG_H

#include <stddef.h>
#include <stdint.h>

enum Os2ApiRoute {
    OS2_API_ROUTE_SHARED = 1,
    OS2_API_ROUTE_BACKEND,
    OS2_API_ROUTE_INTRINSIC,
    OS2_API_ROUTE_NATIVE_ONLY
};

struct Os2ApiDescriptor {
    const char *module_name;
    uint32_t ordinal;
    const char *api_name;
    uint32_t argument_bytes;
    enum Os2ApiRoute route;
};

const struct Os2ApiDescriptor *os2_api_catalog(size_t *count);
const struct Os2ApiDescriptor *os2_api_lookup(const char *module_name,
                                               uint32_t ordinal);
const char *os2_api_name(const char *module_name, uint32_t ordinal);
const char *os2_api_route_name(enum Os2ApiRoute route);

#endif
