#pragma once

#include "internal.h"

/* stb_rect_pack.h - v0.05 - public domain - rectangle packing */
/* Sean Barrett 2014 */
#define NK_RP__MAXVAL  0xffff
typedef unsigned short nk_rp_coord;

struct nk_rp_rect {
    /* reserved for your use: */
    int id;
    /* input: */
    nk_rp_coord w, h;
    /* output: */
    nk_rp_coord x, y;
    int was_packed;
    /* non-zero if valid packing */
}; /* 16 bytes, nominally */

struct nk_rp_node {
    nk_rp_coord  x,y;
    struct nk_rp_node  *next;
};

struct nk_rp_context {
    int width;
    int height;
    int align;
    int init_mode;
    int heuristic;
    int num_nodes;
    struct nk_rp_node *active_head;
    struct nk_rp_node *free_head;
    struct nk_rp_node extra[2];
    /* we allocate two extra nodes so optimal user-node-count is 'width' not 'width+2' */
};

struct nk_rp__findresult {
    int x,y;
    struct nk_rp_node **prev_link;
};

enum NK_RP_HEURISTIC {
    NK_RP_HEURISTIC_Skyline_default=0,
    NK_RP_HEURISTIC_Skyline_BL_sortHeight = NK_RP_HEURISTIC_Skyline_default,
    NK_RP_HEURISTIC_Skyline_BF_sortHeight
};
enum NK_RP_INIT_STATE{NK_RP__INIT_skyline = 1};

static void
nk_rp_setup_allow_out_of_mem(struct nk_rp_context *context, int allow_out_of_mem)
{
    if (allow_out_of_mem)
        /* if it's ok to run out of memory, then don't bother aligning them; */
        /* this gives better packing, but may fail due to OOM (even though */
        /* the rectangles easily fit). @TODO a smarter approach would be to only */
        /* quantize once we've hit OOM, then we could get rid of this parameter. */
        context->align = 1;
    else {
        /* if it's not ok to run out of memory, then quantize the widths */
        /* so that num_nodes is always enough nodes. */
        /* */
        /* I.e. num_nodes * align >= width */
        /*                  align >= width / num_nodes */
        /*                  align = ceil(width/num_nodes) */
        context->align = (context->width + context->num_nodes-1) / context->num_nodes;
    }
}

static void
nk_rp_init_target(struct nk_rp_context *context, int width, int height,
    struct nk_rp_node *nodes, int num_nodes)
{
    int i;
#ifndef STBRP_LARGE_RECTS
    NK_ASSERT(width <= 0xffff && height <= 0xffff);
#endif

    for (i=0; i < num_nodes-1; ++i)
        nodes[i].next = &nodes[i+1];
    nodes[i].next = 0;
    context->init_mode = NK_RP__INIT_skyline;
    context->heuristic = NK_RP_HEURISTIC_Skyline_default;
    context->free_head = &nodes[0];
    context->active_head = &context->extra[0];
    context->width = width;
    context->height = height;
    context->num_nodes = num_nodes;
    nk_rp_setup_allow_out_of_mem(context, 0);

    /* node 0 is the full width, node 1 is the sentinel (lets us not store width explicitly) */
    context->extra[0].x = 0;
    context->extra[0].y = 0;
    context->extra[0].next = &context->extra[1];
    context->extra[1].x = (nk_rp_coord) width;
    context->extra[1].y = 65535;
    context->extra[1].next = 0;
}

/* find minimum y position if it starts at x1 */
static int
nk_rp__skyline_find_min_y(struct nk_rp_context *c, struct nk_rp_node *first,
    int x0, int width, int *pwaste)
{
    struct nk_rp_node *node = first;
    int x1 = x0 + width;
    int min_y, visited_width, waste_area;
    NK_ASSERT(first->x <= x0);
    ((void)(c));

    NK_ASSERT(node->next->x > x0);
    /* we ended up handling this in the caller for efficiency */
    NK_ASSERT(node->x <= x0);

    min_y = 0;
    waste_area = 0;
    visited_width = 0;
    while (node->x < x1)
    {
        if (node->y > min_y) {
            /* raise min_y higher. */
            /* we've accounted for all waste up to min_y, */
            /* but we'll now add more waste for everything we've visited */
            waste_area += visited_width * (node->y - min_y);
            min_y = node->y;
            /* the first time through, visited_width might be reduced */
            if (node->x < x0)
            visited_width += node->next->x - x0;
            else
            visited_width += node->next->x - node->x;
        } else {
            /* add waste area */
            int under_width = node->next->x - node->x;
            if (under_width + visited_width > width)
            under_width = width - visited_width;
            waste_area += under_width * (min_y - node->y);
            visited_width += under_width;
        }
        node = node->next;
    }
    *pwaste = waste_area;
    return min_y;
}

