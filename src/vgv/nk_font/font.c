#include "font.h"
#include "truetype.h"
#include <stdio.h>

// UTF api
#define NK_UTF_INVALID 0xFFFD
static const uint8_t nk_utfbyte[4+1] = {0x80, 0, 0xC0, 0xE0, 0xF0};
static const uint8_t nk_utfmask[4+1] = {0xC0, 0x80, 0xE0, 0xF0, 0xF8};
static const uint32_t nk_utfmin[4+1] = {0, 0, 0x80, 0x800, 0x10000};
static const uint32_t nk_utfmax[4+1] = {0x10FFFF, 0x7F, 0x7FF, 0xFFFF, 0x10FFFF};

static int
nk_utf_validate(uint32_t *u, int i)
{
    NK_ASSERT(u);
    if (!u) return 0;
    if (!(nk_utfmin[i] <= *u && *u < nk_utfmax[i]) ||
         (0xD800 <= *u && *u < 0xDFFF))
            *u = NK_UTF_INVALID;
    for (i = 1; *u > nk_utfmax[i]; ++i);
    return i;
}
uint32_t nk_utf_decode_byte(char c, int *i)
{
    NK_ASSERT(i);
    if (!i) return 0;
    for(*i = 0; *i < 5; ++(*i)) {
        if (((uint8_t)c & nk_utfmask[*i]) == nk_utfbyte[*i])
            return (uint8_t)(c & ~nk_utfmask[*i]);
    }
    return 0;
}

int nk_utf_decode(const char *c, uint32_t *u, int clen)
{
    int i, j, len, type=0;
    uint32_t udecoded;

    NK_ASSERT(c);
    NK_ASSERT(u);

    if (!c || !u) return 0;
    if (!clen) return 0;
    *u = NK_UTF_INVALID;

    udecoded = nk_utf_decode_byte(c[0], &len);
    if (!(1 <= len && len < 4))
        return 1;

    for (i = 1, j = 1; i < clen && j < len; ++i, ++j) {
        udecoded = (udecoded << 6) | nk_utf_decode_byte(c[i], &type);
        if (type != 0)
            return j;
    }
    if (j < len)
        return 0;
    *u = udecoded;
    nk_utf_validate(u, len);
    return len;
}

// util
static uint32_t
nk_round_up_pow2(uint32_t v)
{
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return v;
}

static void* nk_malloc(nk_handle unused, void *old, size_t size)
{
	((void)(unused)); 
	((void)(old)); 
	return malloc(size);
}

static void nk_mfree(nk_handle unused, void *ptr)
{
	((void)(unused));
	free(ptr);
}

static char*
nk_file_load(const char* path, size_t* siz, struct nk_allocator *alloc)
{
    char *buf;
    FILE *fd;
    long ret;

    NK_ASSERT(path);
    NK_ASSERT(siz);
    NK_ASSERT(alloc);
    if (!path || !siz || !alloc)
        return 0;

    fd = fopen(path, "rb");
    if (!fd) return 0;
    fseek(fd, 0, SEEK_END);
    ret = ftell(fd);
    if (ret < 0) {
        fclose(fd);
        return 0;
    }
    *siz = (size_t)ret;
    fseek(fd, 0, SEEK_SET);
    buf = (char*)alloc->alloc(alloc->userdata,0, *siz);
    NK_ASSERT(buf);
    if (!buf) {
        fclose(fd);
        return 0;
    }
    *siz = (size_t)fread(buf, 1,*siz, fd);
    fclose(fd);
    return buf;
}

// font
struct nk_font_bake_data {
    struct nk_tt_fontinfo info;
    struct nk_rp_rect *rects;
    struct nk_tt_pack_range *ranges;
    uint32_t range_count;
};

struct nk_font_baker {
    struct nk_allocator alloc;
    struct nk_tt_pack_context spc;
    struct nk_font_bake_data *build;
    struct nk_tt_packedchar *packed_chars;
    struct nk_rp_rect *rects;
    struct nk_tt_pack_range *ranges;
};

static const size_t nk_rect_align = NK_ALIGNOF(struct nk_rp_rect);
static const size_t nk_range_align = NK_ALIGNOF(struct nk_tt_pack_range);
static const size_t nk_char_align = NK_ALIGNOF(struct nk_tt_packedchar);
static const size_t nk_build_align = NK_ALIGNOF(struct nk_font_bake_data);
static const size_t nk_baker_align = NK_ALIGNOF(struct nk_font_baker);

static int
nk_range_count(const uint32_t *range)
{
    const uint32_t *iter = range;
    NK_ASSERT(range);
    if (!range) return 0;
    while (*(iter++) != 0);
    return (iter == range) ? 0 : (int)((iter - range)/2);
}

static int
nk_range_glyph_count(const uint32_t *range, int count)
{
    int i = 0;
    int total_glyphs = 0;
    for (i = 0; i < count; ++i) {
        int diff;
        uint32_t f = range[(i*2)+0];
        uint32_t t = range[(i*2)+1];
        NK_ASSERT(t >= f);
        diff = (int)((t - f) + 1);
        total_glyphs += diff;
    }
    return total_glyphs;
}

const uint32_t*
nk_font_default_glyph_ranges(void)
{
    static const uint32_t ranges[] = {0x0020, 0x00FF, 0};
    return ranges;
}

const uint32_t*
nk_font_chinese_glyph_ranges(void)
{
    static const uint32_t ranges[] = {
        0x0020, 0x00FF,
        0x3000, 0x30FF,
        0x31F0, 0x31FF,
        0xFF00, 0xFFEF,
        0x4e00, 0x9FAF,
        0
    };
    return ranges;
}

const uint32_t*
nk_font_cyrillic_glyph_ranges(void)
{
    static const uint32_t ranges[] = {
        0x0020, 0x00FF,
        0x0400, 0x052F,
        0x2DE0, 0x2DFF,
        0xA640, 0xA69F,
        0
    };
    return ranges;
}

const uint32_t*
nk_font_korean_glyph_ranges(void)
{
    static const uint32_t ranges[] = {
        0x0020, 0x00FF,
        0x3131, 0x3163,
        0xAC00, 0xD79D,
        0
    };
    return ranges;
}

static void
nk_font_baker_memory(size_t *temp, int *glyph_count,
    struct nk_font_config *config_list, int count)
{
    int range_count = 0;
    int total_range_count = 0;
    struct nk_font_config *iter, *i;

