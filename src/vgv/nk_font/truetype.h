#pragma once

#include "internal.h"
#include "rectpack.h"
#include <math.h>
#include <stdlib.h>

/* stb_truetype.h - v1.07 - public domain */
#define NK_TT_MAX_OVERSAMPLE   8
#define NK_TT__OVER_MASK  (NK_TT_MAX_OVERSAMPLE-1)

struct nk_tt_bakedchar {
    unsigned short x0,y0,x1,y1;
    /* coordinates of bbox in bitmap */
    float xoff,yoff,xadvance;
};

struct nk_tt_aligned_quad{
    float x0,y0,s0,t0; /* top-left */
    float x1,y1,s1,t1; /* bottom-right */
};

struct nk_tt_packedchar {
    unsigned short x0,y0,x1,y1;
    /* coordinates of bbox in bitmap */
    float xoff,yoff,xadvance;
    float xoff2,yoff2;
};

struct nk_tt_pack_range {
    float font_size;
    int first_unicode_codepoint_in_range;
    /* if non-zero, then the chars are continuous, and this is the first codepoint */
    int *array_of_unicode_codepoints;
    /* if non-zero, then this is an array of unicode codepoints */
    int num_chars;
    struct nk_tt_packedchar *chardata_for_range; /* output */
    unsigned char h_oversample, v_oversample;
    /* don't set these, they're used internally */
};

struct nk_tt_pack_context {
    void *pack_info;
    int   width;
    int   height;
    int   stride_in_bytes;
    int   padding;
    unsigned int   h_oversample, v_oversample;
    unsigned char *pixels;
    void  *nodes;
};

struct nk_tt_fontinfo {
    const unsigned char* data; /* pointer to .ttf file */
    int fontstart;/* offset of start of font */
    int numGlyphs;/* number of glyphs, needed for range checking */
    int loca,head,glyf,hhea,hmtx,kern; /* table locations as offset from start of .ttf */
    int index_map; /* a cmap mapping for our chosen character encoding */
    int indexToLocFormat; /* format needed to map from glyph index to glyph */
};

enum {
  NK_TT_vmove=1,
  NK_TT_vline,
  NK_TT_vcurve
};

struct nk_tt_vertex {
    short x,y,cx,cy;
    unsigned char type,padding;
};

struct nk_tt__bitmap{
   int w,h,stride;
   unsigned char *pixels;
};

struct nk_tt__hheap_chunk {
    struct nk_tt__hheap_chunk *next;
};
struct nk_tt__hheap {
    struct nk_allocator alloc;
    struct nk_tt__hheap_chunk *head;
    void   *first_free;
    int    num_remaining_in_head_chunk;
};

struct nk_tt__edge {
    float x0,y0, x1,y1;
    int invert;
};

struct nk_tt__active_edge {
    struct nk_tt__active_edge *next;
    float fx,fdx,fdy;
    float direction;
    float sy;
    float ey;
};
struct nk_tt__point {float x,y;};

#define NK_TT_MACSTYLE_DONTCARE     0
#define NK_TT_MACSTYLE_BOLD         1
#define NK_TT_MACSTYLE_ITALIC       2
#define NK_TT_MACSTYLE_UNDERSCORE   4
#define NK_TT_MACSTYLE_NONE         8
/* <= not same as 0, this makes us check the bitfield is 0 */

enum { /* platformID */
   NK_TT_PLATFORM_ID_UNICODE   =0,
   NK_TT_PLATFORM_ID_MAC       =1,
   NK_TT_PLATFORM_ID_ISO       =2,
   NK_TT_PLATFORM_ID_MICROSOFT =3
};

enum { /* encodingID for NK_TT_PLATFORM_ID_UNICODE */
   NK_TT_UNICODE_EID_UNICODE_1_0    =0,
   NK_TT_UNICODE_EID_UNICODE_1_1    =1,
   NK_TT_UNICODE_EID_ISO_10646      =2,
   NK_TT_UNICODE_EID_UNICODE_2_0_BMP=3,
   NK_TT_UNICODE_EID_UNICODE_2_0_FULL=4
};

enum { /* encodingID for NK_TT_PLATFORM_ID_MICROSOFT */
   NK_TT_MS_EID_SYMBOL        =0,
   NK_TT_MS_EID_UNICODE_BMP   =1,
   NK_TT_MS_EID_SHIFTJIS      =2,
   NK_TT_MS_EID_UNICODE_FULL  =10
};

enum { /* encodingID for NK_TT_PLATFORM_ID_MAC; same as Script Manager codes */
   NK_TT_MAC_EID_ROMAN        =0,   NK_TT_MAC_EID_ARABIC       =4,
   NK_TT_MAC_EID_JAPANESE     =1,   NK_TT_MAC_EID_HEBREW       =5,
   NK_TT_MAC_EID_CHINESE_TRAD =2,   NK_TT_MAC_EID_GREEK        =6,
   NK_TT_MAC_EID_KOREAN       =3,   NK_TT_MAC_EID_RUSSIAN      =7
};

enum { /* languageID for NK_TT_PLATFORM_ID_MICROSOFT; same as LCID... */
       /* problematic because there are e.g. 16 english LCIDs and 16 arabic LCIDs */
   NK_TT_MS_LANG_ENGLISH     =0x0409,   NK_TT_MS_LANG_ITALIAN     =0x0410,
   NK_TT_MS_LANG_CHINESE     =0x0804,   NK_TT_MS_LANG_JAPANESE    =0x0411,
   NK_TT_MS_LANG_DUTCH       =0x0413,   NK_TT_MS_LANG_KOREAN      =0x0412,
   NK_TT_MS_LANG_FRENCH      =0x040c,   NK_TT_MS_LANG_RUSSIAN     =0x0419,
   NK_TT_MS_LANG_GERMAN      =0x0407,   NK_TT_MS_LANG_SPANISH     =0x0409,
   NK_TT_MS_LANG_HEBREW      =0x040d,   NK_TT_MS_LANG_SWEDISH     =0x041D
};

enum { /* languageID for NK_TT_PLATFORM_ID_MAC */
   NK_TT_MAC_LANG_ENGLISH      =0 ,   NK_TT_MAC_LANG_JAPANESE     =11,
   NK_TT_MAC_LANG_ARABIC       =12,   NK_TT_MAC_LANG_KOREAN       =23,
   NK_TT_MAC_LANG_DUTCH        =4 ,   NK_TT_MAC_LANG_RUSSIAN      =32,
   NK_TT_MAC_LANG_FRENCH       =1 ,   NK_TT_MAC_LANG_SPANISH      =6 ,
   NK_TT_MAC_LANG_GERMAN       =2 ,   NK_TT_MAC_LANG_SWEDISH      =5 ,
   NK_TT_MAC_LANG_HEBREW       =10,   NK_TT_MAC_LANG_CHINESE_SIMPLIFIED =33,
   NK_TT_MAC_LANG_ITALIAN      =3 ,   NK_TT_MAC_LANG_CHINESE_TRAD =19
};

#define nk_ttBYTE(p)     (* (const uint8_t *) (p))
#define nk_ttCHAR(p)     (* (const char *) (p))

#if defined(NK_BIGENDIAN) && !defined(NK_ALLOW_UNALIGNED_TRUETYPE)
   #define nk_ttUSHORT(p)   (* (uint16_t *) (p))
   #define nk_ttSHORT(p)    (* (int16_t *) (p))
   #define nk_ttULONG(p)    (* (uint32_t *) (p))
   #define nk_ttLONG(p)     (* (int16_t *) (p))
#else
    static uint16_t nk_ttUSHORT(const uint8_t *p) { return (uint16_t)(p[0]*256 + p[1]); }
    static int16_t nk_ttSHORT(const uint8_t *p)   { return (int16_t)(p[0]*256 + p[1]); }
    static uint32_t nk_ttULONG(const uint8_t *p)  { return (uint32_t)((p[0]<<24) + (p[1]<<16) + (p[2]<<8) + p[3]); }
#endif

#define nk_tt_tag4(p,c0,c1,c2,c3)\
    ((p)[0] == (c0) && (p)[1] == (c1) && (p)[2] == (c2) && (p)[3] == (c3))
#define nk_tt_tag(p,str) nk_tt_tag4(p,str[0],str[1],str[2],str[3])

static int nk_tt_GetGlyphShape(const struct nk_tt_fontinfo *info, struct nk_allocator *alloc,
                                int glyph_index, struct nk_tt_vertex **pvertices);

static uint32_t
nk_tt__find_table(const uint8_t *data, uint32_t fontstart, const char *tag)
{
    /* @OPTIMIZE: binary search */
    int16_t num_tables = nk_ttUSHORT(data+fontstart+4);
    uint32_t tabledir = fontstart + 12;
    int16_t i;
    for (i = 0; i < num_tables; ++i) {
        uint32_t loc = tabledir + (uint32_t)(16*i);
        if (nk_tt_tag(data+loc+0, tag))
            return nk_ttULONG(data+loc+8);
    }
    return 0;
}

