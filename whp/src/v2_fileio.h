/* R5: guest file handles and byte-packed OS/2 file/date structures.
 * Included after guest memory helpers; all pointers are guest addresses.
 */
static int file_buffer(uint32_t va, uint32_t cb)
{
    return va != 0 && guest_range(va, cb);
}

static HANDLE os2_handle(struct Runtime *rt, uint32_t hfile)
{
    HANDLE h;
    if (hfile < 3) {
        if (rt->std_closed[hfile]) return INVALID_HANDLE_VALUE;
        h = GetStdHandle(hfile == 0 ? STD_INPUT_HANDLE :
                         (hfile == 1 ? STD_OUTPUT_HANDLE : STD_ERROR_HANDLE));
        return h ? h : INVALID_HANDLE_VALUE;
    }
    if (hfile >= MAX_FILES || !rt->files[hfile]) return INVALID_HANDLE_VALUE;
    return rt->files[hfile];
}

static uint32_t resize_file(HANDLE h, uint32_t size)
{
    LARGE_INTEGER zero, old, end;
    DWORD err;
    zero.QuadPart = 0;
    end.QuadPart = size;
    if (!SetFilePointerEx(h, zero, &old, FILE_CURRENT)) return GetLastError();
    if (!SetFilePointerEx(h, end, NULL, FILE_BEGIN)) return GetLastError();
    err = SetEndOfFile(h) ? 0 : GetLastError();
    if (!SetFilePointerEx(h, old, NULL, FILE_BEGIN) && !err) err = GetLastError();
    return err;
}

static void file_put16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8);
}

static void file_stamp(uint8_t *p, const FILETIME *ft)
{
    FILETIME local;
    WORD date = 0, time = 0;
    if (FileTimeToLocalFileTime(ft, &local))
        (void)FileTimeToDosDateTime(&local, &date, &time);
    file_put16(p, date); file_put16(p + 2, time);
}

static uint32_t file_status(uint8_t *p, DWORD attrs, DWORD high, DWORD size,
                            const FILETIME *create, const FILETIME *access,
                            const FILETIME *write)
{
    uint32_t allocation;
    if (high) return 111u; /* 32-bit FILESTATUS cannot describe this file. */
    allocation = size > 0xfffff000u ? size : (size + 4095u) & ~4095u;
    file_stamp(p, create); file_stamp(p + 4, access); file_stamp(p + 8, write);
    wr32(p + 12, size); wr32(p + 16, allocation);
    wr32(p + 20, attrs & 0x37u); /* readonly, hidden, system, directory, archive */
    return 0;
}

static uint32_t file_open(struct Runtime *rt, uint32_t esp)
{
    uint32_t a[8], i, slot, action, rc;
    DWORD access, share, disposition, attrs, result;
    HANDLE h;
    char path[1024];
    if (!file_buffer(esp + 4u, 32u)) {
        fprintf(stderr, "v2: DosOpen rejected: argument stack outside RAM ESP=%08X\n", esp);
        return OS2_ERROR_INVALID_PARAMETER;
    }
    for (i = 0; i < 8; ++i) a[i] = guest_u32(rt, esp + 4u + i * 4u);
    fprintf(stderr, "v2: DosOpen ABI8 ESP=%08X args=%08X,%08X,%08X,%08X,%08X,%08X,%08X,%08X\n",
            esp,a[0],a[1],a[2],a[3],a[4],a[5],a[6],a[7]);
    if (!a[0] || !guest_copy_cstr(rt, a[0], path, sizeof(path)) ||
        !file_buffer(a[1], 4) || !file_buffer(a[2], 4)) {
        fprintf(stderr, "v2: DosOpen rejected: invalid path or output pointer\n");
        return OS2_ERROR_INVALID_PARAMETER;
    }
    /* Common eight-argument ABI ends at pEA. Some old callers append a
       reserved word; never inspect beyond the common argument list. */
    if (a[7]) {
        fprintf(stderr, "v2: DosOpen rejected: non-null EA pointer %08X\n", a[7]);
        return OS2_ERROR_INVALID_PARAMETER;
    }
    fprintf(stderr, "v2: DosOpen path=\"%s\" size=%u flags=%08X mode=%08X\n",
           path, a[3], a[5], a[6]);
    switch (a[6] & 7u) {
    case 0: access = GENERIC_READ; break;
    case 1: access = GENERIC_WRITE; break;
    case 2: access = GENERIC_READ | GENERIC_WRITE; break;
    default: return 12u;
    }
    switch (a[6] & 0x70u) {
    case 0: case 0x40: share = FILE_SHARE_READ | FILE_SHARE_WRITE; break;
    case 0x10: share = 0; break;
    case 0x20: share = FILE_SHARE_READ; break;
    case 0x30: share = FILE_SHARE_WRITE; break;
    default: return 12u;
    }
    switch (a[5]) {
    case 1: disposition = OPEN_EXISTING; break;
    case 2: disposition = TRUNCATE_EXISTING; break;
    case 0x10: disposition = CREATE_NEW; break;
    case 0x11: disposition = OPEN_ALWAYS; break;
    case 0x12: disposition = CREATE_ALWAYS; break;
    default: return OS2_ERROR_INVALID_PARAMETER;
    }
    /* No direct disk/DASD or other unimplemented open-mode semantics. */
    if (a[6] & ~0x20F3u) return OS2_ERROR_INVALID_PARAMETER;
    if (a[4] & ~0x27u) return OS2_ERROR_INVALID_PARAMETER;
    for (slot = 3; slot < MAX_FILES; ++slot) if (!rt->files[slot]) break;
    if (slot == MAX_FILES) return 4u;
    attrs = a[4] & 0x27u;
    if (!attrs) attrs = FILE_ATTRIBUTE_NORMAL;
    if (a[6] & 0x2000u) attrs |= FILE_FLAG_WRITE_THROUGH;
    SetLastError(0);
    h = CreateFileA(path, access, share, NULL, disposition, attrs, NULL);
    result = GetLastError();
    if (h == INVALID_HANDLE_VALUE) return result;
    action = (disposition == CREATE_NEW) ? 2u :
             (disposition == TRUNCATE_EXISTING) ? 3u :
             (disposition == OPEN_EXISTING) ? 1u :
             (result == ERROR_ALREADY_EXISTS ? (disposition == CREATE_ALWAYS ? 3u : 1u) : 2u);
    if (action != 1u && a[3]) {
        rc = resize_file(h, a[3]);
        if (rc) { CloseHandle(h); return rc; }
    }
    rt->files[slot] = h;
    guest_put_u32(rt, a[1], slot); guest_put_u32(rt, a[2], action);
    fprintf(stderr, "v2: DosOpen -> hfile=%u action=%u\n", slot, action);
    return 0;
}