    NK_ASSERT(config_list);
    NK_ASSERT(glyph_count);
    if (!config_list) {
        *temp = 0;
        *glyph_count = 0;
        return;
    }
    *glyph_count = 0;
    for (iter = config_list; iter; iter = iter->next) {
        i = iter;
        do {if (!i->range) iter->range = nk_font_default_glyph_ranges();
            range_count = nk_range_count(i->range);
            total_range_count += range_count;
            *glyph_count += nk_range_glyph_count(i->range, range_count);
        } while ((i = i->n) != iter);
    }
    *temp = (size_t)*glyph_count * sizeof(struct nk_rp_rect);
    *temp += (size_t)total_range_count * sizeof(struct nk_tt_pack_range);
    *temp += (size_t)*glyph_count * sizeof(struct nk_tt_packedchar);
    *temp += (size_t)count * sizeof(struct nk_font_bake_data);
    *temp += sizeof(struct nk_font_baker);
    *temp += nk_rect_align + nk_range_align + nk_char_align;
    *temp += nk_build_align + nk_baker_align;
}

static struct nk_font_baker*
nk_font_baker(void *memory, int glyph_count, int count, struct nk_allocator *alloc)
{
    struct nk_font_baker *baker;
    if (!memory) return 0;
    /* setup baker inside a memory block  */
    baker = (struct nk_font_baker*)NK_ALIGN_PTR(memory, nk_baker_align);
    baker->build = (struct nk_font_bake_data*)NK_ALIGN_PTR((baker + 1), nk_build_align);
    baker->packed_chars = (struct nk_tt_packedchar*)NK_ALIGN_PTR((baker->build + count), nk_char_align);
    baker->rects = (struct nk_rp_rect*)NK_ALIGN_PTR((baker->packed_chars + glyph_count), nk_rect_align);
    baker->ranges = (struct nk_tt_pack_range*)NK_ALIGN_PTR((baker->rects + glyph_count), nk_range_align);
    baker->alloc = *alloc;
    return baker;
}

static int
nk_font_bake_pack(struct nk_font_baker *baker,
    size_t *image_memory, int *width, int *height, struct nk_recti *custom,
    const struct nk_font_config *config_list, int count,
    struct nk_allocator *alloc)
{
    static const size_t max_height = 1024 * 32;
    const struct nk_font_config *config_iter, *it;
    int total_glyph_count = 0;
    int total_range_count = 0;
    int range_count = 0;
    int i = 0;

    NK_ASSERT(image_memory);
    NK_ASSERT(width);
    NK_ASSERT(height);
    NK_ASSERT(config_list);
    NK_ASSERT(count);
    NK_ASSERT(alloc);

    if (!image_memory || !width || !height || !config_list || !count) return 0;
    for (config_iter = config_list; config_iter; config_iter = config_iter->next) {
        it = config_iter;
        do {range_count = nk_range_count(it->range);
            total_range_count += range_count;
            total_glyph_count += nk_range_glyph_count(it->range, range_count);
        } while ((it = it->n) != config_iter);
    }
    /* setup font baker from temporary memory */
    for (config_iter = config_list; config_iter; config_iter = config_iter->next) {
        it = config_iter;
        do {if (!nk_tt_InitFont(&baker->build[i++].info, (const unsigned char*)it->ttf_blob, 0))
            return 1;
        } while ((it = it->n) != config_iter);
    }
    *height = 0;
    *width = (total_glyph_count > 1000) ? 1024 : 512;
    nk_tt_PackBegin(&baker->spc, 0, (int)*width, (int)max_height, 0, 1, alloc);
    {
        int input_i = 0;
        int range_n = 0;
        int rect_n = 0;
        int char_n = 0;

        if (custom) {
            /* pack custom user data first so it will be in the upper left corner*/
            struct nk_rp_rect custom_space;
            memset(&custom_space, 0, sizeof(custom_space));
            custom_space.w = (nk_rp_coord)(custom->w);
            custom_space.h = (nk_rp_coord)(custom->h);

            nk_tt_PackSetOversampling(&baker->spc, 1, 1);
            nk_rp_pack_rects((struct nk_rp_context*)baker->spc.pack_info, &custom_space, 1);
            *height = NK_MAX(*height, (int)(custom_space.y + custom_space.h));

            custom->x = (short)custom_space.x;
            custom->y = (short)custom_space.y;
            custom->w = (short)custom_space.w;
            custom->h = (short)custom_space.h;
        }

        /* first font pass: pack all glyphs */
        for (input_i = 0, config_iter = config_list; input_i < count && config_iter;
            config_iter = config_iter->next) {
            it = config_iter;
            do {int n = 0;
                int glyph_count;
                const uint32_t *in_range;
                const struct nk_font_config *cfg = it;
                struct nk_font_bake_data *tmp = &baker->build[input_i++];

                /* count glyphs + ranges in current font */
                glyph_count = 0; range_count = 0;
                for (in_range = cfg->range; in_range[0] && in_range[1]; in_range += 2) {
                    glyph_count += (int)(in_range[1] - in_range[0]) + 1;
                    range_count++;
                }

                /* setup ranges  */
                tmp->ranges = baker->ranges + range_n;
                tmp->range_count = (uint32_t)range_count;
                range_n += range_count;
                for (i = 0; i < range_count; ++i) {
                    in_range = &cfg->range[i * 2];
                    tmp->ranges[i].font_size = cfg->size;
                    tmp->ranges[i].first_unicode_codepoint_in_range = (int)in_range[0];
                    tmp->ranges[i].num_chars = (int)(in_range[1]- in_range[0]) + 1;
                    tmp->ranges[i].chardata_for_range = baker->packed_chars + char_n;
                    char_n += tmp->ranges[i].num_chars;
                }

                /* pack */
                tmp->rects = baker->rects + rect_n;
                rect_n += glyph_count;
                nk_tt_PackSetOversampling(&baker->spc, cfg->oversample_h, cfg->oversample_v);
                n = nk_tt_PackFontRangesGatherRects(&baker->spc, &tmp->info,
                    tmp->ranges, (int)tmp->range_count, tmp->rects);
                nk_rp_pack_rects((struct nk_rp_context*)baker->spc.pack_info, tmp->rects, (int)n);

                /* texture height */
                for (i = 0; i < n; ++i) {
                    if (tmp->rects[i].was_packed)
                        *height = NK_MAX(*height, tmp->rects[i].y + tmp->rects[i].h);
                }
            } while ((it = it->n) != config_iter);
        }
        NK_ASSERT(rect_n == total_glyph_count);
        NK_ASSERT(char_n == total_glyph_count);
        NK_ASSERT(range_n == total_range_count);
    }
    *height = (int)nk_round_up_pow2((uint32_t)*height);
    *image_memory = (size_t)(*width) * (size_t)(*height);
    return 1;
}