static int
nk_tt_InitFont(struct nk_tt_fontinfo *info, const unsigned char *data2, int fontstart)
{
    uint32_t cmap, t;
    int16_t i,numTables;
    const uint8_t *data = (const uint8_t *) data2;

    info->data = data;
    info->fontstart = fontstart;

    cmap = nk_tt__find_table(data, (uint32_t)fontstart, "cmap");       /* required */
    info->loca = (int)nk_tt__find_table(data, (uint32_t)fontstart, "loca"); /* required */
    info->head = (int)nk_tt__find_table(data, (uint32_t)fontstart, "head"); /* required */
    info->glyf = (int)nk_tt__find_table(data, (uint32_t)fontstart, "glyf"); /* required */
    info->hhea = (int)nk_tt__find_table(data, (uint32_t)fontstart, "hhea"); /* required */
    info->hmtx = (int)nk_tt__find_table(data, (uint32_t)fontstart, "hmtx"); /* required */
    info->kern = (int)nk_tt__find_table(data, (uint32_t)fontstart, "kern"); /* not required */
    if (!cmap || !info->loca || !info->head || !info->glyf || !info->hhea || !info->hmtx)
        return 0;

    t = nk_tt__find_table(data, (uint32_t)fontstart, "maxp");
    if (t) info->numGlyphs = nk_ttUSHORT(data+t+4);
    else info->numGlyphs = 0xffff;

    /* find a cmap encoding table we understand *now* to avoid searching */
    /* later. (todo: could make this installable) */
    /* the same regardless of glyph. */
    numTables = nk_ttUSHORT(data + cmap + 2);
    info->index_map = 0;
    for (i=0; i < numTables; ++i)
    {
        uint32_t encoding_record = cmap + 4 + 8 * (uint32_t)i;
        /* find an encoding we understand: */
        switch(nk_ttUSHORT(data+encoding_record)) {
        case NK_TT_PLATFORM_ID_MICROSOFT:
            switch (nk_ttUSHORT(data+encoding_record+2)) {
            case NK_TT_MS_EID_UNICODE_BMP:
            case NK_TT_MS_EID_UNICODE_FULL:
                /* MS/Unicode */
                info->index_map = (int)(cmap + nk_ttULONG(data+encoding_record+4));
                break;
            default: break;
            } break;
        case NK_TT_PLATFORM_ID_UNICODE:
            /* Mac/iOS has these */
            /* all the encodingIDs are unicode, so we don't bother to check it */
            info->index_map = (int)(cmap + nk_ttULONG(data+encoding_record+4));
            break;
        default: break;
        }
    }
    if (info->index_map == 0)
        return 0;
    info->indexToLocFormat = nk_ttUSHORT(data+info->head + 50);
    return 1;
}

static int
nk_tt_FindGlyphIndex(const struct nk_tt_fontinfo *info, int unicode_codepoint)
{
    const uint8_t *data = info->data;
    uint32_t index_map = (uint32_t)info->index_map;

    uint16_t format = nk_ttUSHORT(data + index_map + 0);
    if (format == 0) { /* apple byte encoding */
        int16_t bytes = nk_ttUSHORT(data + index_map + 2);
        if (unicode_codepoint < bytes-6)
            return nk_ttBYTE(data + index_map + 6 + unicode_codepoint);
        return 0;
    } else if (format == 6) {
        uint32_t first = nk_ttUSHORT(data + index_map + 6);
        uint32_t count = nk_ttUSHORT(data + index_map + 8);
        if ((uint32_t) unicode_codepoint >= first && (uint32_t) unicode_codepoint < first+count)
            return nk_ttUSHORT(data + index_map + 10 + (unicode_codepoint - (int)first)*2);
        return 0;
    } else if (format == 2) {
        NK_ASSERT(0); /* @TODO: high-byte mapping for japanese/chinese/korean */
        return 0;
    } else if (format == 4) { /* standard mapping for windows fonts: binary search collection of ranges */
        uint16_t segcount = nk_ttUSHORT(data+index_map+6) >> 1;
        uint16_t searchRange = nk_ttUSHORT(data+index_map+8) >> 1;
        uint16_t entrySelector = nk_ttUSHORT(data+index_map+10);
        uint16_t rangeShift = nk_ttUSHORT(data+index_map+12) >> 1;

        /* do a binary search of the segments */
        uint32_t endCount = index_map + 14;
        uint32_t search = endCount;

        if (unicode_codepoint > 0xffff)
            return 0;

        /* they lie from endCount .. endCount + segCount */
        /* but searchRange is the nearest power of two, so... */
        if (unicode_codepoint >= nk_ttUSHORT(data + search + rangeShift*2))
            search += (uint32_t)(rangeShift*2);

        /* now decrement to bias correctly to find smallest */
        search -= 2;
        while (entrySelector) {
            uint16_t end;
            searchRange >>= 1;
            end = nk_ttUSHORT(data + search + searchRange*2);
            if (unicode_codepoint > end)
                search += (uint32_t)(searchRange*2);
            --entrySelector;
        }
        search += 2;

      {
         uint16_t offset, start;
         uint16_t item = (uint16_t) ((search - endCount) >> 1);

         NK_ASSERT(unicode_codepoint <= nk_ttUSHORT(data + endCount + 2*item));
         start = nk_ttUSHORT(data + index_map + 14 + segcount*2 + 2 + 2*item);
         if (unicode_codepoint < start)
            return 0;

         offset = nk_ttUSHORT(data + index_map + 14 + segcount*6 + 2 + 2*item);
         if (offset == 0)
            return (uint16_t) (unicode_codepoint + nk_ttSHORT(data + index_map + 14 + segcount*4 + 2 + 2*item));

         return nk_ttUSHORT(data + offset + (unicode_codepoint-start)*2 + index_map + 14 + segcount*6 + 2 + 2*item);
      }
   } else if (format == 12 || format == 13) {
        uint32_t ngroups = nk_ttULONG(data+index_map+12);
        int16_t low,high;
        low = 0; high = (int16_t)ngroups;
        /* Binary search the right group. */
        while (low < high) {
            int16_t mid = low + ((high-low) >> 1); /* rounds down, so low <= mid < high */
            uint32_t start_char = nk_ttULONG(data+index_map+16+mid*12);
            uint32_t end_char = nk_ttULONG(data+index_map+16+mid*12+4);
            if ((uint32_t) unicode_codepoint < start_char)
                high = mid;
            else if ((uint32_t) unicode_codepoint > end_char)
                low = mid+1;
            else {
                uint32_t start_glyph = nk_ttULONG(data+index_map+16+mid*12+8);
                if (format == 12)
                    return (int)start_glyph + (int)unicode_codepoint - (int)start_char;
                else /* format == 13 */
                    return (int)start_glyph;
            }
        }
        return 0; /* not found */
    }
    /* @TODO */
    NK_ASSERT(0);
    return 0;
}

static void
nk_tt_setvertex(struct nk_tt_vertex *v, uint8_t type, int16_t x, int16_t y, int16_t cx, int16_t cy)
{
    v->type = type;
    v->x = (int16_t) x;
    v->y = (int16_t) y;
    v->cx = (int16_t) cx;
    v->cy = (int16_t) cy;
}

static int
nk_tt__GetGlyfOffset(const struct nk_tt_fontinfo *info, int glyph_index)
{
    int g1,g2;
    if (glyph_index >= info->numGlyphs) return -1; /* glyph index out of range */
    if (info->indexToLocFormat >= 2)    return -1; /* unknown index->glyph map format */

    if (info->indexToLocFormat == 0) {
        g1 = info->glyf + nk_ttUSHORT(info->data + info->loca + glyph_index * 2) * 2;
        g2 = info->glyf + nk_ttUSHORT(info->data + info->loca + glyph_index * 2 + 2) * 2;
    } else {
        g1 = info->glyf + (int)nk_ttULONG (info->data + info->loca + glyph_index * 4);
        g2 = info->glyf + (int)nk_ttULONG (info->data + info->loca + glyph_index * 4 + 4);
    }
    return g1==g2 ? -1 : g1; /* if length is 0, return -1 */
}

static int
nk_tt_GetGlyphBox(const struct nk_tt_fontinfo *info, int glyph_index,
    int *x0, int *y0, int *x1, int *y1)
{
    int g = nk_tt__GetGlyfOffset(info, glyph_index);
    if (g < 0) return 0;

    if (x0) *x0 = nk_ttSHORT(info->data + g + 2);
    if (y0) *y0 = nk_ttSHORT(info->data + g + 4);
    if (x1) *x1 = nk_ttSHORT(info->data + g + 6);
    if (y1) *y1 = nk_ttSHORT(info->data + g + 8);
    return 1;
}