static struct nk_rp__findresult
nk_rp__skyline_find_best_pos(struct nk_rp_context *c, int width, int height)
{
    int best_waste = (1<<30), best_x, best_y = (1 << 30);
    struct nk_rp__findresult fr;
    struct nk_rp_node **prev, *node, *tail, **best = 0;

    /* align to multiple of c->align */
    width = (width + c->align - 1);
    width -= width % c->align;
    NK_ASSERT(width % c->align == 0);

    node = c->active_head;
    prev = &c->active_head;
    while (node->x + width <= c->width) {
        int y,waste;
        y = nk_rp__skyline_find_min_y(c, node, node->x, width, &waste);
        /* actually just want to test BL */
        if (c->heuristic == NK_RP_HEURISTIC_Skyline_BL_sortHeight) {
            /* bottom left */
            if (y < best_y) {
            best_y = y;
            best = prev;
            }
        } else {
            /* best-fit */
            if (y + height <= c->height) {
                /* can only use it if it first vertically */
                if (y < best_y || (y == best_y && waste < best_waste)) {
                    best_y = y;
                    best_waste = waste;
                    best = prev;
                }
            }
        }
        prev = &node->next;
        node = node->next;
    }
    best_x = (best == 0) ? 0 : (*best)->x;

    /* if doing best-fit (BF), we also have to try aligning right edge to each node position */
    /* */
    /* e.g, if fitting */
    /* */
    /*     ____________________ */
    /*    |____________________| */
    /* */
    /*            into */
    /* */
    /*   |                         | */
    /*   |             ____________| */
    /*   |____________| */
    /* */
    /* then right-aligned reduces waste, but bottom-left BL is always chooses left-aligned */
    /* */
    /* This makes BF take about 2x the time */
    if (c->heuristic == NK_RP_HEURISTIC_Skyline_BF_sortHeight)
    {
        tail = c->active_head;
        node = c->active_head;
        prev = &c->active_head;
        /* find first node that's admissible */
        while (tail->x < width)
            tail = tail->next;
        while (tail)
        {
            int xpos = tail->x - width;
            int y,waste;
            NK_ASSERT(xpos >= 0);
            /* find the left position that matches this */
            while (node->next->x <= xpos) {
                prev = &node->next;
                node = node->next;
            }
            NK_ASSERT(node->next->x > xpos && node->x <= xpos);
            y = nk_rp__skyline_find_min_y(c, node, xpos, width, &waste);
            if (y + height < c->height) {
                if (y <= best_y) {
                    if (y < best_y || waste < best_waste || (waste==best_waste && xpos < best_x)) {
                        best_x = xpos;
                        NK_ASSERT(y <= best_y);
                        best_y = y;
                        best_waste = waste;
                        best = prev;
                    }
                }
            }
            tail = tail->next;
        }
    }
    fr.prev_link = best;
    fr.x = best_x;
    fr.y = best_y;
    return fr;
}

