/* stb_image_write - v1.16 - public domain - http://nothings.org/stb
   writes out PNG/BMP/TGA/JPEG/HDR images
   by Sean Barrett and friends
   LIBRARIES:
      -lpthread (-pthread in gcc) for multithreading on Linux
      -lm               for gamma correction
   QUICK NOTES:
      Typically, you don't define STB_IMAGE_WRITE_IMPLEMENTATION, but just
      #include "stb_image_write.h" and you get the declarations.
      In ONE C/C++ file, you define STB_IMAGE_WRITE_IMPLEMENTATION and
      #include "stb_image_write.h" and you get the implementation.
      Or, if you prefer, you can use the typical two-file header/source setup.
   FULL DOCUMENTATION:
      See the main header file for documentation.
   See stb_image.h for the list of contributors.
*/

#ifndef INCLUDE_STB_IMAGE_WRITE_H
#define INCLUDE_STB_IMAGE_WRITE_H

#include <stddef.h> // size_t

#ifdef __cplusplus
extern "C" {
#endif

// The functions return a non-zero value on success.
//
// The component count 'comp' is the number of components per pixel, e.g.
// 1 for grayscale, 2 for gray+alpha, 3 for RGB, 4 for RGBA.
//
// The image 'data' will be converted to 'comp' components before writing out.
// If the components are different, they're ordered R,G,B,A for writing out.
//
// 'stride_in_bytes' is the distance in bytes from the first byte of a row of
// pixels to the first byte of the next row of pixels.

int stbi_write_png(char const *filename, int w, int h, int comp, const void *data, int stride_in_bytes);
int stbi_write_bmp(char const *filename, int w, int h, int comp, const void *data);
int stbi_write_tga(char const *filename, int w, int h, int comp, const void *data);
int stbi_write_hdr(char const *filename, int w, int h, int comp, const float *data);
int stbi_write_jpg(char const *filename, int w, int h, int comp, const void *data, int quality);

typedef void stbi_write_func(void *context, void *data, int size);

int stbi_write_png_to_func(stbi_write_func *func, void *context, int w, int h, int comp, const void *data, int stride_in_bytes);
int stbi_write_bmp_to_func(stbi_write_func *func, void *context, int w, int h, int comp, const void *data);
int stbi_write_tga_to_func(stbi_write_func *func, void *context, int w, int h, int comp, const void *data);
int stbi_write_hdr_to_func(stbi_write_func *func, void *context, int w, int h, int comp, const float *data);
int stbi_write_jpg_to_func(stbi_write_func *func, void *context, int w, int h, int comp, const void *data, int quality);

#ifdef __cplusplus
}
#endif

#endif//INCLUDE_STB_IMAGE_WRITE_H

#ifdef STB_IMAGE_WRITE_IMPLEMENTATION

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#if defined(STBIW_MALLOC) && defined(STBIW_FREE) && (defined(STBIW_REALLOC) || defined(STBIW_REALLOC_SIZED))
// ok
#elif !defined(STBIW_MALLOC) && !defined(STBIW_FREE) && !defined(STBIW_REALLOC) && !defined(STBIW_REALLOC_SIZED)
// ok
#else
#error "Must define all or none of STBIW_MALLOC, STBIW_FREE, and STBIW_REALLOC (or STBIW_REALLOC_SIZED)."
#endif

#ifndef STBIW_MALLOC
#define STBIW_MALLOC(sz)        malloc(sz)
#define STBIW_REALLOC(p,newsz)  realloc(p,newsz)
#define STBIW_FREE(p)           free(p)
#endif
#ifndef STBIW_REALLOC_SIZED
#define STBIW_REALLOC_SIZED(p,oldsz,newsz) STBIW_REALLOC(p,newsz)
#endif


#ifndef STBIW_ASSERT
#include <assert.h>
#define STBIW_ASSERT(x) assert(x)
#endif

// Forward declaration to fix compilation error
static int stbiw__bitrev(int code, int bits);

typedef struct
{
   stbi_write_func *func;
   void *context;
   unsigned char buffer[64];
   int buf_used;
} stbi__write_context;

// initialize a callback-based context
static void stbi__start_write_callbacks(stbi__write_context *s, stbi_write_func *c, void *context)
{
   s->func = c;
   s->context = context;
   s->buf_used = 0;
}

static void stbi__write_flush(stbi__write_context *s)
{
   if (s->buf_used) {
      s->func(s->context, &s->buffer, s->buf_used);
      s->buf_used = 0;
   }
}