static int
nk_tt__close_shape(struct nk_tt_vertex *vertices, int num_vertices, int was_off,
    int start_off, int16_t sx, int16_t sy, int16_t scx, int16_t scy, int16_t cx, int16_t cy)
{
   if (start_off) {
      if (was_off)
         nk_tt_setvertex(&vertices[num_vertices++], NK_TT_vcurve, (cx+scx)>>1, (cy+scy)>>1, cx,cy);
      nk_tt_setvertex(&vertices[num_vertices++], NK_TT_vcurve, sx,sy,scx,scy);
   } else {
      if (was_off)
         nk_tt_setvertex(&vertices[num_vertices++], NK_TT_vcurve,sx,sy,cx,cy);
      else
         nk_tt_setvertex(&vertices[num_vertices++], NK_TT_vline,sx,sy,0,0);
   }
   return num_vertices;
}

static int
nk_tt_GetGlyphShape(const struct nk_tt_fontinfo *info, struct nk_allocator *alloc,
    int glyph_index, struct nk_tt_vertex **pvertices)
{
    int16_t numberOfContours;
    const uint8_t *endPtsOfContours;
    const uint8_t *data = info->data;
    struct nk_tt_vertex *vertices=0;
    int num_vertices=0;
    int g = nk_tt__GetGlyfOffset(info, glyph_index);
    *pvertices = 0;

    if (g < 0) return 0;
    numberOfContours = nk_ttSHORT(data + g);
    if (numberOfContours > 0) {
        uint8_t flags=0,flagcount;
        int16_t ins, i,j=0,m,n, next_move, was_off=0, off, start_off=0;
        int16_t x,y,cx,cy,sx,sy, scx,scy;
        const uint8_t *points;
        endPtsOfContours = (data + g + 10);
        ins = nk_ttUSHORT(data + g + 10 + numberOfContours * 2);
        points = data + g + 10 + numberOfContours * 2 + 2 + ins;

        n = 1+nk_ttUSHORT(endPtsOfContours + numberOfContours*2-2);
        m = n + 2*numberOfContours;  /* a loose bound on how many vertices we might need */
        vertices = (struct nk_tt_vertex *)alloc->alloc(alloc->userdata, 0, (size_t)m * sizeof(vertices[0]));
        if (vertices == 0)
            return 0;

        next_move = 0;
        flagcount=0;

        /* in first pass, we load uninterpreted data into the allocated array */
        /* above, shifted to the end of the array so we won't overwrite it when */
        /* we create our final data starting from the front */
        off = m - n; /* starting offset for uninterpreted data, regardless of how m ends up being calculated */

        /* first load flags */
        for (i=0; i < n; ++i) {
            if (flagcount == 0) {
                flags = *points++;
                if (flags & 8)
                    flagcount = *points++;
            } else --flagcount;
            vertices[off+i].type = flags;
        }

        /* now load x coordinates */
        x=0;
        for (i=0; i < n; ++i) {
            flags = vertices[off+i].type;
            if (flags & 2) {
                int16_t dx = *points++;
                x += (flags & 16) ? dx : -dx; /* ??? */
            } else {
                if (!(flags & 16)) {
                    x = x + (int16_t) (points[0]*256 + points[1]);
                    points += 2;
                }
            }
            vertices[off+i].x = (int16_t) x;
        }

        /* now load y coordinates */
        y=0;
        for (i=0; i < n; ++i) {
            flags = vertices[off+i].type;
            if (flags & 4) {
                int16_t dy = *points++;
                y += (flags & 32) ? dy : -dy; /* ??? */
            } else {
                if (!(flags & 32)) {
                    y = y + (int16_t) (points[0]*256 + points[1]);
                    points += 2;
                }
            }
            vertices[off+i].y = (int16_t) y;
        }

        /* now convert them to our format */
        num_vertices=0;
        sx = sy = cx = cy = scx = scy = 0;
        for (i=0; i < n; ++i)
        {
            flags = vertices[off+i].type;
            x     = (int16_t) vertices[off+i].x;
            y     = (int16_t) vertices[off+i].y;

            if (next_move == i) {
                if (i != 0)
                    num_vertices = nk_tt__close_shape(vertices, num_vertices, was_off, start_off, sx,sy,scx,scy,cx,cy);

                /* now start the new one                */
                start_off = !(flags & 1);
                if (start_off) {
                    /* if we start off with an off-curve point, then when we need to find a point on the curve */
                    /* where we can start, and we need to save some state for when we wraparound. */
                    scx = x;
                    scy = y;
                    if (!(vertices[off+i+1].type & 1)) {
                        /* next point is also a curve point, so interpolate an on-point curve */
                        sx = (x + (int16_t) vertices[off+i+1].x) >> 1;
                        sy = (y + (int16_t) vertices[off+i+1].y) >> 1;
                    } else {
                        /* otherwise just use the next point as our start point */
                        sx = (int16_t) vertices[off+i+1].x;
                        sy = (int16_t) vertices[off+i+1].y;
                        ++i; /* we're using point i+1 as the starting point, so skip it */
                    }
                } else {
                    sx = x;
                    sy = y;
                }
                nk_tt_setvertex(&vertices[num_vertices++], NK_TT_vmove,sx,sy,0,0);
                was_off = 0;
                next_move = 1 + nk_ttUSHORT(endPtsOfContours+j*2);
                ++j;
            } else {
                if (!(flags & 1))
                { /* if it's a curve */
                    if (was_off) /* two off-curve control points in a row means interpolate an on-curve midpoint */
                        nk_tt_setvertex(&vertices[num_vertices++], NK_TT_vcurve, (cx+x)>>1, (cy+y)>>1, cx, cy);
                    cx = x;
                    cy = y;
                    was_off = 1;
                } else {
                    if (was_off)
                        nk_tt_setvertex(&vertices[num_vertices++], NK_TT_vcurve, x,y, cx, cy);
                    else nk_tt_setvertex(&vertices[num_vertices++], NK_TT_vline, x,y,0,0);
                    was_off = 0;
                }
            }
        }
        num_vertices = nk_tt__close_shape(vertices, num_vertices, was_off, start_off, sx,sy,scx,scy,cx,cy);
    } else if (numberOfContours == -1) {
        /* Compound shapes. */
        int more = 1;
        const uint8_t *comp = data + g + 10;
        num_vertices = 0;
        vertices = 0;

        while (more)
        {
            uint16_t flags, gidx;
            int comp_num_verts = 0, i;
            struct nk_tt_vertex *comp_verts = 0, *tmp = 0;
            float mtx[6] = {1,0,0,1,0,0}, m, n;

            flags = (uint16_t)nk_ttSHORT(comp); comp+=2;
            gidx = (uint16_t)nk_ttSHORT(comp); comp+=2;

            if (flags & 2) { /* XY values */
                if (flags & 1) { /* shorts */
                    mtx[4] = nk_ttSHORT(comp); comp+=2;
                    mtx[5] = nk_ttSHORT(comp); comp+=2;
                } else {
                    mtx[4] = nk_ttCHAR(comp); comp+=1;
                    mtx[5] = nk_ttCHAR(comp); comp+=1;
                }
            } else {
                /* @TODO handle matching point */
                NK_ASSERT(0);
            }
            if (flags & (1<<3)) { /* WE_HAVE_A_SCALE */
                mtx[0] = mtx[3] = nk_ttSHORT(comp)/16384.0f; comp+=2;
                mtx[1] = mtx[2] = 0;
            } else if (flags & (1<<6)) { /* WE_HAVE_AN_X_AND_YSCALE */
                mtx[0] = nk_ttSHORT(comp)/16384.0f; comp+=2;
                mtx[1] = mtx[2] = 0;
                mtx[3] = nk_ttSHORT(comp)/16384.0f; comp+=2;
            } else if (flags & (1<<7)) { /* WE_HAVE_A_TWO_BY_TWO */
                mtx[0] = nk_ttSHORT(comp)/16384.0f; comp+=2;
                mtx[1] = nk_ttSHORT(comp)/16384.0f; comp+=2;
                mtx[2] = nk_ttSHORT(comp)/16384.0f; comp+=2;
                mtx[3] = nk_ttSHORT(comp)/16384.0f; comp+=2;
            }

             /* Find transformation scales. */
            m = (float) sqrt(mtx[0]*mtx[0] + mtx[1]*mtx[1]);
            n = (float) sqrt(mtx[2]*mtx[2] + mtx[3]*mtx[3]);

             /* Get indexed glyph. */
            comp_num_verts = nk_tt_GetGlyphShape(info, alloc, gidx, &comp_verts);
            if (comp_num_verts > 0)
            {
                /* Transform vertices. */
                for (i = 0; i < comp_num_verts; ++i) {
                    struct nk_tt_vertex* v = &comp_verts[i];
                    short x,y;
                    x=v->x; y=v->y;
                    v->x = (short)(m * (mtx[0]*x + mtx[2]*y + mtx[4]));
                    v->y = (short)(n * (mtx[1]*x + mtx[3]*y + mtx[5]));
                    x=v->cx; y=v->cy;
                    v->cx = (short)(m * (mtx[0]*x + mtx[2]*y + mtx[4]));
                    v->cy = (short)(n * (mtx[1]*x + mtx[3]*y + mtx[5]));
                }
                /* Append vertices. */
                tmp = (struct nk_tt_vertex*)alloc->alloc(alloc->userdata, 0,
                    (size_t)(num_vertices+comp_num_verts)*sizeof(struct nk_tt_vertex));
                if (!tmp) {
                    if (vertices) alloc->free(alloc->userdata, vertices);
                    if (comp_verts) alloc->free(alloc->userdata, comp_verts);
                    return 0;
                }
                if (num_vertices > 0) memcpy(tmp, vertices, (size_t)num_vertices*sizeof(struct nk_tt_vertex));
                memcpy(tmp+num_vertices, comp_verts, (size_t)comp_num_verts*sizeof(struct nk_tt_vertex));
                if (vertices) alloc->free(alloc->userdata,vertices);
                vertices = tmp;
                alloc->free(alloc->userdata,comp_verts);
                num_vertices += comp_num_verts;
            }
            /* More components ? */
            more = flags & (1<<5);
        }
    } else if (numberOfContours < 0) {
        /* @TODO other compound variations? */
        NK_ASSERT(0);
    } else {
        /* numberOfCounters == 0, do nothing */
    }
    *pvertices = vertices;
    return num_vertices;
}

