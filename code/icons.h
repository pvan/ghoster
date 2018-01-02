




static HBITMAP global_bitmap_w;
static HBITMAP global_bitmap_b;

static HBITMAP global_bitmap_c1;
static HBITMAP global_bitmap_c2;
static HBITMAP global_bitmap_c3;
static HBITMAP global_bitmap_c4;

static HBITMAP global_bitmap_p1;
static HBITMAP global_bitmap_p2;
static HBITMAP global_bitmap_p3;
static HBITMAP global_bitmap_p4;

static HBITMAP global_bitmap_r1;
static HBITMAP global_bitmap_r2;
static HBITMAP global_bitmap_r3;
static HBITMAP global_bitmap_r4;

static HBITMAP global_bitmap_y1;
static HBITMAP global_bitmap_y2;
static HBITMAP global_bitmap_y3;
static HBITMAP global_bitmap_y4;


// static HICON global_icon;
static HICON global_icon_w;
static HICON global_icon_b;

static HICON global_icon_c1;
static HICON global_icon_c2;
static HICON global_icon_c3;
static HICON global_icon_c4;

static HICON global_icon_p1;
static HICON global_icon_p2;
static HICON global_icon_p3;
static HICON global_icon_p4;

static HICON global_icon_r1;
static HICON global_icon_r2;
static HICON global_icon_r3;
static HICON global_icon_r4;

static HICON global_icon_y1;
static HICON global_icon_y2;
static HICON global_icon_y3;
static HICON global_icon_y4;




HICON MakeIconFromBitmap(HINSTANCE hInstance, HBITMAP hbm)
{
    ICONINFO info = {true, 0, 0, hbm, hbm};
    return CreateIconIndirect(&info);
}