static uint32_t dispatch_fileio(struct Runtime *rt, uint32_t ordinal,
                                uint32_t esp)
{
    uint32_t a1 = guest_u32(rt, esp + 4u), a2 = guest_u32(rt, esp + 8u);
    uint32_t a3 = guest_u32(rt, esp + 12u), a4 = guest_u32(rt, esp + 16u);
    HANDLE h;
    DWORD done, err;
    char path[1024];
    fprintf(stderr, "v2: DOSCALLS.%u file/time API(%08X,%08X,%08X,%08X)\n", ordinal,a1,a2,a3,a4);
    switch (ordinal) {
    case 273: return file_open(rt, esp);
    case 257:
        h = os2_handle(rt, a1);
        if (h == INVALID_HANDLE_VALUE) return OS2_ERROR_INVALID_HANDLE;
        if (a1 < 3) rt->std_closed[a1] = 1; /* borrowed host standard handle */
        else {
            if (!CloseHandle(h)) return GetLastError();
            rt->files[a1] = NULL;
        }
        return 0;
    case 259:
        if (!a1 || !guest_copy_cstr(rt, a1, path, sizeof(path))) return OS2_ERROR_INVALID_PARAMETER;
        return DeleteFileA(path) ? 0 : GetLastError();
    case 272:
        h = os2_handle(rt, a1);
        return h == INVALID_HANDLE_VALUE ? OS2_ERROR_INVALID_HANDLE : resize_file(h, a2);
    case 281:
        if (!file_buffer(a4, 4) || (a3 && !file_buffer(a2, a3))) return OS2_ERROR_INVALID_PARAMETER;
        h = os2_handle(rt, a1);
        if (h == INVALID_HANDLE_VALUE) return OS2_ERROR_INVALID_HANDLE;
        done = 0;
        err = ReadFile(h, a3 ? rt->ram + a2 : rt->ram, a3, &done, NULL) ? 0 : GetLastError();
        if (err == ERROR_BROKEN_PIPE) err = 0; /* redirected stdin EOF */
        guest_put_u32(rt, a4, done);
        return err;
    case 223:
        if (!a1 || !guest_copy_cstr(rt, a1, path, sizeof(path))) return OS2_ERROR_INVALID_PARAMETER;
        if (a2 == 5) {
            if (!a4 || !file_buffer(a3, a4)) return OS2_ERROR_INVALID_PARAMETER;
            done = GetFullPathNameA(path, a4, (char *)rt->ram + a3, NULL);
            if (!done) return GetLastError();
            return done >= a4 ? 111u : 0u;
        }
        if (a2 != 1 || a4 < 24 || !file_buffer(a3, 24)) return OS2_ERROR_INVALID_PARAMETER;
        {
            WIN32_FILE_ATTRIBUTE_DATA fd;
            if (!GetFileAttributesExA(path, GetFileExInfoStandard, &fd)) return GetLastError();
            return file_status(rt->ram + a3, fd.dwFileAttributes, fd.nFileSizeHigh,
                               fd.nFileSizeLow, &fd.ftCreationTime, &fd.ftLastAccessTime, &fd.ftLastWriteTime);
        }
    case 279:
        if (a2 != 1 || a4 < 24 || !file_buffer(a3, 24)) return OS2_ERROR_INVALID_PARAMETER;
        h = os2_handle(rt, a1);
        if (h == INVALID_HANDLE_VALUE) return OS2_ERROR_INVALID_HANDLE;
        {
            BY_HANDLE_FILE_INFORMATION fi;
            if (!GetFileInformationByHandle(h, &fi)) return GetLastError();
            return file_status(rt->ram + a3, fi.dwFileAttributes, fi.nFileSizeHigh,
                               fi.nFileSizeLow, &fi.ftCreationTime, &fi.ftLastAccessTime, &fi.ftLastWriteTime);
        }
    default: return OS2_ERROR_INVALID_FUNCTION;
    }
}
