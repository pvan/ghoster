


// some test vids
// https://www.youtube.com/watch?v=TmoBMjbY5Nw
// https://www.youtube.com/watch?v=7Z0lNch5qkQ
// https://www.youtube.com/watch?v=63gdelpCp4k
// https://www.youtube.com/watch?v=T9hHKYfXIE0
void draw_rect(u32 *dst, int w, int h, RECT subRect)
{
    int rx = subRect.left;
    int ry = subRect.top;
    int rr = subRect.right;
    int rb = subRect.bottom;
    for (int x = 0; x < w; x++)
    {
        for (int y = 0; y < h; y++)
        {
            if ((x==rx || x==rr-1) && y>ry && y<rb)
                dst[y*w + x] = 0xff00ff00;
            if ((y==ry || y==rb-1) && x>rx && x<rr)
                dst[y*w + x] = 0xff00ff00;
        }
    }
}

void copy_subrect(u32 *dst, RECT subRect, u32 *src, int sw, int sh)
{
    int rx = subRect.left;
    int ry = subRect.top;
    int rw = subRect.right-subRect.left;
    int rh = subRect.bottom-subRect.top;
    for (int x = 0; x < rw; x++)
    {
        for (int y = 0; y < rh; y++)
        {
            dst[y*rw + x] = src[(y+ry)*sw + (x+rx)];
        }
    }
}

bool row_all_black(u8 *buf, int w, int h, int thres, int row)
{
    for (int x = 0; x < w; x++)
    {
        if (buf[(row*w + x)*4 + 0] > thres) return false;
        if (buf[(row*w + x)*4 + 1] > thres) return false;
        if (buf[(row*w + x)*4 + 2] > thres) return false;
        // if (buf[(row*w + x)*4 + 3] > thres) return false;  // don't check alpha
    }
    return true;
}
bool col_all_black(u8 *buf, int w, int h, int thres, int col)
{
    for (int y = 0; y < h; y++)
    {
        if (buf[(y*w + col)*4 + 0] > thres) return false;
        if (buf[(y*w + col)*4 + 1] > thres) return false;
        if (buf[(y*w + col)*4 + 2] > thres) return false;
        // if (buf[(y*w + col)*4 + 3] > thres) return false;  // don't check alpha
    }
    return true;
}

RECT calc_autocroped_rect(u32 *buf, int bw, int bh, int thres=AUTOCROP_DEFAULT_THRESHOLD)
{
    int startRow = 0;
    for (int r = 0; r < bh; r++)
    {
        if (!row_all_black((u8*)buf,bw,bh, thres,r))
        {
            startRow = r;
            break;
        }
    }
    int endRow = 0;
    for (int r = bh-1; r >= 0; r--)
    {
        if (!row_all_black((u8*)buf,bw,bh, thres,r))
        {
            endRow = r;
            break;
        }
    }

    int startCol = 0;
    for (int c = 0; c < bw; c++)
    {
        if (!col_all_black((u8*)buf,bw,bh, thres,c))
        {
            startRow = c;
            break;
        }
    }
    int endCol = 0;
    for (int c = bw-1; c >= 0; c--)
    {
        if (!col_all_black((u8*)buf,bw,bh, thres,c))
        {
            endCol = c;
            break;
        }
    }

    int frstRow = 0; for (int r = 0;    r < bh; r++) { if (!row_all_black((u8*)buf,bw,bh, thres,r)) { frstRow = r; break; } }
    int lastRow = 0; for (int r = bh-1; r >= 0; r--) { if (!row_all_black((u8*)buf,bw,bh, thres,r)) { lastRow = r; break; } }
    int frstCol = 0; for (int c = 0;    c < bw; c++) { if (!col_all_black((u8*)buf,bw,bh, thres,c)) { frstCol = c; break; } }
    int lastCol = 0; for (int c = bw-1; c >= 0; c--) { if (!col_all_black((u8*)buf,bw,bh, thres,c)) { lastCol = c; break; } }

    // assert(frstRow == startRow);  // sanity check
    // assert(lastRow == endRow);
    // assert(frstCol == startCol);
    // assert(lastCol == endCol);

    if (lastRow-frstRow > 0 && lastCol-frstCol > 0)
        return {frstCol, frstRow, lastCol, lastRow};
    else
        return {0,0,bw,bh};
}



// todo: improve the speed and accuracy of this
// different sampling method? (a few frames at multiple locations?)
// faster seek somehow? auto-correct while movie runs? let user manually adjust?
RECT slow_sample_for_autocrop(RollingMovie *movie, int thres)
{
    double current_percent = movie->percent_elapsed();
    PRINT("\n\ncurrent_percent: %f\n\n", current_percent);

    u8 *src = movie->reel.vid_buffer;
    int bw = movie->reel.vid_width;
    int bh = movie->reel.vid_height;

    // todo: should check more than one spot and check consistency / proportion of screen / etc
    movie->hard_seek_to_timestamp(0.5 * movie->reel.durationSeconds);

    RECT crop = calc_autocroped_rect((u32*)src, bw, bh, thres);
    PRINT("AUTOCROP CALC RECT: %i, %i, %i, %i\n", crop.left, crop.top, crop.right, crop.bottom);

    movie->hard_seek_to_percent(current_percent);

    return crop;
}

RECT sample_current_frame_for_autocrop(RollingMovie *movie, int thres)
{
    u8 *src = movie->reel.vid_buffer;
    int bw = movie->reel.vid_width;
    int bh = movie->reel.vid_height;
    RECT crop = calc_autocroped_rect((u32*)src, bw, bh, thres);
    PRINT("AUTOCROP CALC RECT: %i, %i, %i, %i\n", crop.left, crop.top, crop.right, crop.bottom);
    return crop;
}




void debug_draw_live_autocrop_rect(u8 *src, int bw, int bh, frame_buffer *back_buffer, int autocrop_thres)
{
    RECT crop = calc_autocroped_rect((u32*)src, bw, bh, autocrop_thres);
    int dw = crop.right - crop.left;
    int dh = crop.bottom - crop.top;

    back_buffer->resize_if_needed(bw, bh);
    copy_subrect((u32*)back_buffer->mem,{0,0,bw,bh}, (u32*)src,bw,bh);

    draw_rect((u32*)back_buffer->mem,bw,bh, crop);
}
void debug_draw_cached_autocrop_rect(u8 *src, int bw, int bh, frame_buffer *back_buffer, RECT autocrop_rect)
{
    RECT crop = autocrop_rect;
    int dw = crop.right - crop.left;
    int dh = crop.bottom - crop.top;

    // PRINT("crop: %i, %i, %i, %i\n", crop.left, crop.top, crop.right, crop.bottom);

    back_buffer->resize_if_needed(bw, bh);
    copy_subrect((u32*)back_buffer->mem,{0,0,bw,bh}, (u32*)src,bw,bh);

    draw_rect((u32*)back_buffer->mem,bw,bh, crop);
}