static void stbi__putc(stbi__write_context *s, unsigned char c)
{
   if (s->buf_used >= (int) sizeof(s->buffer))
      stbi__write_flush(s);
   s->buffer[s->buf_used++] = c;
}

static void stbi__write(stbi__write_context *s, const void *p, int n)
{
   stbi__write_flush(s);
   s->func(s->context, (void*)p, n);
}

static void stbi__write_zeros(stbi__write_context *s, int n)
{
   int i;
   for (i=0; i < n; ++i)
      stbi__putc(s, 0);
}

static void stbi__write_pixel(stbi__write_context *s, int rgb_dir, int comp, int write_alpha, int expand_mono, unsigned char *d)
{
   unsigned char pixel[4];
   int i;
   if (expand_mono) {
      if (write_alpha) {
         pixel[0] = pixel[1] = pixel[2] = d[0];
         pixel[3] = 255;
      } else {
         pixel[0] = pixel[1] = pixel[2] = d[0];
      }
   } else {
      if (write_alpha) {
         if (comp == 2) { // G+A
            pixel[0] = pixel[1] = pixel[2] = d[0];
            pixel[3] = d[1];
         } else {
            pixel[0] = d[0];
            pixel[1] = d[1];
            pixel[2] = d[2];
            pixel[3] = d[3];
         }
      } else {
         pixel[0] = d[0];
         pixel[1] = d[1];
         pixel[2] = d[2];
      }
   }

   for (i=0; i < 3; ++i)
      stbi__putc(s, pixel[rgb_dir ? 2-i : i]);
   if (write_alpha)
      stbi__putc(s, pixel[3]);
}

static void stbi__write_pixels(stbi__write_context *s, int rgb_dir, int vdir, int x, int y, int comp, void *data, int write_alpha, int scanline_pad, int expand_mono)
{
   unsigned char *d = (unsigned char *) data;
   int i, j, k, j_end;

   if (vdir) {
      j_end = y;
      j = 0;
   } else {
      j_end = 0;
      j = y-1;
   }

   for (; j != j_end; vdir ? j++ : j--) {
      for (i=0; i < x; ++i) {
         stbi__write_pixel(s, rgb_dir, comp, write_alpha, expand_mono, d + (j*x+i)*comp);
      }
      stbi__write_zeros(s, scanline_pad);
   }
}

//////////////////////////////////////////////////////////////////////////////
//
// BMP writer
//

static int stbi__write_bmp_core(stbi__write_context *s, int x, int y, int comp, const void *data)
{
   int pad = (-x*3) & 3;
   int hsz = 54;
   int sz = y * (x*3 + pad);
   stbi__putc(s, 'B');
   stbi__putc(s, 'M');
   stbi__putc(s, hsz+sz);
   stbi__putc(s, 0); stbi__putc(s, 0);
   stbi__putc(s, 0); stbi__putc(s, 0);
   stbi__putc(s, hsz);
   stbi__putc(s, 0); stbi__putc(s, 0); stbi__putc(s, 0);
   stbi__putc(s, 40);
   stbi__putc(s, 0); stbi__putc(s, 0); stbi__putc(s, 0);
   stbi__putc(s, x); stbi__putc(s, x>>8); stbi__putc(s, x>>16); stbi__putc(s, x>>24);
   stbi__putc(s, y); stbi__putc(s, y>>8); stbi__putc(s, y>>16); stbi__putc(s, y>>24);
   stbi__putc(s, 1); stbi__putc(s, 0);
   stbi__putc(s, 24); stbi__putc(s, 0);
   stbi__putc(s, 0); stbi__putc(s, 0); stbi__putc(s, 0); stbi__putc(s, 0);
   stbi__putc(s, sz); stbi__putc(s, sz>>8); stbi__putc(s, sz>>16); stbi__putc(s, sz>>24);
   stbi__putc(s, 0); stbi__putc(s, 0); stbi__putc(s, 0); stbi__putc(s, 0);
   stbi__putc(s, 0); stbi__putc(s, 0); stbi__putc(s, 0); stbi__putc(s, 0);
   stbi__putc(s, 0); stbi__putc(s, 0); stbi__putc(s, 0); stbi__putc(s, 0);
   stbi__write_pixels(s, 1, 0, x, y, comp, (void *) data, 0, pad, (comp==1));
   return 1;
}