static struct nk_rp__findresult
nk_rp__skyline_pack_rectangle(struct nk_rp_context *context, int width, int height)
{
    /* find best position according to heuristic */
    struct nk_rp__findresult res = nk_rp__skyline_find_best_pos(context, width, height);
    struct nk_rp_node *node, *cur;

    /* bail if: */
    /*    1. it failed */
    /*    2. the best node doesn't fit (we don't always check this) */
    /*    3. we're out of memory */
    if (res.prev_link == 0 || res.y + height > context->height || context->free_head == 0) {
        res.prev_link = 0;
        return res;
    }

    /* on success, create new node */
    node = context->free_head;
    node->x = (nk_rp_coord) res.x;
    node->y = (nk_rp_coord) (res.y + height);

    context->free_head = node->next;

    /* insert the new node into the right starting point, and */
    /* let 'cur' point to the remaining nodes needing to be */
    /* stitched back in */
    cur = *res.prev_link;
    if (cur->x < res.x) {
        /* preserve the existing one, so start testing with the next one */
        struct nk_rp_node *next = cur->next;
        cur->next = node;
        cur = next;
    } else {
        *res.prev_link = node;
    }

    /* from here, traverse cur and free the nodes, until we get to one */
    /* that shouldn't be freed */
    while (cur->next && cur->next->x <= res.x + width) {
        struct nk_rp_node *next = cur->next;
        /* move the current node to the free list */
        cur->next = context->free_head;
        context->free_head = cur;
        cur = next;
    }
    /* stitch the list back in */
    node->next = cur;

    if (cur->x < res.x + width)
        cur->x = (nk_rp_coord) (res.x + width);
    return res;
}

static int
nk_rect_height_compare(const void *a, const void *b)
{
    const struct nk_rp_rect *p = (const struct nk_rp_rect *) a;
    const struct nk_rp_rect *q = (const struct nk_rp_rect *) b;
    if (p->h > q->h)
        return -1;
    if (p->h < q->h)
        return  1;
    return (p->w > q->w) ? -1 : (p->w < q->w);
}

static int
nk_rect_original_order(const void *a, const void *b)
{
    const struct nk_rp_rect *p = (const struct nk_rp_rect *) a;
    const struct nk_rp_rect *q = (const struct nk_rp_rect *) b;
    return (p->was_packed < q->was_packed) ? -1 : (p->was_packed > q->was_packed);
}

static void
nk_rp_qsort(struct nk_rp_rect *array, unsigned int len, int(*cmp)(const void*,const void*))
{
    /* iterative quick sort */
    #define NK_MAX_SORT_STACK 64
    unsigned right, left = 0, stack[NK_MAX_SORT_STACK], pos = 0;
    unsigned seed = len/2 * 69069+1;
    for (;;) {
        for (; left+1 < len; len++) {
            struct nk_rp_rect pivot, tmp;
            if (pos == NK_MAX_SORT_STACK) len = stack[pos = 0];
            pivot = array[left+seed%(len-left)];
            seed = seed * 69069 + 1;
            stack[pos++] = len;
            for (right = left-1;;) {
                while (cmp(&array[++right], &pivot) < 0);
                while (cmp(&pivot, &array[--len]) < 0);
                if (right >= len) break;
                tmp = array[right];
                array[right] = array[len];
                array[len] = tmp;
            }
        }
        if (pos == 0) break;
        left = len;
        len = stack[--pos];
    }
    #undef NK_MAX_SORT_STACK
}

static void
nk_rp_pack_rects(struct nk_rp_context *context, struct nk_rp_rect *rects, int num_rects)
{
    int i;
    /* we use the 'was_packed' field internally to allow sorting/unsorting */
    for (i=0; i < num_rects; ++i) {
        rects[i].was_packed = i;
    }

    /* sort according to heuristic */
    nk_rp_qsort(rects, (unsigned)num_rects, nk_rect_height_compare);

    for (i=0; i < num_rects; ++i) {
        struct nk_rp__findresult fr = nk_rp__skyline_pack_rectangle(context, rects[i].w, rects[i].h);
        if (fr.prev_link) {
            rects[i].x = (nk_rp_coord) fr.x;
            rects[i].y = (nk_rp_coord) fr.y;
        } else {
            rects[i].x = rects[i].y = NK_RP__MAXVAL;
        }
    }

    /* unsort */
    nk_rp_qsort(rects, (unsigned)num_rects, nk_rect_original_order);

    /* set was_packed flags */
    for (i=0; i < num_rects; ++i)
        rects[i].was_packed = !(rects[i].x == NK_RP__MAXVAL && rects[i].y == NK_RP__MAXVAL);
}