static void
nk_tt_GetGlyphHMetrics(const struct nk_tt_fontinfo *info, int glyph_index,
    int *advanceWidth, int *leftSideBearing)
{
    uint16_t numOfLongHorMetrics = nk_ttUSHORT(info->data+info->hhea + 34);
    if (glyph_index < numOfLongHorMetrics) {
        if (advanceWidth)
            *advanceWidth    = nk_ttSHORT(info->data + info->hmtx + 4*glyph_index);
        if (leftSideBearing)
            *leftSideBearing = nk_ttSHORT(info->data + info->hmtx + 4*glyph_index + 2);
    } else {
        if (advanceWidth)
            *advanceWidth    = nk_ttSHORT(info->data + info->hmtx + 4*(numOfLongHorMetrics-1));
        if (leftSideBearing)
            *leftSideBearing = nk_ttSHORT(info->data + info->hmtx + 4*numOfLongHorMetrics + 2*(glyph_index - numOfLongHorMetrics));
    }
}

static void
nk_tt_GetFontVMetrics(const struct nk_tt_fontinfo *info,
    int *ascent, int *descent, int *lineGap)
{
   if (ascent ) *ascent  = nk_ttSHORT(info->data+info->hhea + 4);
   if (descent) *descent = nk_ttSHORT(info->data+info->hhea + 6);
   if (lineGap) *lineGap = nk_ttSHORT(info->data+info->hhea + 8);
}

static float
nk_tt_ScaleForPixelHeight(const struct nk_tt_fontinfo *info, float height)
{
   int fheight = nk_ttSHORT(info->data + info->hhea + 4) - nk_ttSHORT(info->data + info->hhea + 6);
   return (float) height / (float)fheight;
}

static float
nk_tt_ScaleForMappingEmToPixels(const struct nk_tt_fontinfo *info, float pixels)
{
   int unitsPerEm = nk_ttUSHORT(info->data + info->head + 18);
   return pixels / (float)unitsPerEm;
}

/*-------------------------------------------------------------
 *            antialiasing software rasterizer
 * --------------------------------------------------------------*/
static void
nk_tt_GetGlyphBitmapBoxSubpixel(const struct nk_tt_fontinfo *font,
    int glyph, float scale_x, float scale_y,float shift_x, float shift_y,
    int *ix0, int *iy0, int *ix1, int *iy1)
{
    int x0,y0,x1,y1;
    if (!nk_tt_GetGlyphBox(font, glyph, &x0,&y0,&x1,&y1)) {
        /* e.g. space character */
        if (ix0) *ix0 = 0;
        if (iy0) *iy0 = 0;
        if (ix1) *ix1 = 0;
        if (iy1) *iy1 = 0;
    } else {
        /* move to integral bboxes (treating pixels as little squares, what pixels get touched)? */
        if (ix0) *ix0 = nk_ifloorf((float)x0 * scale_x + shift_x);
        if (iy0) *iy0 = nk_ifloorf((float)-y1 * scale_y + shift_y);
        if (ix1) *ix1 = nk_iceilf ((float)x1 * scale_x + shift_x);
        if (iy1) *iy1 = nk_iceilf ((float)-y0 * scale_y + shift_y);
    }
}

static void
nk_tt_GetGlyphBitmapBox(const struct nk_tt_fontinfo *font, int glyph,
    float scale_x, float scale_y, int *ix0, int *iy0, int *ix1, int *iy1)
{
   nk_tt_GetGlyphBitmapBoxSubpixel(font, glyph, scale_x, scale_y,0.0f,0.0f, ix0, iy0, ix1, iy1);
}

/*-------------------------------------------------------------
 *                          Rasterizer
 * --------------------------------------------------------------*/
static void*
nk_tt__hheap_alloc(struct nk_tt__hheap *hh, size_t size)
{
    if (hh->first_free) {
        void *p = hh->first_free;
        hh->first_free = * (void **) p;
        return p;
    } else {
        if (hh->num_remaining_in_head_chunk == 0) {
            int count = (size < 32 ? 2000 : size < 128 ? 800 : 100);
            struct nk_tt__hheap_chunk *c = (struct nk_tt__hheap_chunk *)
                hh->alloc.alloc(hh->alloc.userdata, 0,
                sizeof(struct nk_tt__hheap_chunk) + size * (size_t)count);
            if (c == 0) return 0;
            c->next = hh->head;
            hh->head = c;
            hh->num_remaining_in_head_chunk = count;
        }
        --hh->num_remaining_in_head_chunk;
        return (char *) (hh->head) + size * (size_t)hh->num_remaining_in_head_chunk;
    }
}

static void
nk_tt__hheap_free(struct nk_tt__hheap *hh, void *p)
{
    *(void **) p = hh->first_free;
    hh->first_free = p;
}

static void
nk_tt__hheap_cleanup(struct nk_tt__hheap *hh)
{
    struct nk_tt__hheap_chunk *c = hh->head;
    while (c) {
        struct nk_tt__hheap_chunk *n = c->next;
        hh->alloc.free(hh->alloc.userdata, c);
        c = n;
    }
}

static struct nk_tt__active_edge*
nk_tt__new_active(struct nk_tt__hheap *hh, struct nk_tt__edge *e,
    int off_x, float start_point)
{
    struct nk_tt__active_edge *z = (struct nk_tt__active_edge *)
        nk_tt__hheap_alloc(hh, sizeof(*z));
    float dxdy = (e->x1 - e->x0) / (e->y1 - e->y0);
    /*STBTT_assert(e->y0 <= start_point); */
    if (!z) return z;
    z->fdx = dxdy;
    z->fdy = (dxdy != 0) ? (1/dxdy): 0;
    z->fx = e->x0 + dxdy * (start_point - e->y0);
    z->fx -= (float)off_x;
    z->direction = e->invert ? 1.0f : -1.0f;
    z->sy = e->y0;
    z->ey = e->y1;
    z->next = 0;
    return z;
}

static void
nk_tt__handle_clipped_edge(float *scanline, int x, struct nk_tt__active_edge *e,
    float x0, float y0, float x1, float y1)
{
    if (y0 == y1) return;
    NK_ASSERT(y0 < y1);
    NK_ASSERT(e->sy <= e->ey);
    if (y0 > e->ey) return;
    if (y1 < e->sy) return;
    if (y0 < e->sy) {
        x0 += (x1-x0) * (e->sy - y0) / (y1-y0);
        y0 = e->sy;
    }
    if (y1 > e->ey) {
        x1 += (x1-x0) * (e->ey - y1) / (y1-y0);
        y1 = e->ey;
    }

    if (x0 == x) NK_ASSERT(x1 <= x+1);
    else if (x0 == x+1) NK_ASSERT(x1 >= x);
    else if (x0 <= x) NK_ASSERT(x1 <= x);
    else if (x0 >= x+1) NK_ASSERT(x1 >= x+1);
    else NK_ASSERT(x1 >= x && x1 <= x+1);

    if (x0 <= x && x1 <= x)
        scanline[x] += e->direction * (y1-y0);
    else if (x0 >= x+1 && x1 >= x+1);
    else {
        NK_ASSERT(x0 >= x && x0 <= x+1 && x1 >= x && x1 <= x+1);
        /* coverage = 1 - average x position */
        scanline[x] += (float)e->direction * (float)(y1-y0) * (1.0f-((x0-(float)x)+(x1-(float)x))/2.0f);
    }
}

