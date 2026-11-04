/* GPL-3.0-or-later. Narrow adapter for the pinned Heroes V guest allocator. */
#ifndef H4_HOST_H
#define H4_HOST_H
#include "recomp_types.h"
#include <pthread.h>
#include <stdarg.h>
#define GUEST_PAGE 0x1000u
#define GUEST_GRANULARITY 0x10000u
#define GUEST_USER_TOP 0x7FFE0000u
#define GUEST_THUNK_BASE 0x7FF00000u
#define GUEST_THUNK_LIMIT 0x7FFD0000u
#define ERROR_INVALID_PARAMETER_ 87
#define ERROR_NOT_ENOUGH_MEMORY_ 8
#define ERROR_INVALID_ADDRESS_ 487
#define ERROR_ACCESS_DENIED_ 5
void void_log(const char *fmt, ...) __attribute__((format(printf,1,2)));
_Noreturn void void_fatal(const char *fmt, ...) __attribute__((format(printf,1,2)));
void void_set_last_error(uint32_t error);
void h4_memory_status(uint32_t out);
uint32_t h4_test_api(const char *name,unsigned count,...);
void h4_need(uint32_t va,uint32_t size,int write);
uint32_t h4_arg(unsigned n);
const char *h4_string(uint32_t va);
uint32_t h4_directdraw_create(uint32_t adapter,uint32_t out,uint32_t iid,uint32_t outer);
recomp_func_t h4_ddraw_resolve(uint32_t va);
int h4_ddraw_primary_io(int write,int x,int y,int w,int h,uint16_t *data,int stride);
int h4_gdi_api(const char *name,uint32_t *result,unsigned *argc);
int h4_window_api(const char *name,uint32_t *result,unsigned *argc);
uint32_t h4_call_guest(uint32_t va,unsigned count,const uint32_t *args);
uint32_t h4_pe_resource(unsigned type,int id,uint32_t *size);
uint32_t h4_version_size(const char *path);
int h4_version_copy(const char *path,uint32_t cap,uint32_t out);
int h4_version_query(uint32_t block,const char *query,uint32_t out,uint32_t length);
int void_guest_reserve(void);
/* Win32 VirtualAlloc semantics on the guest window. Returns 0 on failure. */
uint32_t void_vm_alloc(uint32_t address, uint32_t size, uint32_t type, uint32_t protect);
int void_vm_free(uint32_t address, uint32_t size, uint32_t type);
int void_vm_protect(uint32_t address, uint32_t size, uint32_t protect, uint32_t *old);
/* Fills a MEMORY_BASIC_INFORMATION (28 bytes) at guest VA info. */
uint32_t void_vm_query(uint32_t address, uint32_t info);
int void_vm_readable(uint32_t address, uint32_t size);
int void_vm_find(const uint8_t *pattern, uint32_t length, uint32_t *hits, int max);   /* debugging */
int void_vm_dump(const char *path);                                                   /* debugging */
int void_vm_writable(uint32_t address, uint32_t size);
/* Reserve+commit an exact range (images, fixed regions); 0 on conflict. */
int void_vm_claim(uint32_t address, uint32_t size, uint32_t protect);
uint32_t void_vm_map_file(int fd, uint64_t offset, uint32_t size, int writable, int shared);
int void_vm_unmap_file(uint32_t address);

/* Win32 heaps over dlmalloc mspaces in the guest window. */
uint32_t void_heap_create(uint32_t initial, uint32_t maximum);
int void_heap_destroy(uint32_t heap);
uint32_t void_heap_alloc(uint32_t heap, uint32_t size, int zero);
int void_heap_free(uint32_t heap, uint32_t block);
uint32_t void_heap_realloc(uint32_t heap, uint32_t block, uint32_t size, int zero, int in_place_only);
uint32_t void_heap_size(uint32_t heap, uint32_t block);
uint32_t void_process_heap(void);
/* Host-side convenience: process-heap allocation for runtime structures
 * that the guest must be able to read (strings, structs). */
uint32_t void_galloc(uint32_t size);
void void_gfree(uint32_t va);
uint32_t void_gstrdup(const char *s);

/* --------------------------------------------------------- guest strings */
size_t void_gstrlen(uint32_t va);
size_t void_gwcslen(uint32_t va);
/* UTF-16 guest string -> malloc'ed UTF-8 (NULL for va == 0). */
char *void_wstr_utf8(uint32_t va);
/* ANSI (Russian system code page 1251) guest string -> malloc'ed UTF-8. */
char *void_astr_utf8(uint32_t va);
/* UTF-8 -> guest UTF-16, capacity in UTF-16 units including terminator.
 * Returns the number of units written without the terminator, or the
 * required size (with terminator) when cap is too small (and writes nothing). */
uint32_t void_utf8_to_gw(const char *s, uint32_t dst, uint32_t cap);
uint32_t void_utf8_to_ga(const char *s, uint32_t dst, uint32_t cap);
/* System ANSI CP1251 and explicit CP1252 byte conversions. */
uint16_t void_ansi_to_u16(uint8_t c);
int void_u16_to_ansi(uint16_t u, uint8_t *out);
uint8_t void_ansi_lower(uint8_t c);
uint16_t void_cp1252_to_u16(uint8_t c);
int void_u16_to_cp1252(uint16_t u, uint8_t *out);
size_t void_utf8_encode(uint32_t cp, char *out);


#endif
