# Milestone 30M1 — declaration-order build fix

M30M1 is functionally identical to M30M.  It fixes the C declaration-order
error introduced when the new bound-EXE inference code called
`image_imports_module()` before that static function's definition.

The proper fix is a static forward declaration:

    static int image_imports_module(struct LxImage *x, const char *name);

The function itself remains `static`; there is no need to export it globally.
No EMX relocation inference behavior changed from M30M.