static void
nk_tt__fill_active_edges_new(float *scanline, float *scanline_fill, int len,
    struct nk_tt__active_edge *e, float y_top)
{
    float y_bottom = y_top+1;
    while (e)
    {
        /* brute force every pixel */
        /* compute intersection points with top & bottom */
        NK_ASSERT(e->ey >= y_top);
        if (e->fdx == 0) {
            float x0 = e->fx;
            if (x0 < len) {
                if (x0 >= 0) {
                    nk_tt__handle_clipped_edge(scanline,(int) x0,e, x0,y_top, x0,y_bottom);
                    nk_tt__handle_clipped_edge(scanline_fill-1,(int) x0+1,e, x0,y_top, x0,y_bottom);
                } else {
                    nk_tt__handle_clipped_edge(scanline_fill-1,0,e, x0,y_top, x0,y_bottom);
                }
            }
        } else {
            float x0 = e->fx;
            float dx = e->fdx;
            float xb = x0 + dx;
            float x_top, x_bottom;
            float y0,y1;
            float dy = e->fdy;
            NK_ASSERT(e->sy <= y_bottom && e->ey >= y_top);

            /* compute endpoints of line segment clipped to this scanline (if the */
            /* line segment starts on this scanline. x0 is the intersection of the */
            /* line with y_top, but that may be off the line segment. */
            if (e->sy > y_top) {
                x_top = x0 + dx * (e->sy - y_top);
                y0 = e->sy;
            } else {
                x_top = x0;
                y0 = y_top;
            }

            if (e->ey < y_bottom) {
                x_bottom = x0 + dx * (e->ey - y_top);
                y1 = e->ey;
            } else {
                x_bottom = xb;
                y1 = y_bottom;
            }

            if (x_top >= 0 && x_bottom >= 0 && x_top < len && x_bottom < len)
            {
                /* from here on, we don't have to range check x values */
                if ((int) x_top == (int) x_bottom) {
                    float height;
                    /* simple case, only spans one pixel */
                    int x = (int) x_top;
                    height = y1 - y0;
                    NK_ASSERT(x >= 0 && x < len);
                    scanline[x] += e->direction * (1.0f-(((float)x_top - (float)x) + ((float)x_bottom-(float)x))/2.0f)  * (float)height;
                    scanline_fill[x] += e->direction * (float)height; /* everything right of this pixel is filled */
                } else {
                    int x,x1,x2;
                    float y_crossing, step, sign, area;
                    /* covers 2+ pixels */
                    if (x_top > x_bottom)
                    {
                        /* flip scanline vertically; signed area is the same */
                        float t;
                        y0 = y_bottom - (y0 - y_top);
                        y1 = y_bottom - (y1 - y_top);
                        t = y0; y0 = y1; y1 = t;
                        t = x_bottom; x_bottom = x_top; x_top = t;
                        dx = -dx;
                        dy = -dy;
                        t = x0; x0 = xb; xb = t;
                    }

                    x1 = (int) x_top;
                    x2 = (int) x_bottom;
                    /* compute intersection with y axis at x1+1 */
                    y_crossing = ((float)x1+1 - (float)x0) * (float)dy + (float)y_top;

                    sign = e->direction;
                    /* area of the rectangle covered from y0..y_crossing */
                    area = sign * (y_crossing-y0);
                    /* area of the triangle (x_top,y0), (x+1,y0), (x+1,y_crossing) */
                    scanline[x1] += area * (1.0f-((float)((float)x_top - (float)x1)+(float)(x1+1-x1))/2.0f);

                    step = sign * dy;
                    for (x = x1+1; x < x2; ++x) {
                        scanline[x] += area + step/2;
                        area += step;
                    }
                    y_crossing += (float)dy * (float)(x2 - (x1+1));

                    scanline[x2] += area + sign * (1.0f-((float)(x2-x2)+((float)x_bottom-(float)x2))/2.0f) * (y1-y_crossing);
                    scanline_fill[x2] += sign * (y1-y0);
                }
            }
            else
            {
                /* if edge goes outside of box we're drawing, we require */
                /* clipping logic. since this does not match the intended use */
                /* of this library, we use a different, very slow brute */
                /* force implementation */
                int x;
                for (x=0; x < len; ++x)
                {
                    /* cases: */
                    /* */
                    /* there can be up to two intersections with the pixel. any intersection */
                    /* with left or right edges can be handled by splitting into two (or three) */
                    /* regions. intersections with top & bottom do not necessitate case-wise logic. */
                    /* */
                    /* the old way of doing this found the intersections with the left & right edges, */
                    /* then used some simple logic to produce up to three segments in sorted order */
                    /* from top-to-bottom. however, this had a problem: if an x edge was epsilon */
                    /* across the x border, then the corresponding y position might not be distinct */
                    /* from the other y segment, and it might ignored as an empty segment. to avoid */
                    /* that, we need to explicitly produce segments based on x positions. */

                    /* rename variables to clear pairs */
                    float ya = y_top;
                    float x1 = (float) (x);
                    float x2 = (float) (x+1);
                    float x3 = xb;
                    float y3 = y_bottom;
                    float yb,y2;

                    yb = ((float)x - x0) / dx + y_top;
                    y2 = ((float)x+1 - x0) / dx + y_top;

                    if (x0 < x1 && x3 > x2) {         /* three segments descending down-right */
                        nk_tt__handle_clipped_edge(scanline,x,e, x0,ya, x1,yb);
                        nk_tt__handle_clipped_edge(scanline,x,e, x1,yb, x2,y2);
                        nk_tt__handle_clipped_edge(scanline,x,e, x2,y2, x3,y3);
                    } else if (x3 < x1 && x0 > x2) {  /* three segments descending down-left */
                        nk_tt__handle_clipped_edge(scanline,x,e, x0,ya, x2,y2);
                        nk_tt__handle_clipped_edge(scanline,x,e, x2,y2, x1,yb);
                        nk_tt__handle_clipped_edge(scanline,x,e, x1,yb, x3,y3);
                    } else if (x0 < x1 && x3 > x1) {  /* two segments across x, down-right */
                        nk_tt__handle_clipped_edge(scanline,x,e, x0,ya, x1,yb);
                        nk_tt__handle_clipped_edge(scanline,x,e, x1,yb, x3,y3);
                    } else if (x3 < x1 && x0 > x1) {  /* two segments across x, down-left */
                        nk_tt__handle_clipped_edge(scanline,x,e, x0,ya, x1,yb);
                        nk_tt__handle_clipped_edge(scanline,x,e, x1,yb, x3,y3);
                    } else if (x0 < x2 && x3 > x2) {  /* two segments across x+1, down-right */
                        nk_tt__handle_clipped_edge(scanline,x,e, x0,ya, x2,y2);
                        nk_tt__handle_clipped_edge(scanline,x,e, x2,y2, x3,y3);
                    } else if (x3 < x2 && x0 > x2) {  /* two segments across x+1, down-left */
                        nk_tt__handle_clipped_edge(scanline,x,e, x0,ya, x2,y2);
                        nk_tt__handle_clipped_edge(scanline,x,e, x2,y2, x3,y3);
                    } else {  /* one segment */
                        nk_tt__handle_clipped_edge(scanline,x,e, x0,ya, x3,y3);
                    }
                }
            }
        }
        e = e->next;
    }
}

/* directly AA rasterize edges w/o supersampling */
static void
nk_tt__rasterize_sorted_edges(struct nk_tt__bitmap *result, struct nk_tt__edge *e,
    int n, int vsubsample, int off_x, int off_y, struct nk_allocator *alloc)
{
    struct nk_tt__hheap hh;
    struct nk_tt__active_edge *active = 0;
    int y,j=0, i;
    float scanline_data[129], *scanline, *scanline2;

    ((void)(vsubsample));
    nk_zero_struct(hh);
    hh.alloc = *alloc;

    if (result->w > 64)
        scanline = (float *) alloc->alloc(alloc->userdata,0, (size_t)(result->w*2+1) * sizeof(float));
    else scanline = scanline_data;

    scanline2 = scanline + result->w;
    y = off_y;
    e[n].y0 = (float) (off_y + result->h) + 1;

    while (j < result->h)
    {
        /* find center of pixel for this scanline */
        float scan_y_top    = (float)y + 0.0f;
        float scan_y_bottom = (float)y + 1.0f;
        struct nk_tt__active_edge **step = &active;

        memset(scanline , 0, (size_t)result->w*sizeof(scanline[0]));
        memset(scanline2, 0, (size_t)(result->w+1)*sizeof(scanline[0]));

        /* update all active edges; */
        /* remove all active edges that terminate before the top of this scanline */
        while (*step) {
            struct nk_tt__active_edge * z = *step;
            if (z->ey <= scan_y_top) {
                *step = z->next; /* delete from list */
                NK_ASSERT(z->direction);
                z->direction = 0;
                nk_tt__hheap_free(&hh, z);
            } else {
                step = &((*step)->next); /* advance through list */
            }
        }

        /* insert all edges that start before the bottom of this scanline */
        while (e->y0 <= scan_y_bottom) {
            if (e->y0 != e->y1) {
                struct nk_tt__active_edge *z = nk_tt__new_active(&hh, e, off_x, scan_y_top);
                if (z != 0) {
                    NK_ASSERT(z->ey >= scan_y_top);
                    /* insert at front */
                    z->next = active;
                    active = z;
                }
            }
            ++e;
        }

        /* now process all active edges */
        if (active)
            nk_tt__fill_active_edges_new(scanline, scanline2+1, result->w, active, scan_y_top);

        {
            float sum = 0;
            for (i=0; i < result->w; ++i) {
                float k;
                int m;
                sum += scanline2[i];
                k = scanline[i] + sum;
                k = (float) abs(k) * 255.0f + 0.5f;
                m = (int) k;
                if (m > 255) m = 255;
                result->pixels[j*result->stride + i] = (unsigned char) m;
            }
        }
        /* advance all the edges */
        step = &active;
        while (*step) {
            struct nk_tt__active_edge *z = *step;
            z->fx += z->fdx; /* advance to position for current scanline */
            step = &((*step)->next); /* advance through list */
        }
        ++y;
        ++j;
    }
    nk_tt__hheap_cleanup(&hh);
    if (scanline != scanline_data)
        alloc->free(alloc->userdata, scanline);
}