static void
nk_font_bake(struct nk_font_baker *baker, void *image_memory, int width, int height,
    struct nk_font_glyph *glyphs, int glyphs_count,
    const struct nk_font_config *config_list, int font_count)
{
    int input_i = 0;
    uint32_t glyph_n = 0;
    const struct nk_font_config *config_iter;
    const struct nk_font_config *it;

    NK_ASSERT(image_memory);
    NK_ASSERT(width);
    NK_ASSERT(height);
    NK_ASSERT(config_list);
    NK_ASSERT(baker);
    NK_ASSERT(font_count);
    NK_ASSERT(glyphs_count);
    if (!image_memory || !width || !height || !config_list ||
        !font_count || !glyphs || !glyphs_count)
        return;

    /* second font pass: render glyphs */
    memset(image_memory, 0, (size_t)((size_t)width * (size_t)height));
    baker->spc.pixels = (unsigned char*)image_memory;
    baker->spc.height = (int)height;
    for (input_i = 0, config_iter = config_list; input_i < font_count && config_iter;
        config_iter = config_iter->next) {
        it = config_iter;
        do {const struct nk_font_config *cfg = it;
            struct nk_font_bake_data *tmp = &baker->build[input_i++];
            nk_tt_PackSetOversampling(&baker->spc, cfg->oversample_h, cfg->oversample_v);
            nk_tt_PackFontRangesRenderIntoRects(&baker->spc, &tmp->info, tmp->ranges,
                (int)tmp->range_count, tmp->rects, &baker->alloc);
        } while ((it = it->n) != config_iter);
    } nk_tt_PackEnd(&baker->spc, &baker->alloc);

    /* third pass: setup font and glyphs */
    for (input_i = 0, config_iter = config_list; input_i < font_count && config_iter;
        config_iter = config_iter->next) {
        it = config_iter;
        do {size_t i = 0;
            int char_idx = 0;
            uint32_t glyph_count = 0;
            const struct nk_font_config *cfg = it;
            struct nk_font_bake_data *tmp = &baker->build[input_i++];
            struct nk_baked_font *dst_font = cfg->font;

            float font_scale = nk_tt_ScaleForPixelHeight(&tmp->info, cfg->size);
            int unscaled_ascent, unscaled_descent, unscaled_line_gap;
            nk_tt_GetFontVMetrics(&tmp->info, &unscaled_ascent, &unscaled_descent,
                                    &unscaled_line_gap);

            /* fill baked font */
            if (!cfg->merge_mode) {
                dst_font->ranges = cfg->range;
                dst_font->height = cfg->size;
                dst_font->ascent = ((float)unscaled_ascent * font_scale);
                dst_font->descent = ((float)unscaled_descent * font_scale);
                dst_font->glyph_offset = glyph_n;
            }

            /* fill own baked font glyph array */
            for (i = 0; i < tmp->range_count; ++i) {
                struct nk_tt_pack_range *range = &tmp->ranges[i];
                for (char_idx = 0; char_idx < range->num_chars; char_idx++)
                {
                    uint32_t codepoint = 0;
                    float dummy_x = 0, dummy_y = 0;
                    struct nk_tt_aligned_quad q;
                    struct nk_font_glyph *glyph;

                    /* query glyph bounds from stb_truetype */
                    const struct nk_tt_packedchar *pc = &range->chardata_for_range[char_idx];
                    if (!pc->x0 && !pc->x1 && !pc->y0 && !pc->y1) continue;
                    codepoint = (uint32_t)(range->first_unicode_codepoint_in_range + char_idx);
                    nk_tt_GetPackedQuad(range->chardata_for_range, (int)width,
                        (int)height, char_idx, &dummy_x, &dummy_y, &q, 0);

                    /* fill own glyph type with data */
                    glyph = &glyphs[dst_font->glyph_offset + dst_font->glyph_count + (unsigned int)glyph_count];
                    glyph->codepoint = codepoint;
                    glyph->x0 = q.x0; glyph->y0 = q.y0;
                    glyph->x1 = q.x1; glyph->y1 = q.y1;
                    glyph->y0 += (dst_font->ascent + 0.5f);
                    glyph->y1 += (dst_font->ascent + 0.5f);
                    glyph->w = glyph->x1 - glyph->x0 + 0.5f;
                    glyph->h = glyph->y1 - glyph->y0;

                    if (cfg->coord_type == NK_COORD_PIXEL) {
                        glyph->u0 = q.s0 * (float)width;
                        glyph->v0 = q.t0 * (float)height;
                        glyph->u1 = q.s1 * (float)width;
                        glyph->v1 = q.t1 * (float)height;
                    } else {
                        glyph->u0 = q.s0;
                        glyph->v0 = q.t0;
                        glyph->u1 = q.s1;
                        glyph->v1 = q.t1;
                    }
                    glyph->xadvance = (pc->xadvance + cfg->spacing.x);
                    if (cfg->pixel_snap)
                        glyph->xadvance = (float)(int)(glyph->xadvance + 0.5f);
                    glyph_count++;
                }
            }
            dst_font->glyph_count += glyph_count;
            glyph_n += glyph_count;
        } while ((it = it->n) != config_iter);
    }
}

static void
nk_font_bake_custom_data(void *img_memory, int img_width, int img_height,
    struct nk_recti img_dst, const char *texture_data_mask, int tex_width,
    int tex_height, char white, char black)
{
    uint8_t *pixels;
    int y = 0;
    int x = 0;
    int n = 0;

    NK_ASSERT(img_memory);
    NK_ASSERT(img_width);
    NK_ASSERT(img_height);
    NK_ASSERT(texture_data_mask);
    ((void)(tex_height));
    if (!img_memory || !img_width || !img_height || !texture_data_mask)
        return;

    pixels = (uint8_t*)img_memory;
    for (y = 0, n = 0; y < tex_height; ++y) {
        for (x = 0; x < tex_width; ++x, ++n) {
            const int off0 = ((img_dst.x + x) + (img_dst.y + y) * img_width);
            const int off1 = off0 + 1 + tex_width;
            pixels[off0] = (texture_data_mask[n] == white) ? 0xFF : 0x00;
            pixels[off1] = (texture_data_mask[n] == black) ? 0xFF : 0x00;
        }
    }
}