int stbi_write_bmp_to_func(stbi_write_func *func, void *context, int x, int y, int comp, const void *data)
{
   stbi__write_context s;
   stbi__start_write_callbacks(&s, func, context);
   stbi__write_bmp_core(&s, x, y, comp, data);
   stbi__write_flush(&s);
   return 1;
}

#ifndef STBI_NO_STDIO
int stbi_write_bmp(char const *filename, int x, int y, int comp, const void *data)
{
   stbi__write_context s;
   FILE *f;
   if (fopen_s(&f, filename, "wb") || !f)
      return 0;
   stbi__start_write_callbacks(&s, (stbi_write_func*)fwrite, f);
   stbi__write_bmp_core(&s, x, y, comp, data);
   stbi__write_flush(&s);
   fclose(f);
   return 1;
}
#endif

//////////////////////////////////////////////////////////////////////////////
//
// TGA writer
//

static int stbi__write_tga_core(stbi__write_context *s, int x, int y, int comp, void *data)
{
   int hsz = 18;
   stbi__putc(s, 0);
   stbi__putc(s, 0);
   stbi__putc(s, 2);
   stbi__putc(s, 0); stbi__putc(s, 0);
   stbi__putc(s, 0); stbi__putc(s, 0);
   stbi__putc(s, 0);
   stbi__putc(s, 0); stbi__putc(s, 0);
   stbi__putc(s, 0); stbi__putc(s, 0);
   stbi__putc(s, x); stbi__putc(s, x>>8);
   stbi__putc(s, y); stbi__putc(s, y>>8);
   stbi__putc(s, comp*8);
   stbi__putc(s, comp == 4 ? 8 : 0);
   stbi__write_pixels(s, 1, 0, x, y, comp, data, (comp==4), 0, (comp==1));
   return 1;
}

int stbi_write_tga_to_func(stbi_write_func *func, void *context, int x, int y, int comp, const void *data)
{
   stbi__write_context s;
   stbi__start_write_callbacks(&s, func, context);
   stbi__write_tga_core(&s, x, y, comp, (void*)data);
   stbi__write_flush(&s);
   return 1;
}

#ifndef STBI_NO_STDIO
int stbi_write_tga(char const *filename, int x, int y, int comp, const void *data)
{
   stbi__write_context s;
   FILE *f;
   if (fopen_s(&f, filename, "wb") || !f)
      return 0;
   stbi__start_write_callbacks(&s, (stbi_write_func*)fwrite, f);
   stbi__write_tga_core(&s, x, y, comp, (void *) data);
   stbi__write_flush(&s);
   fclose(f);
   return 1;
}
#endif

//
// HDR writer
//

static void stbi__write_hdr_header(stbi__write_context *s, int x, int y)
{
   char buffer[128];
   sprintf_s(buffer, sizeof(buffer), "#?RADIANCE\n# Made with stb_image_write.h\nFORMAT=32-bit_rle_rgbe\n\n-Y %d +X %d\n", y, x);
   stbi__write(s, buffer, (int)strlen(buffer));
}

static void stbi__write_hdr_scanline(stbi__write_context *s, int x, int comp, float *data)
{
   unsigned char rgbe[4];
   int i;
   rgbe[0] = 2;
   rgbe[1] = 2;
   rgbe[2] = x >> 8;
   rgbe[3] = x & 255;
   stbi__write(s, rgbe, 4);

   for(i=0; i < x; ++i) {
      float *d = data + comp*i;
      float v;
      int e;
      v = d[0]; if (d[1] > v) v = d[1]; if (d[2] > v) v = d[2];
      if (v < 1e-32) {
         rgbe[0] = rgbe[1] = rgbe[2] = rgbe[3] = 0;
      } else {
         v = (float) (frexp(v, &e) * 256.0 / v);
         rgbe[0] = (unsigned char) (d[0] * v);
         rgbe[1] = (unsigned char) (d[1] * v);
         rgbe[2] = (unsigned char) (d[2] * v);
         rgbe[3] = (unsigned char) (e + 128);
      }
      stbi__write(s, rgbe, 4);
   }
}

static int stbi__write_hdr_core(stbi__write_context *s, int x, int y, int comp, float *data)
{
   int i;
   if (comp != 3)
      return 0;

   stbi__write_hdr_header(s, x, y);
   for(i=0; i < y; ++i)
      stbi__write_hdr_scanline(s, x, comp, data + comp*x*i);
   return 1;
}