#define NK_TT__COMPARE(a,b)  ((a)->y0 < (b)->y0)
static void
nk_tt__sort_edges_ins_sort(struct nk_tt__edge *p, int n)
{
    int i,j;
    for (i=1; i < n; ++i) {
        struct nk_tt__edge t = p[i], *a = &t;
        j = i;
        while (j > 0) {
            struct nk_tt__edge *b = &p[j-1];
            int c = NK_TT__COMPARE(a,b);
            if (!c) break;
            p[j] = p[j-1];
            --j;
        }
        if (i != j)
            p[j] = t;
    }
}

static void
nk_tt__sort_edges_quicksort(struct nk_tt__edge *p, int n)
{
    /* threshold for transitioning to insertion sort */
    while (n > 12) {
        struct nk_tt__edge t;
        int c01,c12,c,m,i,j;

        /* compute median of three */
        m = n >> 1;
        c01 = NK_TT__COMPARE(&p[0],&p[m]);
        c12 = NK_TT__COMPARE(&p[m],&p[n-1]);

        /* if 0 >= mid >= end, or 0 < mid < end, then use mid */
        if (c01 != c12) {
            /* otherwise, we'll need to swap something else to middle */
            int z;
            c = NK_TT__COMPARE(&p[0],&p[n-1]);
            /* 0>mid && mid<n:  0>n => n; 0<n => 0 */
            /* 0<mid && mid>n:  0>n => 0; 0<n => n */
            z = (c == c12) ? 0 : n-1;
            t = p[z];
            p[z] = p[m];
            p[m] = t;
        }

        /* now p[m] is the median-of-three */
        /* swap it to the beginning so it won't move around */
        t = p[0];
        p[0] = p[m];
        p[m] = t;

        /* partition loop */
        i=1;
        j=n-1;
        for(;;) {
            /* handling of equality is crucial here */
            /* for sentinels & efficiency with duplicates */
            for (;;++i) {
                if (!NK_TT__COMPARE(&p[i], &p[0])) break;
            }
            for (;;--j) {
                if (!NK_TT__COMPARE(&p[0], &p[j])) break;
            }

            /* make sure we haven't crossed */
             if (i >= j) break;
             t = p[i];
             p[i] = p[j];
             p[j] = t;

            ++i;
            --j;

        }

        /* recurse on smaller side, iterate on larger */
        if (j < (n-i)) {
            nk_tt__sort_edges_quicksort(p,j);
            p = p+i;
            n = n-i;
        } else {
            nk_tt__sort_edges_quicksort(p+i, n-i);
            n = j;
        }
    }
}

static void
nk_tt__sort_edges(struct nk_tt__edge *p, int n)
{
   nk_tt__sort_edges_quicksort(p, n);
   nk_tt__sort_edges_ins_sort(p, n);
}

static void
nk_tt__rasterize(struct nk_tt__bitmap *result, struct nk_tt__point *pts,
    int *wcount, int windings, float scale_x, float scale_y,
    float shift_x, float shift_y, int off_x, int off_y, int invert,
    struct nk_allocator *alloc)
{
    float y_scale_inv = invert ? -scale_y : scale_y;
    struct nk_tt__edge *e;
    int n,i,j,k,m;
    int vsubsample = 1;
    /* vsubsample should divide 255 evenly; otherwise we won't reach full opacity */

    /* now we have to blow out the windings into explicit edge lists */
    n = 0;
    for (i=0; i < windings; ++i)
        n += wcount[i];

    e = (struct nk_tt__edge*)
       alloc->alloc(alloc->userdata, 0,(sizeof(*e) * (size_t)(n+1)));
    if (e == 0) return;
    n = 0;

    m=0;
    for (i=0; i < windings; ++i)
    {
        struct nk_tt__point *p = pts + m;
        m += wcount[i];
        j = wcount[i]-1;
        for (k=0; k < wcount[i]; j=k++) {
            int a=k,b=j;
            /* skip the edge if horizontal */
            if (p[j].y == p[k].y)
                continue;

            /* add edge from j to k to the list */
            e[n].invert = 0;
            if (invert ? p[j].y > p[k].y : p[j].y < p[k].y) {
                e[n].invert = 1;
                a=j,b=k;
            }
            e[n].x0 = p[a].x * scale_x + shift_x;
            e[n].y0 = (p[a].y * y_scale_inv + shift_y) * (float)vsubsample;
            e[n].x1 = p[b].x * scale_x + shift_x;
            e[n].y1 = (p[b].y * y_scale_inv + shift_y) * (float)vsubsample;
            ++n;
        }
    }

    /* now sort the edges by their highest point (should snap to integer, and then by x) */
    /*STBTT_sort(e, n, sizeof(e[0]), nk_tt__edge_compare); */
    nk_tt__sort_edges(e, n);
    /* now, traverse the scanlines and find the intersections on each scanline, use xor winding rule */
    nk_tt__rasterize_sorted_edges(result, e, n, vsubsample, off_x, off_y, alloc);
    alloc->free(alloc->userdata, e);
}

static void
nk_tt__add_point(struct nk_tt__point *points, int n, float x, float y)
{
    if (!points) return; /* during first pass, it's unallocated */
    points[n].x = x;
    points[n].y = y;
}

static int
nk_tt__tesselate_curve(struct nk_tt__point *points, int *num_points,
    float x0, float y0, float x1, float y1, float x2, float y2,
    float objspace_flatness_squared, int n)
{
    /* tesselate until threshold p is happy...
     * @TODO warped to compensate for non-linear stretching */
    /* midpoint */
    float mx = (x0 + 2*x1 + x2)/4;
    float my = (y0 + 2*y1 + y2)/4;
    /* versus directly drawn line */
    float dx = (x0+x2)/2 - mx;
    float dy = (y0+y2)/2 - my;
    if (n > 16) /* 65536 segments on one curve better be enough! */
        return 1;

    /* half-pixel error allowed... need to be smaller if AA */
    if (dx*dx+dy*dy > objspace_flatness_squared) {
        nk_tt__tesselate_curve(points, num_points, x0,y0,
            (x0+x1)/2.0f,(y0+y1)/2.0f, mx,my, objspace_flatness_squared,n+1);
        nk_tt__tesselate_curve(points, num_points, mx,my,
            (x1+x2)/2.0f,(y1+y2)/2.0f, x2,y2, objspace_flatness_squared,n+1);
    } else {
        nk_tt__add_point(points, *num_points,x2,y2);
        *num_points = *num_points+1;
    }
    return 1;
}

/* returns number of contours */
static struct nk_tt__point*
nk_tt_FlattenCurves(struct nk_tt_vertex *vertices, int num_verts,
    float objspace_flatness, int **contour_lengths, int *num_contours,
    struct nk_allocator *alloc)
{
    struct nk_tt__point *points=0;
    int num_points=0;
    float objspace_flatness_squared = objspace_flatness * objspace_flatness;
    int i;
    int n=0;
    int start=0;
    int pass;

    /* count how many "moves" there are to get the contour count */
    for (i=0; i < num_verts; ++i)
        if (vertices[i].type == NK_TT_vmove) ++n;

    *num_contours = n;
    if (n == 0) return 0;

    *contour_lengths = (int *)
        alloc->alloc(alloc->userdata,0, (sizeof(**contour_lengths) * (size_t)n));
    if (*contour_lengths == 0) {
        *num_contours = 0;
        return 0;
    }