static void
nk_font_bake_convert(void *out_memory, int img_width, int img_height,
    const void *in_memory)
{
    int n = 0;
    uint32_t *dst;
    const uint8_t *src;

    NK_ASSERT(out_memory);
    NK_ASSERT(in_memory);
    NK_ASSERT(img_width);
    NK_ASSERT(img_height);
    if (!out_memory || !in_memory || !img_height || !img_width) return;

    dst = (uint32_t*)out_memory;
    src = (const uint8_t*)in_memory;
    for (n = (int)(img_width * img_height); n > 0; n--)
        *dst++ = ((uint32_t)(*src++) << 24) | 0x00FFFFFF;
}

/* -------------------------------------------------------------
 *
 *                          FONT
 *
 * --------------------------------------------------------------*/
static float
nk_font_text_width(nk_handle handle, float height, const char *text, int len)
{
    uint32_t unicode;
    int text_len  = 0;
    float text_width = 0;
    int glyph_len = 0;
    float scale = 0;

    struct nk_font *font = (struct nk_font*)handle.ptr;
    NK_ASSERT(font);
    NK_ASSERT(font->glyphs);
    if (!font || !text || !len)
        return 0;

    scale = height/font->info.height;
    glyph_len = text_len = nk_utf_decode(text, &unicode, (int)len);
    if (!glyph_len) return 0;
    while (text_len <= (int)len && glyph_len) {
        const struct nk_font_glyph *g;
        if (unicode == NK_UTF_INVALID) break;

        /* query currently drawn glyph information */
        g = nk_font_find_glyph(font, unicode);
        text_width += g->xadvance * scale;

        /* offset next glyph */
        glyph_len = nk_utf_decode(text + text_len, &unicode, (int)len - text_len);
        text_len += glyph_len;
    }
    return text_width;
}

static void
nk_font_query_font_glyph(nk_handle handle, float height,
    struct nk_user_font_glyph *glyph, uint32_t codepoint, uint32_t next_codepoint)
{
    float scale;
    const struct nk_font_glyph *g;
    struct nk_font *font;

    NK_ASSERT(glyph);
    ((void)(next_codepoint));

    font = (struct nk_font*)handle.ptr;
    NK_ASSERT(font);
    NK_ASSERT(font->glyphs);
    if (!font || !glyph)
        return;

    scale = height/font->info.height;
    g = nk_font_find_glyph(font, codepoint);
    glyph->width = (g->x1 - g->x0) * scale;
    glyph->height = (g->y1 - g->y0) * scale;
    glyph->offset = nk_vec2(g->x0 * scale, g->y0 * scale);
    glyph->xadvance = (g->xadvance * scale);
    glyph->uv[0] = nk_vec2(g->u0, g->v0);
    glyph->uv[1] = nk_vec2(g->u1, g->v1);
}

const struct nk_font_glyph*
nk_font_find_glyph(struct nk_font *font, uint32_t unicode)
{
    int i = 0;
    int count;
    int total_glyphs = 0;
    const struct nk_font_glyph *glyph = 0;
    const struct nk_font_config *iter = 0;

    NK_ASSERT(font);
    NK_ASSERT(font->glyphs);
    NK_ASSERT(font->info.ranges);
    if (!font || !font->glyphs) return 0;

    glyph = font->fallback;
    iter = font->config;
    do {count = nk_range_count(iter->range);
        for (i = 0; i < count; ++i) {
            uint32_t f = iter->range[(i*2)+0];
            uint32_t t = iter->range[(i*2)+1];
            int diff = (int)((t - f) + 1);
            if (unicode >= f && unicode <= t)
                return &font->glyphs[((uint32_t)total_glyphs + (unicode - f))];
            total_glyphs += diff;
        }
    } while ((iter = iter->n) != font->config);
    return glyph;
}

static void
nk_font_init(struct nk_font *font, float pixel_height,
    uint32_t fallback_codepoint, struct nk_font_glyph *glyphs,
    const struct nk_baked_font *baked_font, nk_handle atlas)
{
    struct nk_baked_font baked;
    NK_ASSERT(font);
    NK_ASSERT(glyphs);
    NK_ASSERT(baked_font);
    if (!font || !glyphs || !baked_font)
        return;

    baked = *baked_font;
    font->fallback = 0;
    font->info = baked;
    font->scale = (float)pixel_height / (float)font->info.height;
    font->glyphs = &glyphs[baked_font->glyph_offset];
    font->texture = atlas;
    font->fallback_codepoint = fallback_codepoint;
    font->fallback = nk_font_find_glyph(font, fallback_codepoint);

    font->handle.height = font->info.height * font->scale;
    font->handle.width = nk_font_text_width;
    font->handle.userdata.ptr = font;
    font->handle.query = nk_font_query_font_glyph;
    font->handle.texture = font->texture;
}

#define NK_CURSOR_DATA_W 90
#define NK_CURSOR_DATA_H 27
static const char nk_custom_cursor_data[NK_CURSOR_DATA_W * NK_CURSOR_DATA_H + 1] =
{
    "..-         -XXXXXXX-    X    -           X           -XXXXXXX          -          XXXXXXX"
    "..-         -X.....X-   X.X   -          X.X          -X.....X          -          X.....X"
    "---         -XXX.XXX-  X...X  -         X...X         -X....X           -           X....X"
    "X           -  X.X  - X.....X -        X.....X        -X...X            -            X...X"
    "XX          -  X.X  -X.......X-       X.......X       -X..X.X           -           X.X..X"
    "X.X         -  X.X  -XXXX.XXXX-       XXXX.XXXX       -X.X X.X          -          X.X X.X"
    "X..X        -  X.X  -   X.X   -          X.X          -XX   X.X         -         X.X   XX"
    "X...X       -  X.X  -   X.X   -    XX    X.X    XX    -      X.X        -        X.X      "
    "X....X      -  X.X  -   X.X   -   X.X    X.X    X.X   -       X.X       -       X.X       "
    "X.....X     -  X.X  -   X.X   -  X..X    X.X    X..X  -        X.X      -      X.X        "
    "X......X    -  X.X  -   X.X   - X...XXXXXX.XXXXXX...X -         X.X   XX-XX   X.X         "
    "X.......X   -  X.X  -   X.X   -X.....................X-          X.X X.X-X.X X.X          "
    "X........X  -  X.X  -   X.X   - X...XXXXXX.XXXXXX...X -           X.X..X-X..X.X           "
    "X.........X -XXX.XXX-   X.X   -  X..X    X.X    X..X  -            X...X-X...X            "
    "X..........X-X.....X-   X.X   -   X.X    X.X    X.X   -           X....X-X....X           "
    "X......XXXXX-XXXXXXX-   X.X   -    XX    X.X    XX    -          X.....X-X.....X          "
    "X...X..X    ---------   X.X   -          X.X          -          XXXXXXX-XXXXXXX          "
    "X..X X..X   -       -XXXX.XXXX-       XXXX.XXXX       ------------------------------------"
    "X.X  X..X   -       -X.......X-       X.......X       -    XX           XX    -           "
    "XX    X..X  -       - X.....X -        X.....X        -   X.X           X.X   -           "
    "      X..X          -  X...X  -         X...X         -  X..X           X..X  -           "
    "       XX           -   X.X   -          X.X          - X...XXXXXXXXXXXXX...X -           "
    "------------        -    X    -           X           -X.....................X-           "
    "                    ----------------------------------- X...XXXXXXXXXXXXX...X -           "
    "                                                      -  X..X           X..X  -           "
    "                                                      -   X.X           X.X   -           "
    "                                                      -    XX           XX    -           "
};

