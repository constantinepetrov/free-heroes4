/* GPL-3.0-or-later. C ABI for the already tested DDrawSurface implementation. */
#pragma once
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
void *h4_surface_new(int w,int h,int bits);
void h4_surface_delete(void *surface);
int h4_surface_lock(void *surface,void **pixels,size_t *bytes);
int h4_surface_unlock(void *surface,const void *pixels,size_t bytes);
int h4_surface_fill(void *surface,int x,int y,int w,int h,unsigned color);
int h4_surface_blt(void *surface,int x,int y,void *source,int sx,int sy,int w,int h,int key);
int h4_surface_flip(void *surface,void *back);
int h4_surface_palette(void *surface,unsigned index,unsigned r,unsigned g,unsigned b);
int h4_surface_present(void *surface,void *window);
void h4_surface_window_closed(void);
const char *h4_surface_error(void);
#ifdef __cplusplus
}
#endif