    /* make two passes through the points so we don't need to realloc */
    for (pass=0; pass < 2; ++pass)
    {
        float x=0,y=0;
        if (pass == 1) {
            points = (struct nk_tt__point *)
                alloc->alloc(alloc->userdata,0, (size_t)num_points * sizeof(points[0]));
            if (points == 0) goto error;
        }
        num_points = 0;
        n= -1;

        for (i=0; i < num_verts; ++i)
        {
            switch (vertices[i].type) {
            case NK_TT_vmove:
                /* start the next contour */
                if (n >= 0)
                (*contour_lengths)[n] = num_points - start;
                ++n;
                start = num_points;

                x = vertices[i].x, y = vertices[i].y;
                nk_tt__add_point(points, num_points++, x,y);
                break;
            case NK_TT_vline:
               x = vertices[i].x, y = vertices[i].y;
               nk_tt__add_point(points, num_points++, x, y);
               break;
            case NK_TT_vcurve:
               nk_tt__tesselate_curve(points, &num_points, x,y,
                                        vertices[i].cx, vertices[i].cy,
                                        vertices[i].x,  vertices[i].y,
                                        objspace_flatness_squared, 0);
               x = vertices[i].x, y = vertices[i].y;
               break;
            default: break;
         }
      }
      (*contour_lengths)[n] = num_points - start;
   }
   return points;

error:
   alloc->free(alloc->userdata, points);
   alloc->free(alloc->userdata, *contour_lengths);
   *contour_lengths = 0;
   *num_contours = 0;
   return 0;
}

static void
nk_tt_Rasterize(struct nk_tt__bitmap *result, float flatness_in_pixels,
    struct nk_tt_vertex *vertices, int num_verts,
    float scale_x, float scale_y, float shift_x, float shift_y,
    int x_off, int y_off, int invert, struct nk_allocator *alloc)
{
    float scale = scale_x > scale_y ? scale_y : scale_x;
    int winding_count, *winding_lengths;
    struct nk_tt__point *windings = nk_tt_FlattenCurves(vertices, num_verts,
        flatness_in_pixels / scale, &winding_lengths, &winding_count, alloc);

    NK_ASSERT(alloc);
    if (windings) {
        nk_tt__rasterize(result, windings, winding_lengths, winding_count,
            scale_x, scale_y, shift_x, shift_y, x_off, y_off, invert, alloc);
        alloc->free(alloc->userdata, winding_lengths);
        alloc->free(alloc->userdata, windings);
    }
}

static void
nk_tt_MakeGlyphBitmapSubpixel(const struct nk_tt_fontinfo *info, unsigned char *output,
    int out_w, int out_h, int out_stride, float scale_x, float scale_y,
    float shift_x, float shift_y, int glyph, struct nk_allocator *alloc)
{
    int ix0,iy0;
    struct nk_tt_vertex *vertices;
    int num_verts = nk_tt_GetGlyphShape(info, alloc, glyph, &vertices);
    struct nk_tt__bitmap gbm;

    nk_tt_GetGlyphBitmapBoxSubpixel(info, glyph, scale_x, scale_y, shift_x,
        shift_y, &ix0,&iy0,0,0);
    gbm.pixels = output;
    gbm.w = out_w;
    gbm.h = out_h;
    gbm.stride = out_stride;

    if (gbm.w && gbm.h)
        nk_tt_Rasterize(&gbm, 0.35f, vertices, num_verts, scale_x, scale_y,
            shift_x, shift_y, ix0,iy0, 1, alloc);
    alloc->free(alloc->userdata, vertices);
}

/*-------------------------------------------------------------
 *                          Bitmap baking
 * --------------------------------------------------------------*/
static int
nk_tt_PackBegin(struct nk_tt_pack_context *spc, unsigned char *pixels,
    int pw, int ph, int stride_in_bytes, int padding, struct nk_allocator *alloc)
{
    int num_nodes = pw - padding;
    struct nk_rp_context *context = (struct nk_rp_context *)
        alloc->alloc(alloc->userdata,0, sizeof(*context));
    struct nk_rp_node *nodes = (struct nk_rp_node*)
        alloc->alloc(alloc->userdata,0, (sizeof(*nodes  ) * (size_t)num_nodes));

    if (context == 0 || nodes == 0) {
        if (context != 0) alloc->free(alloc->userdata, context);
        if (nodes   != 0) alloc->free(alloc->userdata, nodes);
        return 0;
    }

    spc->width = pw;
    spc->height = ph;
    spc->pixels = pixels;
    spc->pack_info = context;
    spc->nodes = nodes;
    spc->padding = padding;
    spc->stride_in_bytes = (stride_in_bytes != 0) ? stride_in_bytes : pw;
    spc->h_oversample = 1;
    spc->v_oversample = 1;

    nk_rp_init_target(context, pw-padding, ph-padding, nodes, num_nodes);
    if (pixels)
        memset(pixels, 0, (size_t)(pw*ph)); /* background of 0 around pixels */
    return 1;
}

static void
nk_tt_PackEnd(struct nk_tt_pack_context *spc, struct nk_allocator *alloc)
{
    alloc->free(alloc->userdata, spc->nodes);
    alloc->free(alloc->userdata, spc->pack_info);
}

static void
nk_tt_PackSetOversampling(struct nk_tt_pack_context *spc,
    unsigned int h_oversample, unsigned int v_oversample)
{
   NK_ASSERT(h_oversample <= NK_TT_MAX_OVERSAMPLE);
   NK_ASSERT(v_oversample <= NK_TT_MAX_OVERSAMPLE);
   if (h_oversample <= NK_TT_MAX_OVERSAMPLE)
      spc->h_oversample = h_oversample;
   if (v_oversample <= NK_TT_MAX_OVERSAMPLE)
      spc->v_oversample = v_oversample;
}

static void
nk_tt__h_prefilter(unsigned char *pixels, int w, int h, int stride_in_bytes,
    int kernel_width)
{
    unsigned char buffer[NK_TT_MAX_OVERSAMPLE];
    int safe_w = w - kernel_width;
    int j;

    for (j=0; j < h; ++j)
    {
        int i;
        unsigned int total;
        memset(buffer, 0, (size_t)kernel_width);

        total = 0;

        /* make kernel_width a constant in common cases so compiler can optimize out the divide */
        switch (kernel_width) {
        case 2:
            for (i=0; i <= safe_w; ++i) {
                total += (unsigned int)(pixels[i] - buffer[i & NK_TT__OVER_MASK]);
                buffer[(i+kernel_width) & NK_TT__OVER_MASK] = pixels[i];
                pixels[i] = (unsigned char) (total / 2);
            }
            break;
        case 3:
            for (i=0; i <= safe_w; ++i) {
                total += (unsigned int)(pixels[i] - buffer[i & NK_TT__OVER_MASK]);
                buffer[(i+kernel_width) & NK_TT__OVER_MASK] = pixels[i];
                pixels[i] = (unsigned char) (total / 3);
            }
            break;
        case 4:
            for (i=0; i <= safe_w; ++i) {
                total += (unsigned int)pixels[i] - buffer[i & NK_TT__OVER_MASK];
                buffer[(i+kernel_width) & NK_TT__OVER_MASK] = pixels[i];
                pixels[i] = (unsigned char) (total / 4);
            }
            break;
        case 5:
            for (i=0; i <= safe_w; ++i) {
                total += (unsigned int)(pixels[i] - buffer[i & NK_TT__OVER_MASK]);
                buffer[(i+kernel_width) & NK_TT__OVER_MASK] = pixels[i];
                pixels[i] = (unsigned char) (total / 5);
            }
            break;
        default:
            for (i=0; i <= safe_w; ++i) {
                total += (unsigned int)(pixels[i] - buffer[i & NK_TT__OVER_MASK]);
                buffer[(i+kernel_width) & NK_TT__OVER_MASK] = pixels[i];
                pixels[i] = (unsigned char) (total / (unsigned int)kernel_width);
            }
            break;
        }

        for (; i < w; ++i) {
            NK_ASSERT(pixels[i] == 0);
            total -= (unsigned int)(buffer[i & NK_TT__OVER_MASK]);
            pixels[i] = (unsigned char) (total / (unsigned int)kernel_width);
        }
        pixels += stride_in_bytes;
    }
}

