#pragma once

#include <stdbool.h>

/* Partition a scrolling text segment around the stationary guide border.
 * The caller advances the source offset even for omitted border rows.
 * All surviving interior rows use a symmetric two-pixel horizontal inset.
 */
typedef struct {
    int rows;
    int inset;
    bool skip;
} tp_guide_clip_t;

static inline tp_guide_clip_t tp_guide_clip(int destination, int rows,
                                           int top, int bottom, bool enabled)
{
    tp_guide_clip_t clip = {.rows=rows};
    if (!enabled) return clip;
    int boundary;
    if (destination < top) boundary=top;
    else if (bottom-top<=4&&destination<bottom) { boundary=bottom; clip.skip=true; }
    else if (destination < top+2) { boundary=top+2; clip.skip=true; }
    else if (destination < bottom-2) { boundary=bottom-2; clip.inset=2; }
    else if (destination < bottom) { boundary=bottom; clip.skip=true; }
    else return clip;
    if (clip.rows > boundary-destination) clip.rows=boundary-destination;
    return clip;
}