static unsigned int
nk_decompress_length(unsigned char *input)
{
    return (unsigned int)((input[8] << 24) + (input[9] << 16) + (input[10] << 8) + input[11]);
}

static unsigned char *nk__barrier;
static unsigned char *nk__barrier2;
static unsigned char *nk__barrier3;
static unsigned char *nk__barrier4;
static unsigned char *nk__dout;

static void
nk__match(unsigned char *data, unsigned int length)
{
    /* INVERSE of memmove... write each byte before copying the next...*/
    NK_ASSERT (nk__dout + length <= nk__barrier);
    if (nk__dout + length > nk__barrier) { nk__dout += length; return; }
    if (data < nk__barrier4) { nk__dout = nk__barrier+1; return; }
    while (length--) *nk__dout++ = *data++;
}

static void
nk__lit(unsigned char *data, unsigned int length)
{
    NK_ASSERT (nk__dout + length <= nk__barrier);
    if (nk__dout + length > nk__barrier) { nk__dout += length; return; }
    if (data < nk__barrier2) { nk__dout = nk__barrier+1; return; }
    memcpy(nk__dout, data, length);
    nk__dout += length;
}

#define nk__in2(x)   ((i[x] << 8) + i[(x)+1])
#define nk__in3(x)   ((i[x] << 16) + nk__in2((x)+1))
#define nk__in4(x)   ((i[x] << 24) + nk__in3((x)+1))

static unsigned char*
nk_decompress_token(unsigned char *i)
{
    if (*i >= 0x20) { /* use fewer if's for cases that expand small */
        if (*i >= 0x80)       nk__match(nk__dout-i[1]-1, (unsigned int)i[0] - 0x80 + 1), i += 2;
        else if (*i >= 0x40)  nk__match(nk__dout-(nk__in2(0) - 0x4000 + 1), (unsigned int)i[2]+1), i += 3;
        else /* *i >= 0x20 */ nk__lit(i+1, (unsigned int)i[0] - 0x20 + 1), i += 1 + (i[0] - 0x20 + 1);
    } else { /* more ifs for cases that expand large, since overhead is amortized */
        if (*i >= 0x18)       nk__match(nk__dout-(unsigned int)(nk__in3(0) - 0x180000 + 1), (unsigned int)i[3]+1), i += 4;
        else if (*i >= 0x10)  nk__match(nk__dout-(unsigned int)(nk__in3(0) - 0x100000 + 1), (unsigned int)nk__in2(3)+1), i += 5;
        else if (*i >= 0x08)  nk__lit(i+2, (unsigned int)nk__in2(0) - 0x0800 + 1), i += 2 + (nk__in2(0) - 0x0800 + 1);
        else if (*i == 0x07)  nk__lit(i+3, (unsigned int)nk__in2(1) + 1), i += 3 + (nk__in2(1) + 1);
        else if (*i == 0x06)  nk__match(nk__dout-(unsigned int)(nk__in3(1)+1), i[4]+1u), i += 5;
        else if (*i == 0x04)  nk__match(nk__dout-(unsigned int)(nk__in3(1)+1), (unsigned int)nk__in2(4)+1u), i += 6;
    }
    return i;
}

static unsigned int
nk_adler32(unsigned int adler32, unsigned char *buffer, unsigned int buflen)
{
    const unsigned long ADLER_MOD = 65521;
    unsigned long s1 = adler32 & 0xffff, s2 = adler32 >> 16;
    unsigned long blocklen, i;

    blocklen = buflen % 5552;
    while (buflen) {
        for (i=0; i + 7 < blocklen; i += 8) {
            s1 += buffer[0]; s2 += s1;
            s1 += buffer[1]; s2 += s1;
            s1 += buffer[2]; s2 += s1;
            s1 += buffer[3]; s2 += s1;
            s1 += buffer[4]; s2 += s1;
            s1 += buffer[5]; s2 += s1;
            s1 += buffer[6]; s2 += s1;
            s1 += buffer[7]; s2 += s1;
            buffer += 8;
        }
        for (; i < blocklen; ++i) {
            s1 += *buffer++; s2 += s1;
        }

        s1 %= ADLER_MOD; s2 %= ADLER_MOD;
        buflen -= (unsigned int)blocklen;
        blocklen = 5552;
    }
    return (unsigned int)(s2 << 16) + (unsigned int)s1;
}

static unsigned int
nk_decompress(unsigned char *output, unsigned char *i, unsigned int length)
{
    unsigned int olen;
    if (nk__in4(0) != 0x57bC0000) return 0;
    if (nk__in4(4) != 0)          return 0; /* error! stream is > 4GB */
    olen = nk_decompress_length(i);
    nk__barrier2 = i;
    nk__barrier3 = i+length;
    nk__barrier = output + olen;
    nk__barrier4 = output;
    i += 16;

    nk__dout = output;
    for (;;) {
        unsigned char *old_i = i;
        i = nk_decompress_token(i);
        if (i == old_i) {
            if (*i == 0x05 && i[1] == 0xfa) {
                NK_ASSERT(nk__dout == output + olen);
                if (nk__dout != output + olen) return 0;
                if (nk_adler32(1, output, olen) != (unsigned int) nk__in4(2))
                    return 0;
                return olen;
            } else {
                NK_ASSERT(0); /* NOTREACHED */
                return 0;
            }
        }
        NK_ASSERT(nk__dout <= output + olen);
        if (nk__dout > output + olen)
            return 0;
    }
}