void LoadIcons(HINSTANCE hInstance)
{
    // global_icon = (HICON)LoadImage(
    //     hInstance,
    //     MAKEINTRESOURCE(ID_ICON),
    //     IMAGE_ICON,
    //     0, 0, LR_DEFAULTSIZE);

    global_bitmap_w  = (HBITMAP)LoadImage(hInstance, MAKEINTRESOURCE(ID_ICON_W ), IMAGE_BITMAP, 0, 0, LR_DEFAULTSIZE);
    global_bitmap_b  = (HBITMAP)LoadImage(hInstance, MAKEINTRESOURCE(ID_ICON_B ), IMAGE_BITMAP, 0, 0, LR_DEFAULTSIZE);
    global_bitmap_c1 = (HBITMAP)LoadImage(hInstance, MAKEINTRESOURCE(ID_ICON_C1), IMAGE_BITMAP, 0, 0, LR_DEFAULTSIZE);
    global_bitmap_c2 = (HBITMAP)LoadImage(hInstance, MAKEINTRESOURCE(ID_ICON_C2), IMAGE_BITMAP, 0, 0, LR_DEFAULTSIZE);
    global_bitmap_c3 = (HBITMAP)LoadImage(hInstance, MAKEINTRESOURCE(ID_ICON_C3), IMAGE_BITMAP, 0, 0, LR_DEFAULTSIZE);
    global_bitmap_c4 = (HBITMAP)LoadImage(hInstance, MAKEINTRESOURCE(ID_ICON_C4), IMAGE_BITMAP, 0, 0, LR_DEFAULTSIZE);
    global_bitmap_p1 = (HBITMAP)LoadImage(hInstance, MAKEINTRESOURCE(ID_ICON_P1), IMAGE_BITMAP, 0, 0, LR_DEFAULTSIZE);
    global_bitmap_p2 = (HBITMAP)LoadImage(hInstance, MAKEINTRESOURCE(ID_ICON_P2), IMAGE_BITMAP, 0, 0, LR_DEFAULTSIZE);
    global_bitmap_p3 = (HBITMAP)LoadImage(hInstance, MAKEINTRESOURCE(ID_ICON_P3), IMAGE_BITMAP, 0, 0, LR_DEFAULTSIZE);
    global_bitmap_p4 = (HBITMAP)LoadImage(hInstance, MAKEINTRESOURCE(ID_ICON_P4), IMAGE_BITMAP, 0, 0, LR_DEFAULTSIZE);
    global_bitmap_r1 = (HBITMAP)LoadImage(hInstance, MAKEINTRESOURCE(ID_ICON_R1), IMAGE_BITMAP, 0, 0, LR_DEFAULTSIZE);
    global_bitmap_r2 = (HBITMAP)LoadImage(hInstance, MAKEINTRESOURCE(ID_ICON_R2), IMAGE_BITMAP, 0, 0, LR_DEFAULTSIZE);
    global_bitmap_r3 = (HBITMAP)LoadImage(hInstance, MAKEINTRESOURCE(ID_ICON_R3), IMAGE_BITMAP, 0, 0, LR_DEFAULTSIZE);
    global_bitmap_r4 = (HBITMAP)LoadImage(hInstance, MAKEINTRESOURCE(ID_ICON_R4), IMAGE_BITMAP, 0, 0, LR_DEFAULTSIZE);
    global_bitmap_y1 = (HBITMAP)LoadImage(hInstance, MAKEINTRESOURCE(ID_ICON_Y1), IMAGE_BITMAP, 0, 0, LR_DEFAULTSIZE);
    global_bitmap_y2 = (HBITMAP)LoadImage(hInstance, MAKEINTRESOURCE(ID_ICON_Y2), IMAGE_BITMAP, 0, 0, LR_DEFAULTSIZE);
    global_bitmap_y3 = (HBITMAP)LoadImage(hInstance, MAKEINTRESOURCE(ID_ICON_Y3), IMAGE_BITMAP, 0, 0, LR_DEFAULTSIZE);
    global_bitmap_y4 = (HBITMAP)LoadImage(hInstance, MAKEINTRESOURCE(ID_ICON_Y4), IMAGE_BITMAP, 0, 0, LR_DEFAULTSIZE);

    global_icon_w =  MakeIconFromBitmap(hInstance, global_bitmap_w );
    global_icon_b =  MakeIconFromBitmap(hInstance, global_bitmap_b );
    global_icon_c1 = MakeIconFromBitmap(hInstance, global_bitmap_c1);
    global_icon_c2 = MakeIconFromBitmap(hInstance, global_bitmap_c2);
    global_icon_c3 = MakeIconFromBitmap(hInstance, global_bitmap_c3);
    global_icon_c4 = MakeIconFromBitmap(hInstance, global_bitmap_c4);
    global_icon_p1 = MakeIconFromBitmap(hInstance, global_bitmap_p1);
    global_icon_p2 = MakeIconFromBitmap(hInstance, global_bitmap_p2);
    global_icon_p3 = MakeIconFromBitmap(hInstance, global_bitmap_p3);
    global_icon_p4 = MakeIconFromBitmap(hInstance, global_bitmap_p4);
    global_icon_r1 = MakeIconFromBitmap(hInstance, global_bitmap_r1);
    global_icon_r2 = MakeIconFromBitmap(hInstance, global_bitmap_r2);
    global_icon_r3 = MakeIconFromBitmap(hInstance, global_bitmap_r3);
    global_icon_r4 = MakeIconFromBitmap(hInstance, global_bitmap_r4);
    global_icon_y1 = MakeIconFromBitmap(hInstance, global_bitmap_y1);
    global_icon_y2 = MakeIconFromBitmap(hInstance, global_bitmap_y2);
    global_icon_y3 = MakeIconFromBitmap(hInstance, global_bitmap_y3);
    global_icon_y4 = MakeIconFromBitmap(hInstance, global_bitmap_y4);

}

HICON GetIconByInt(int i)
{
    if (i-- < 1) return global_icon_c1;
    if (i-- < 1) return global_icon_c2;
    if (i-- < 1) return global_icon_c3;
    if (i-- < 1) return global_icon_c4;

    if (i-- < 1) return global_icon_p1;
    if (i-- < 1) return global_icon_p2;
    if (i-- < 1) return global_icon_p3;
    if (i-- < 1) return global_icon_p4;

    if (i-- < 1) return global_icon_r1;
    if (i-- < 1) return global_icon_r2;
    if (i-- < 1) return global_icon_r3;
    if (i-- < 1) return global_icon_r4;

    if (i-- < 1) return global_icon_y1;
    if (i-- < 1) return global_icon_y2;
    if (i-- < 1) return global_icon_y3;
    if (i-- < 1) return global_icon_y4;

    return global_icon_b;
}


HICON RandomIcon()
{
    return GetIconByInt(rand_int(16));
}