int stbi_write_hdr_to_func(stbi_write_func *func, void *context, int x, int y, int comp, const float *data)
{
   stbi__write_context s;
   stbi__start_write_callbacks(&s, func, context);
   stbi__write_hdr_core(&s, x, y, comp, (float*)data);
   stbi__write_flush(&s);
   return 1;
}

#ifndef STBI_NO_STDIO
int stbi_write_hdr(char const *filename, int x, int y, int comp, const float *data)
{
   stbi__write_context s;
   FILE *f;
   if (fopen_s(&f, filename, "wb") || !f)
      return 0;
   stbi__start_write_callbacks(&s, (stbi_write_func*)fwrite, f);
   stbi__write_hdr_core(&s, x, y, comp, (float*)data);
   stbi__write_flush(&s);
   fclose(f);
   return 1;
}
#endif

//////////////////////////////////////////////////////////////////////////////
//
// PNG writer
//

// stretchy buffer; stbiw__sbpush() == vector<>::push_back() -- stbiw__sbcount() == vector<>::size()
#define stbiw__sbraw(a) ((int *) (a) - 2)
#define stbiw__sbm(a)   stbiw__sbraw(a)[0]
#define stbiw__sbn(a)   stbiw__sbraw(a)[1]

#define stbiw__sbneedgrow(a,n)  ((a)==0 || stbiw__sbn(a)+n >= stbiw__sbm(a))
#define stbiw__sbmaybegrow(a,n) (stbiw__sbneedgrow(a,(n)) ? stbiw__sbgrow(a,n) : (void)0)
#define stbiw__sbgrow(a,n)      ((*(void **)&(a)) = stbiw__sbgrowf((a), (n), sizeof(*(a))))

#define stbiw__sbpush(a, v)      (stbiw__sbmaybegrow(a,1), (a)[stbiw__sbn(a)++] = (v))
#define stbiw__sbcount(a)        ((a) ? stbiw__sbn(a) : 0)
#define stbiw__sbfree(a)         ((a) ? STBIW_FREE(stbiw__sbraw(a)),0 : 0)

static void *stbiw__sbgrowf(void *arr, int increment, int itemsize)
{
   int m = arr ? 2*stbiw__sbm(arr)+increment : increment+1;
   void *p = STBIW_REALLOC_SIZED(arr ? stbiw__sbraw(arr) : 0, arr ? (stbiw__sbm(arr)*itemsize + sizeof(int)*2) : 0, itemsize*m + sizeof(int)*2);
   if (p) {
      if (!arr) ((int *) p)[1] = 0;
      ((int *) p)[0] = m;
      return (int *) p + 2;
   } else {
      #ifdef STBIW_ASSERT
      STBIW_ASSERT(0);
      #endif
      return (void *) (size_t) 0;
   }
}

static unsigned int stbiw__crc32_table[256] =
{
   0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3,
   0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988, 0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91,
   0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
   0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5,
   0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172, 0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,
   0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
   0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423, 0xcfba9599, 0xb8bda50f,
   0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924, 0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,
   0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
   0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01,
   0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e, 0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457,
   0x65b0d9c6, 0x12b7e950, 0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
   0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb,
   0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0, 0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9,
   0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
   0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81, 0xb7bd5c3b, 0xc0ba6cad,
   0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a, 0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683,
   0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
   0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb, 0x196c3671, 0x6e6b06e7,
   0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc, 0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5,
   0xd6d6a3e8, 0xa1d1937e, 0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
   0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55, 0x316e8eef, 0x4669be79,
   0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236, 0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f,
   0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
   0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713,
   0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38, 0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21,
   0x86d3d2d4, 0xf1d4e242, 0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
   0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45,
   0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2, 0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db,
   0xaed16a4a, 0xd9d65adc, 0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
   0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693, 0x54de5729, 0x23d967bf,
   0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94, 0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};

static unsigned int stbiw__crc32(unsigned char *buffer, int len)
{
   unsigned int crc = ~0u;
   int i;
   for (i=0; i < len; ++i)
      crc = (crc >> 8) ^ stbiw__crc32_table[buffer[i] ^ (crc & 0xff)];
   return ~crc;
}

static unsigned char stbiw__paeth(int a, int b, int c)
{
   int p = a + b - c, pa = abs(p-a), pb = abs(p-b), pc = abs(p-c);
   if (pa <= pb && pa <= pc) return (unsigned char) a;
   if (pb <= pc) return (unsigned char) b;
   return (unsigned char) c;
}