static unsigned int
nk_decode_85_byte(char c)
{ return (unsigned int)((c >= '\\') ? c-36 : c-35); }

static void
nk_decode_85(unsigned char* dst, const unsigned char* src)
{
    while (*src)
    {
        unsigned int tmp =
            nk_decode_85_byte((char)src[0]) +
            85 * (nk_decode_85_byte((char)src[1]) +
            85 * (nk_decode_85_byte((char)src[2]) +
            85 * (nk_decode_85_byte((char)src[3]) +
            85 * nk_decode_85_byte((char)src[4]))));

        /* we can't assume little-endianess. */
        dst[0] = (unsigned char)((tmp >> 0) & 0xFF);
        dst[1] = (unsigned char)((tmp >> 8) & 0xFF);
        dst[2] = (unsigned char)((tmp >> 16) & 0xFF);
        dst[3] = (unsigned char)((tmp >> 24) & 0xFF);

        src += 5;
        dst += 4;
    }
}

/* -------------------------------------------------------------
 *
 *                          FONT ATLAS
 *
 * --------------------------------------------------------------*/
struct nk_font_config
nk_font_config(float pixel_height)
{
    struct nk_font_config cfg;
    nk_zero_struct(cfg);
    cfg.ttf_blob = 0;
    cfg.ttf_size = 0;
    cfg.ttf_data_owned_by_atlas = 0;
    cfg.size = pixel_height;
    cfg.oversample_h = 3;
    cfg.oversample_v = 1;
    cfg.pixel_snap = 0;
    cfg.coord_type = NK_COORD_UV;
    cfg.spacing = nk_vec2(0,0);
    cfg.range = nk_font_default_glyph_ranges();
    cfg.merge_mode = 0;
    cfg.fallback_glyph = '?';
    cfg.font = 0;
    cfg.n = 0;
    return cfg;
}

void
nk_font_atlas_init_default(struct nk_font_atlas *atlas)
{
    NK_ASSERT(atlas);
    if (!atlas) return;
    nk_zero_struct(*atlas);
    atlas->temporary.userdata.ptr = 0;
    atlas->temporary.alloc = nk_malloc;
    atlas->temporary.free = nk_mfree;
    atlas->permanent.userdata.ptr = 0;
    atlas->permanent.alloc = nk_malloc;
    atlas->permanent.free = nk_mfree;
}

void
nk_font_atlas_init(struct nk_font_atlas *atlas, struct nk_allocator *alloc)
{
    NK_ASSERT(atlas);
    NK_ASSERT(alloc);
    if (!atlas || !alloc) return;
    nk_zero_struct(*atlas);
    atlas->permanent = *alloc;
    atlas->temporary = *alloc;
}

void
nk_font_atlas_init_custom(struct nk_font_atlas *atlas,
    struct nk_allocator *permanent, struct nk_allocator *temporary)
{
    NK_ASSERT(atlas);
    NK_ASSERT(permanent);
    NK_ASSERT(temporary);
    if (!atlas || !permanent || !temporary) return;
    nk_zero_struct(*atlas);
    atlas->permanent = *permanent;
    atlas->temporary = *temporary;
}

void
nk_font_atlas_begin(struct nk_font_atlas *atlas)
{
    NK_ASSERT(atlas);
    NK_ASSERT(atlas->temporary.alloc && atlas->temporary.free);
    NK_ASSERT(atlas->permanent.alloc && atlas->permanent.free);
    if (!atlas || !atlas->permanent.alloc || !atlas->permanent.free ||
        !atlas->temporary.alloc || !atlas->temporary.free) return;
    if (atlas->glyphs) {
        atlas->permanent.free(atlas->permanent.userdata, atlas->glyphs);
        atlas->glyphs = 0;
    }
    if (atlas->pixel) {
        atlas->permanent.free(atlas->permanent.userdata, atlas->pixel);
        atlas->pixel = 0;
    }
}

struct nk_font*
nk_font_atlas_add(struct nk_font_atlas *atlas, const struct nk_font_config *config)
{
    struct nk_font *font = 0;
    struct nk_font_config *cfg;

    NK_ASSERT(atlas);
    NK_ASSERT(atlas->permanent.alloc);
    NK_ASSERT(atlas->permanent.free);
    NK_ASSERT(atlas->temporary.alloc);
    NK_ASSERT(atlas->temporary.free);

    NK_ASSERT(config);
    NK_ASSERT(config->ttf_blob);
    NK_ASSERT(config->ttf_size);
    NK_ASSERT(config->size > 0.0f);

    if (!atlas || !config || !config->ttf_blob || !config->ttf_size || config->size <= 0.0f||
        !atlas->permanent.alloc || !atlas->permanent.free ||
        !atlas->temporary.alloc || !atlas->temporary.free)
        return 0;

    /* allocate font config  */
    cfg = (struct nk_font_config*)
        atlas->permanent.alloc(atlas->permanent.userdata,0, sizeof(struct nk_font_config));
    memcpy(cfg, config, sizeof(*config));
    cfg->n = cfg;
    cfg->p = cfg;

    if (!config->merge_mode) {
        /* insert font config into list */
        if (!atlas->config) {
            atlas->config = cfg;
            cfg->next = 0;
        } else {
            struct nk_font_config *i = atlas->config;
            while (i->next) i = i->next;
            i->next = cfg;
            cfg->next = 0;
        }
        /* allocate new font */
        font = (struct nk_font*)
            atlas->permanent.alloc(atlas->permanent.userdata,0, sizeof(struct nk_font));
        NK_ASSERT(font);
        memset(font, 0, sizeof(*font));
        if (!font) return 0;
        font->config = cfg;

        /* insert font into list */
        if (!atlas->fonts) {
            atlas->fonts = font;
            font->next = 0;
        } else {
            struct nk_font *i = atlas->fonts;
            while (i->next) i = i->next;
            i->next = font;
            font->next = 0;
        }
        cfg->font = &font->info;
    } else {
        /* extend previously added font */
        struct nk_font *f = 0;
        struct nk_font_config *c = 0;
        NK_ASSERT(atlas->font_num);
        f = atlas->fonts;
        c = f->config;
        cfg->font = &f->info;

        cfg->n = c;
        cfg->p = c->p;
        c->p->n = cfg;
        c->p = cfg;
    }
    /* create own copy of .TTF font blob */
    if (!config->ttf_data_owned_by_atlas) {
        cfg->ttf_blob = atlas->permanent.alloc(atlas->permanent.userdata,0, cfg->ttf_size);
        NK_ASSERT(cfg->ttf_blob);
        if (!cfg->ttf_blob) {
            atlas->font_num++;
            return 0;
        }
        memcpy(cfg->ttf_blob, config->ttf_blob, cfg->ttf_size);
        cfg->ttf_data_owned_by_atlas = 1;
    }
    atlas->font_num++;
    return font;
}

