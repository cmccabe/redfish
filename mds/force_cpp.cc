/* This is here to force C++ linkage.
 *
 * Unfortunately, some versions of LevelDB only export static libraries,
 * and to link with a C++ static library, you have to use the C++ linker.
 *
 * Hopefully this will be fixed upstream soon, and this hack won't be
 * necessary.
 * Alternately, we could start bundling levelDB, and handle it that way.
 */