static unsigned char *stbi__zlib_compress(unsigned char *data, int data_len, int *out_len, int quality);

static int stbi__write_png_core(stbi__write_context *s, int x, int y, int comp, void *data, int stride_bytes)
{
   unsigned char *filtered;
   int i,j,k;
   stbi__putc(s, 0x89);
   stbi__putc(s, 'P'); stbi__putc(s, 'N'); stbi__putc(s, 'G');
   stbi__putc(s, 0x0d); stbi__putc(s, 0x0a); stbi__putc(s, 0x1a); stbi__putc(s, 0x0a);
   stbi__putc(s, 0); stbi__putc(s, 0); stbi__putc(s, 0); stbi__putc(s, 13);
   stbi__putc(s, 'I'); stbi__putc(s, 'H'); stbi__putc(s, 'D'); stbi__putc(s, 'R');
   stbi__putc(s, x >> 24); stbi__putc(s, x >> 16); stbi__putc(s, x >> 8); stbi__putc(s, x);
   stbi__putc(s, y >> 24); stbi__putc(s, y >> 16); stbi__putc(s, y >> 8); stbi__putc(s, y);
   stbi__putc(s, 8);
   stbi__putc(s, comp==1 ? 0 : comp==2 ? 4 : comp==3 ? 2:6);
   stbi__putc(s, 0); stbi__putc(s, 0); stbi__putc(s, 0);
   stbi__putc(s, 0); stbi__putc(s, 0); stbi__putc(s, 0); stbi__putc(s, 0); // crc
   
   filtered = (unsigned char *) STBIW_MALLOC((x*comp+1) * y);
   if (!filtered) return 0;
   for (j=0; j < y; ++j) {
      unsigned char *prev = j ? filtered + (j-1)*(x*comp+1) : 0;
      int filter = 0; // no filter
      filtered[j*(x*comp+1)] = (unsigned char) filter;
      for (k=0; k < x*comp; ++k) {
         switch (filter) {
            case 0: filtered[j*(x*comp+1)+1+k] = ((unsigned char*)data)[j*stride_bytes+k]; break;
            case 1: filtered[j*(x*comp+1)+1+k] = ((unsigned char*)data)[j*stride_bytes+k] - (k >= comp ? ((unsigned char*)data)[j*stride_bytes+k-comp] : 0); break;
            case 2: filtered[j*(x*comp+1)+1+k] = ((unsigned char*)data)[j*stride_bytes+k] - (prev ? prev[1+k] : 0); break;
            case 3: filtered[j*(x*comp+1)+1+k] = ((unsigned char*)data)[j*stride_bytes+k] - (prev ? (prev[1+k] + (k >= comp ? ((unsigned char*)data)[j*stride_bytes+k-comp] : 0)) / 2 : (k >= comp ? ((unsigned char*)data)[j*stride_bytes+k-comp] / 2 : 0)); break;
            case 4: filtered[j*(x*comp+1)+1+k] = ((unsigned char*)data)[j*stride_bytes+k] - stbiw__paeth(k >= comp ? ((unsigned char*)data)[j*stride_bytes+k-comp] : 0, prev ? prev[1+k] : 0, prev && k>=comp ? prev[1+k-comp] : 0); break;
         }
      }
   }
   unsigned char *zlib;
   int zlen;
   zlib = stbi__zlib_compress(filtered, y*(x*comp+1), &zlen, 8);
   STBIW_FREE(filtered);
   if (!zlib) return 0;

   stbi__putc(s, zlen >> 24); stbi__putc(s, zlen >> 16); stbi__putc(s, zlen >> 8); stbi__putc(s, zlen);
   stbi__putc(s, 'I'); stbi__putc(s, 'D'); stbi__putc(s, 'A'); stbi__putc(s, 'T');
   stbi__write(s, zlib, zlen);
   STBIW_FREE(zlib);
   stbi__putc(s, 0); stbi__putc(s, 0); stbi__putc(s, 0); stbi__putc(s, 0); // crc

   stbi__putc(s, 0); stbi__putc(s, 0); stbi__putc(s, 0); stbi__putc(s, 0);
   stbi__putc(s, 'I'); stbi__putc(s, 'E'); stbi__putc(s, 'N'); stbi__putc(s, 'D');
   stbi__putc(s, 0xae); stbi__putc(s, 0x42); stbi__putc(s, 0x60); stbi__putc(s, 0x82); // crc
   return 1;
}