struct nk_font*
nk_font_atlas_add_from_memory(struct nk_font_atlas *atlas, void *memory,
    size_t size, float height, const struct nk_font_config *config)
{
    struct nk_font_config cfg;
    NK_ASSERT(memory);
    NK_ASSERT(size);

    NK_ASSERT(atlas);
    NK_ASSERT(atlas->temporary.alloc);
    NK_ASSERT(atlas->temporary.free);
    NK_ASSERT(atlas->permanent.alloc);
    NK_ASSERT(atlas->permanent.free);
    if (!atlas || !atlas->temporary.alloc || !atlas->temporary.free || !memory || !size ||
        !atlas->permanent.alloc || !atlas->permanent.free)
        return 0;

    cfg = (config) ? *config: nk_font_config(height);
    cfg.ttf_blob = memory;
    cfg.ttf_size = size;
    cfg.size = height;
    cfg.ttf_data_owned_by_atlas = 0;
    return nk_font_atlas_add(atlas, &cfg);
}

struct nk_font*
nk_font_atlas_add_from_file(struct nk_font_atlas *atlas, const char *file_path,
    float height, const struct nk_font_config *config)
{
    size_t size;
    char *memory;
    struct nk_font_config cfg;

    NK_ASSERT(atlas);
    NK_ASSERT(atlas->temporary.alloc);
    NK_ASSERT(atlas->temporary.free);
    NK_ASSERT(atlas->permanent.alloc);
    NK_ASSERT(atlas->permanent.free);

    if (!atlas || !file_path) return 0;
    memory = nk_file_load(file_path, &size, &atlas->permanent);
    if (!memory) return 0;

    cfg = (config) ? *config: nk_font_config(height);
    cfg.ttf_blob = memory;
    cfg.ttf_size = size;
    cfg.size = height;
    cfg.ttf_data_owned_by_atlas = 1;
    return nk_font_atlas_add(atlas, &cfg);
}

struct nk_font*
nk_font_atlas_add_compressed(struct nk_font_atlas *atlas,
    void *compressed_data, size_t compressed_size, float height,
    const struct nk_font_config *config)
{
    unsigned int decompressed_size;
    void *decompressed_data;
    struct nk_font_config cfg;

    NK_ASSERT(atlas);
    NK_ASSERT(atlas->temporary.alloc);
    NK_ASSERT(atlas->temporary.free);
    NK_ASSERT(atlas->permanent.alloc);
    NK_ASSERT(atlas->permanent.free);

    NK_ASSERT(compressed_data);
    NK_ASSERT(compressed_size);
    if (!atlas || !compressed_data || !atlas->temporary.alloc || !atlas->temporary.free ||
        !atlas->permanent.alloc || !atlas->permanent.free)
        return 0;

    decompressed_size = nk_decompress_length((unsigned char*)compressed_data);
    decompressed_data = atlas->permanent.alloc(atlas->permanent.userdata,0,decompressed_size);
    NK_ASSERT(decompressed_data);
    if (!decompressed_data) return 0;
    nk_decompress((unsigned char*)decompressed_data, (unsigned char*)compressed_data,
        (unsigned int)compressed_size);

    cfg = (config) ? *config: nk_font_config(height);
    cfg.ttf_blob = decompressed_data;
    cfg.ttf_size = decompressed_size;
    cfg.size = height;
    cfg.ttf_data_owned_by_atlas = 1;
    return nk_font_atlas_add(atlas, &cfg);
}

struct nk_font*
nk_font_atlas_add_compressed_base85(struct nk_font_atlas *atlas,
    const char *data_base85, float height, const struct nk_font_config *config)
{
    int compressed_size;
    void *compressed_data;
    struct nk_font *font;

    NK_ASSERT(atlas);
    NK_ASSERT(atlas->temporary.alloc);
    NK_ASSERT(atlas->temporary.free);
    NK_ASSERT(atlas->permanent.alloc);
    NK_ASSERT(atlas->permanent.free);

    NK_ASSERT(data_base85);
    if (!atlas || !data_base85 || !atlas->temporary.alloc || !atlas->temporary.free ||
        !atlas->permanent.alloc || !atlas->permanent.free)
        return 0;

    compressed_size = (((int)strlen(data_base85) + 4) / 5) * 4;
    compressed_data = atlas->temporary.alloc(atlas->temporary.userdata,0, (size_t)compressed_size);
    NK_ASSERT(compressed_data);
    if (!compressed_data) return 0;
    nk_decode_85((unsigned char*)compressed_data, (const unsigned char*)data_base85);
    font = nk_font_atlas_add_compressed(atlas, compressed_data,
                    (size_t)compressed_size, height, config);
    atlas->temporary.free(atlas->temporary.userdata, compressed_data);
    return font;
}

const void*
nk_font_atlas_bake(struct nk_font_atlas *atlas, int *width, int *height,
    enum nk_font_atlas_format fmt)
{
    void *tmp = 0;
    size_t tmp_size, img_size;
    struct nk_font *font_iter;
    struct nk_font_baker *baker;

    NK_ASSERT(atlas);
    NK_ASSERT(atlas->temporary.alloc);
    NK_ASSERT(atlas->temporary.free);
    NK_ASSERT(atlas->permanent.alloc);
    NK_ASSERT(atlas->permanent.free);

    NK_ASSERT(width);
    NK_ASSERT(height);
    if (!atlas || !width || !height ||
        !atlas->temporary.alloc || !atlas->temporary.free ||
        !atlas->permanent.alloc || !atlas->permanent.free)
        return 0;

    NK_ASSERT(atlas->font_num);
    if (!atlas->font_num) return 0;

    /* allocate temporary baker memory required for the baking process */
    nk_font_baker_memory(&tmp_size, &atlas->glyph_count, atlas->config, atlas->font_num);
    tmp = atlas->temporary.alloc(atlas->temporary.userdata,0, tmp_size);
    NK_ASSERT(tmp);
    if (!tmp) goto failed;