static void
nk_tt__v_prefilter(unsigned char *pixels, int w, int h, int stride_in_bytes,
    int kernel_width)
{
    unsigned char buffer[NK_TT_MAX_OVERSAMPLE];
    int safe_h = h - kernel_width;
    int j;

    for (j=0; j < w; ++j)
    {
        int i;
        unsigned int total;
        memset(buffer, 0, (size_t)kernel_width);

        total = 0;

        /* make kernel_width a constant in common cases so compiler can optimize out the divide */
        switch (kernel_width) {
        case 2:
            for (i=0; i <= safe_h; ++i) {
                total += (unsigned int)(pixels[i*stride_in_bytes] - buffer[i & NK_TT__OVER_MASK]);
                buffer[(i+kernel_width) & NK_TT__OVER_MASK] = pixels[i*stride_in_bytes];
                pixels[i*stride_in_bytes] = (unsigned char) (total / 2);
            }
            break;
         case 3:
            for (i=0; i <= safe_h; ++i) {
                total += (unsigned int)(pixels[i*stride_in_bytes] - buffer[i & NK_TT__OVER_MASK]);
                buffer[(i+kernel_width) & NK_TT__OVER_MASK] = pixels[i*stride_in_bytes];
                pixels[i*stride_in_bytes] = (unsigned char) (total / 3);
            }
            break;
         case 4:
            for (i=0; i <= safe_h; ++i) {
                total += (unsigned int)(pixels[i*stride_in_bytes] - buffer[i & NK_TT__OVER_MASK]);
                buffer[(i+kernel_width) & NK_TT__OVER_MASK] = pixels[i*stride_in_bytes];
                pixels[i*stride_in_bytes] = (unsigned char) (total / 4);
            }
            break;
         case 5:
            for (i=0; i <= safe_h; ++i) {
                total += (unsigned int)(pixels[i*stride_in_bytes] - buffer[i & NK_TT__OVER_MASK]);
                buffer[(i+kernel_width) & NK_TT__OVER_MASK] = pixels[i*stride_in_bytes];
                pixels[i*stride_in_bytes] = (unsigned char) (total / 5);
            }
            break;
         default:
            for (i=0; i <= safe_h; ++i) {
                total += (unsigned int)(pixels[i*stride_in_bytes] - buffer[i & NK_TT__OVER_MASK]);
                buffer[(i+kernel_width) & NK_TT__OVER_MASK] = pixels[i*stride_in_bytes];
                pixels[i*stride_in_bytes] = (unsigned char) (total / (unsigned int)kernel_width);
            }
            break;
        }

        for (; i < h; ++i) {
            NK_ASSERT(pixels[i*stride_in_bytes] == 0);
            total -= (unsigned int)(buffer[i & NK_TT__OVER_MASK]);
            pixels[i*stride_in_bytes] = (unsigned char) (total / (unsigned int)kernel_width);
        }
        pixels += 1;
    }
}

static float
nk_tt__oversample_shift(int oversample)
{
    if (!oversample)
        return 0.0f;

    /* The prefilter is a box filter of width "oversample", */
    /* which shifts phase by (oversample - 1)/2 pixels in */
    /* oversampled space. We want to shift in the opposite */
    /* direction to counter this. */
    return (float)-(oversample - 1) / (2.0f * (float)oversample);
}

/* rects array must be big enough to accommodate all characters in the given ranges */
static int
nk_tt_PackFontRangesGatherRects(struct nk_tt_pack_context *spc,
    struct nk_tt_fontinfo *info, struct nk_tt_pack_range *ranges,
    int num_ranges, struct nk_rp_rect *rects)
{
    int i,j,k;
    k = 0;

    for (i=0; i < num_ranges; ++i) {
        float fh = ranges[i].font_size;
        float scale = (fh > 0) ? nk_tt_ScaleForPixelHeight(info, fh):
            nk_tt_ScaleForMappingEmToPixels(info, -fh);
        ranges[i].h_oversample = (unsigned char) spc->h_oversample;
        ranges[i].v_oversample = (unsigned char) spc->v_oversample;
        for (j=0; j < ranges[i].num_chars; ++j) {
            int x0,y0,x1,y1;
            int codepoint = ranges[i].first_unicode_codepoint_in_range ?
                ranges[i].first_unicode_codepoint_in_range + j :
                ranges[i].array_of_unicode_codepoints[j];

            int glyph = nk_tt_FindGlyphIndex(info, codepoint);
            nk_tt_GetGlyphBitmapBoxSubpixel(info,glyph, scale * (float)spc->h_oversample,
                scale * (float)spc->v_oversample, 0,0, &x0,&y0,&x1,&y1);
            rects[k].w = (nk_rp_coord) (x1-x0 + spc->padding + (int)spc->h_oversample-1);
            rects[k].h = (nk_rp_coord) (y1-y0 + spc->padding + (int)spc->v_oversample-1);
            ++k;
        }
    }
    return k;
}

static int
nk_tt_PackFontRangesRenderIntoRects(struct nk_tt_pack_context *spc,
    struct nk_tt_fontinfo *info, struct nk_tt_pack_range *ranges,
    int num_ranges, struct nk_rp_rect *rects, struct nk_allocator *alloc)
{
    int i,j,k, return_value = 1;
    /* save current values */
    int old_h_over = (int)spc->h_oversample;
    int old_v_over = (int)spc->v_oversample;
    /* rects array must be big enough to accommodate all characters in the given ranges */

    k = 0;
    for (i=0; i < num_ranges; ++i)
    {
        float fh = ranges[i].font_size;
        float recip_h,recip_v,sub_x,sub_y;
        float scale = fh > 0 ? nk_tt_ScaleForPixelHeight(info, fh):
            nk_tt_ScaleForMappingEmToPixels(info, -fh);

        spc->h_oversample = ranges[i].h_oversample;
        spc->v_oversample = ranges[i].v_oversample;

        recip_h = 1.0f / (float)spc->h_oversample;
        recip_v = 1.0f / (float)spc->v_oversample;

        sub_x = nk_tt__oversample_shift((int)spc->h_oversample);
        sub_y = nk_tt__oversample_shift((int)spc->v_oversample);

        for (j=0; j < ranges[i].num_chars; ++j)
        {
            struct nk_rp_rect *r = &rects[k];
            if (r->was_packed)
            {
                struct nk_tt_packedchar *bc = &ranges[i].chardata_for_range[j];
                int advance, lsb, x0,y0,x1,y1;
                int codepoint = ranges[i].first_unicode_codepoint_in_range ?
                    ranges[i].first_unicode_codepoint_in_range + j :
                    ranges[i].array_of_unicode_codepoints[j];
                int glyph = nk_tt_FindGlyphIndex(info, codepoint);
                nk_rp_coord pad = (nk_rp_coord) spc->padding;

                /* pad on left and top */
                r->x = (nk_rp_coord)((int)r->x + (int)pad);
                r->y = (nk_rp_coord)((int)r->y + (int)pad);
                r->w = (nk_rp_coord)((int)r->w - (int)pad);
                r->h = (nk_rp_coord)((int)r->h - (int)pad);

                nk_tt_GetGlyphHMetrics(info, glyph, &advance, &lsb);
                nk_tt_GetGlyphBitmapBox(info, glyph, scale * (float)spc->h_oversample,
                        (scale * (float)spc->v_oversample), &x0,&y0,&x1,&y1);
                nk_tt_MakeGlyphBitmapSubpixel(info, spc->pixels + r->x + r->y*spc->stride_in_bytes,
                    (int)(r->w - spc->h_oversample+1), (int)(r->h - spc->v_oversample+1),
                    spc->stride_in_bytes, scale * (float)spc->h_oversample,
                    scale * (float)spc->v_oversample, 0,0, glyph, alloc);

                if (spc->h_oversample > 1)
                   nk_tt__h_prefilter(spc->pixels + r->x + r->y*spc->stride_in_bytes,
                        r->w, r->h, spc->stride_in_bytes, (int)spc->h_oversample);

                if (spc->v_oversample > 1)
                   nk_tt__v_prefilter(spc->pixels + r->x + r->y*spc->stride_in_bytes,
                        r->w, r->h, spc->stride_in_bytes, (int)spc->v_oversample);

                bc->x0       = (uint16_t)  r->x;
                bc->y0       = (uint16_t)  r->y;
                bc->x1       = (uint16_t) (r->x + r->w);
                bc->y1       = (uint16_t) (r->y + r->h);
                bc->xadvance = scale * (float)advance;
                bc->xoff     = (float)  x0 * recip_h + sub_x;
                bc->yoff     = (float)  y0 * recip_v + sub_y;
                bc->xoff2    = ((float)x0 + r->w) * recip_h + sub_x;
                bc->yoff2    = ((float)y0 + r->h) * recip_v + sub_y;
            } else {
                return_value = 0; /* if any fail, report failure */
            }
            ++k;
        }
    }
    /* restore original values */
    spc->h_oversample = (unsigned int)old_h_over;
    spc->v_oversample = (unsigned int)old_v_over;
    return return_value;
}

static void
nk_tt_GetPackedQuad(struct nk_tt_packedchar *chardata, int pw, int ph,
    int char_index, float *xpos, float *ypos, struct nk_tt_aligned_quad *q,
    int align_to_integer)
{
    float ipw = 1.0f / (float)pw, iph = 1.0f / (float)ph;
    struct nk_tt_packedchar *b = (struct nk_tt_packedchar*)(chardata + char_index);
    if (align_to_integer) {
        int tx = nk_ifloorf((*xpos + b->xoff) + 0.5f);
        int ty = nk_ifloorf((*ypos + b->yoff) + 0.5f);

        float x = (float)tx;
        float y = (float)ty;

        q->x0 = x;
        q->y0 = y;
        q->x1 = x + b->xoff2 - b->xoff;
        q->y1 = y + b->yoff2 - b->yoff;
    } else {
        q->x0 = *xpos + b->xoff;
        q->y0 = *ypos + b->yoff;
        q->x1 = *xpos + b->xoff2;
        q->y1 = *ypos + b->yoff2;
    }
    q->s0 = b->x0 * ipw;
    q->t0 = b->y0 * iph;
    q->s1 = b->x1 * ipw;
    q->t1 = b->y1 * iph;
    *xpos += b->xadvance;
}