int stbi_write_png_to_func(stbi_write_func *func, void *context, int x, int y, int comp, const void *data, int stride_bytes)
{
   stbi__write_context s;
   stbi__start_write_callbacks(&s, func, context);
   return stbi__write_png_core(&s, x, y, comp, (void *) data, stride_bytes);
}

#ifndef STBI_NO_STDIO
int stbi_write_png(char const *filename, int x, int y, int comp, const void *data, int stride_bytes)
{
   stbi__write_context s;
   FILE *f;
   if (fopen_s(&f, filename, "wb") || !f)
      return 0;
   stbi__start_write_callbacks(&s, (stbi_write_func*)fwrite, f);
   stbi__write_png_core(&s, x, y, comp, (void *) data, stride_bytes);
   stbi__write_flush(&s);
   fclose(f);
   return 1;
}
#endif

//
// JPG writer
//
#define STBIW_ZFAST_BITS 9 // accelerate all cases in default tables
#define STBIW_ZFAST_MASK ((1 << STBIW_ZFAST_BITS) - 1)

// zlib-style huffman encoding
// length code table = 257..285
// distance code table = 0..29
typedef struct
{
   unsigned short fast[1 << STBIW_ZFAST_BITS];
   unsigned short firstcode[16];
   int maxcode[17];
   unsigned short firstsymbol[16];
   unsigned char  size[288];
   unsigned short value[288];
} stbiw__zhuff;

static int stbiw__zbuild_huffman(stbiw__zhuff *z, const unsigned char *sizelist, int num)
{
   int i,k=0;
   int code, next_code[16], sizes[17];

   // DEFLATE spec for generating codes
   memset(sizes, 0, sizeof(sizes));
   memset(z->fast, 255, sizeof(z->fast));
   for (i=0; i < num; ++i)
      ++sizes[sizelist[i]];
   sizes[0] = 0;
   for (i=1; i < 16; ++i)
      if (sizes[i] > (1 << i))
         return 0; // over-subscribed--history indicates this is probably an error
   code = 0;
   for (i=1; i < 16; ++i) {
      next_code[i] = code;
      z->firstcode[i] = (unsigned short) code;
      z->firstsymbol[i] = (unsigned short) k;
      code += sizes[i];
      if (sizes[i])
         if (code-1 >= (1 << i)) return 0;
      z->maxcode[i] = code << (16-i); // preshift for inner loop
      code <<= 1;
      k += sizes[i];
   }
   z->maxcode[16] = 0x10000; // sentinel
   for (i=0; i < num; ++i) {
      int s = sizelist[i];
      if (s) {
         int c = next_code[s] - z->firstcode[s] + z->firstsymbol[s];
         unsigned short fastv = (unsigned short) ((s << 9) | i);
         z->size [c] = (unsigned char) s;
         z->value[c] = (unsigned short) i;
         if (s <= STBIW_ZFAST_BITS) {
            int j = stbiw__bitrev(next_code[s],s);
            while (j < (1 << STBIW_ZFAST_BITS)) {
               z->fast[j] = fastv;
               j += (1 << s);
            }
         }
         ++next_code[s];
      }
   }
   return 1;
}

static int stbiw__bitrev(int code, int bits)
{
   int rev=0;
   while(bits--) {
      rev = (rev << 1) | (code & 1);
      code >>= 1;
   }
   return rev;
}

static unsigned char * stbi__zlib_compress(unsigned char *data, int data_len, int *out_len, int quality)
{
   static const int MAX_LEN = 1000, MAX_DIST = 32768; // should be configurable
   // stub for now
   (void) quality;
   (void) data;
   (void) data_len;
   (void) out_len;
   return NULL;
}

int stbi_write_jpg(char const *filename, int x, int y, int comp, const void *data, int quality)
{
   // stub
   (void)filename;
   (void)x;
   (void)y;
   (void)comp;
   (void)data;
   (void)quality;
   return 0;
}

int stbi_write_jpg_to_func(stbi_write_func *func, void *context, int x, int y, int comp, const void *data, int quality)
{
   (void)func;
   (void)context;
   (void)x;
   (void)y;
   (void)comp;
   (void)data;
   (void)quality;
   return 0;
}

#endif // STB_IMAGE_WRITE_IMPLEMENTATION