    /* allocate glyph memory for all fonts */
    baker = nk_font_baker(tmp, atlas->glyph_count, atlas->font_num, &atlas->temporary);
    atlas->glyphs = (struct nk_font_glyph*)atlas->permanent.alloc(
        atlas->permanent.userdata,0, sizeof(struct nk_font_glyph)*(size_t)atlas->glyph_count);
    NK_ASSERT(atlas->glyphs);
    if (!atlas->glyphs)
        goto failed;

    /* pack all glyphs into a tight fit space */
    atlas->custom.w = (NK_CURSOR_DATA_W*2)+1;
    atlas->custom.h = NK_CURSOR_DATA_H + 1;
    if (!nk_font_bake_pack(baker, &img_size, width, height, &atlas->custom,
        atlas->config, atlas->font_num, &atlas->temporary))
        goto failed;

    /* allocate memory for the baked image font atlas */
    atlas->pixel = atlas->temporary.alloc(atlas->temporary.userdata,0, img_size);
    NK_ASSERT(atlas->pixel);
    if (!atlas->pixel)
        goto failed;

    /* bake glyphs and custom white pixel into image */
    nk_font_bake(baker, atlas->pixel, *width, *height,
        atlas->glyphs, atlas->glyph_count, atlas->config, atlas->font_num);
    nk_font_bake_custom_data(atlas->pixel, *width, *height, atlas->custom,
            nk_custom_cursor_data, NK_CURSOR_DATA_W, NK_CURSOR_DATA_H, '.', 'X');

    if (fmt == NK_FONT_ATLAS_RGBA32) {
        /* convert alpha8 image into rgba32 image */
        void *img_rgba = atlas->temporary.alloc(atlas->temporary.userdata,0,
                            (size_t)(*width * *height * 4));
        NK_ASSERT(img_rgba);
        if (!img_rgba) goto failed;
        nk_font_bake_convert(img_rgba, *width, *height, atlas->pixel);
        atlas->temporary.free(atlas->temporary.userdata, atlas->pixel);
        atlas->pixel = img_rgba;
    }
    atlas->tex_width = *width;
    atlas->tex_height = *height;

    /* initialize each font */
    for (font_iter = atlas->fonts; font_iter; font_iter = font_iter->next) {
        struct nk_font *font = font_iter;
        struct nk_font_config *config = font->config;
		nk_handle handle = {0};
        nk_font_init(font, config->size, config->fallback_glyph, atlas->glyphs,
            config->font, handle);
    }

    /* free temporary memory */
    atlas->temporary.free(atlas->temporary.userdata, tmp);
    return atlas->pixel;

failed:
    /* error so cleanup all memory */
    if (tmp) atlas->temporary.free(atlas->temporary.userdata, tmp);
    if (atlas->glyphs) {
        atlas->permanent.free(atlas->permanent.userdata, atlas->glyphs);
        atlas->glyphs = 0;
    }
    if (atlas->pixel) {
        atlas->temporary.free(atlas->temporary.userdata, atlas->pixel);
        atlas->pixel = 0;
    }
    return 0;
}

void
nk_font_atlas_end(struct nk_font_atlas *atlas, nk_handle texture,
    struct nk_draw_null_texture *null)
{
    struct nk_font *font_iter;
    NK_ASSERT(atlas);
    if (!atlas) {
        if (!null) return;
        null->texture = texture;
        null->uv = nk_vec2(0.5f,0.5f);
    }
    if (null) {
        null->texture = texture;
        null->uv.x = (atlas->custom.x + 0.5f)/(float)atlas->tex_width;
        null->uv.y = (atlas->custom.y + 0.5f)/(float)atlas->tex_height;
    }
    for (font_iter = atlas->fonts; font_iter; font_iter = font_iter->next) {
        font_iter->texture = texture;
        font_iter->handle.texture = texture;
    }

    atlas->temporary.free(atlas->temporary.userdata, atlas->pixel);
    atlas->pixel = 0;
    atlas->tex_width = 0;
    atlas->tex_height = 0;
    atlas->custom.x = 0;
    atlas->custom.y = 0;
    atlas->custom.w = 0;
    atlas->custom.h = 0;
}

void
nk_font_atlas_cleanup(struct nk_font_atlas *atlas)
{
    NK_ASSERT(atlas);
    NK_ASSERT(atlas->temporary.alloc);
    NK_ASSERT(atlas->temporary.free);
    NK_ASSERT(atlas->permanent.alloc);
    NK_ASSERT(atlas->permanent.free);
    if (!atlas || !atlas->permanent.alloc || !atlas->permanent.free) return;
    if (atlas->config) {
        struct nk_font_config *iter;
        for (iter = atlas->config; iter; iter = iter->next) {
            struct nk_font_config *i;
            for (i = iter->n; i != iter; i = i->n) {
                atlas->permanent.free(atlas->permanent.userdata, i->ttf_blob);
                i->ttf_blob = 0;
            }
            atlas->permanent.free(atlas->permanent.userdata, iter->ttf_blob);
            iter->ttf_blob = 0;
        }
    }
}

void
nk_font_atlas_clear(struct nk_font_atlas *atlas)
{
    NK_ASSERT(atlas);
    NK_ASSERT(atlas->temporary.alloc);
    NK_ASSERT(atlas->temporary.free);
    NK_ASSERT(atlas->permanent.alloc);
    NK_ASSERT(atlas->permanent.free);
    if (!atlas || !atlas->permanent.alloc || !atlas->permanent.free) return;

    if (atlas->config) {
        struct nk_font_config *iter, *next;
        for (iter = atlas->config; iter; iter = next) {
            struct nk_font_config *i, *n;
            for (i = iter->n; i != iter; i = n) {
                n = i->n;
                if (i->ttf_blob)
                    atlas->permanent.free(atlas->permanent.userdata, i->ttf_blob);
                atlas->permanent.free(atlas->permanent.userdata, i);
            }
            next = iter->next;
            if (i->ttf_blob)
                atlas->permanent.free(atlas->permanent.userdata, iter->ttf_blob);
            atlas->permanent.free(atlas->permanent.userdata, iter);
        }
        atlas->config = 0;
    }
    if (atlas->fonts) {
        struct nk_font *iter, *next;
        for (iter = atlas->fonts; iter; iter = next) {
            next = iter->next;
            atlas->permanent.free(atlas->permanent.userdata, iter);
        }
        atlas->fonts = 0;
    }
    if (atlas->glyphs)
        atlas->permanent.free(atlas->permanent.userdata, atlas->glyphs);
    nk_zero_struct(*atlas);
}
