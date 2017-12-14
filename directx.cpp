

#include <d3d9.h>
#pragma comment(lib, "d3d9.lib")

// for compiling shaders
// could pre-compile shaders to avoid the need for this?
#include <D3Dcompiler.h>
// #pragma comment(lib, "D3dcompiler_47.lib") // load a dll instead



// hoops to avoid installing the sdk
// basically to get D3DCompile

// typedef struct {
//     LPCSTR Name;
//     LPCSTR Definition;
// } D3D_SHADER_MACRO;

DEFINE_GUID(IID_ID3D10Blob, 0x8ba5fb08, 0x5195, 0x40e2, 0xac, 0x58, 0xd, 0x98, 0x9c, 0x3a, 0x1, 0x2);

typedef struct ID3D10BlobVtbl {
    BEGIN_INTERFACE

    HRESULT ( STDMETHODCALLTYPE *QueryInterface )
            (ID3D10Blob * This, REFIID riid, void **ppvObject);

    ULONG   ( STDMETHODCALLTYPE *AddRef )(ID3D10Blob * This);
    ULONG   ( STDMETHODCALLTYPE *Release )(ID3D10Blob * This);
    LPVOID  ( STDMETHODCALLTYPE *GetBufferPointer )(ID3D10Blob * This);
    SIZE_T  ( STDMETHODCALLTYPE *GetBufferSize )(ID3D10Blob * This);

    END_INTERFACE
} ID3D10BlobVtbl;

#define ID3D10Blob_QueryInterface(This,riid,ppvObject) \
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) )

#define ID3D10Blob_AddRef(This) \
    ( (This)->lpVtbl -> AddRef(This) )

#define ID3D10Blob_Release(This) \
    ( (This)->lpVtbl -> Release(This) )

#define ID3D10Blob_GetBufferPointer(This) \
    ( (This)->lpVtbl -> GetBufferPointer(This) )

#define ID3D10Blob_GetBufferSize(This) \
    ( (This)->lpVtbl -> GetBufferSize(This) )

// interface ID3D10Blob
// {
//     CONST_VTBL struct ID3D10BlobVtbl *lpVtbl;
// };

typedef ID3D10Blob ID3DBlob;

typedef HRESULT (WINAPI *COMPILE_FUNC)
    (LPCVOID                         pSrcData,
     SIZE_T                          SrcDataSize,
     LPCSTR                          pFileName,
     CONST D3D_SHADER_MACRO*         pDefines,
     void* /*ID3DInclude* */         pInclude,
     LPCSTR                          pEntrypoint,
     LPCSTR                          pTarget,
     UINT                            Flags1,
     UINT                            Flags2,
     ID3DBlob**                      ppCode,
     ID3DBlob**                      ppErrorMsgs);

COMPILE_FUNC myD3DCompile;



#define MULTILINE_STRING(...) #__VA_ARGS__

char *vertex_shader = MULTILINE_STRING
(
    struct VIN
    {
        float4 Position   : POSITION;
        float2 Texture    : TEXCOORD0;
    };
    struct VOUT
    {
        float4 Position   : POSITION;
        float2 Texture    : TEXCOORD0;
    };
    VOUT vs_main(in VIN In)
    {
        VOUT Out;
        Out.Position = In.Position;
        Out.Texture  = In.Texture;
        return Out;
    }
);

char *pixel_shader = MULTILINE_STRING
(
    float alpha : register(c0);
    struct PIN
    {
        float4 Position   : POSITION;
        float2 Texture    : TEXCOORD0;
    };
    struct POUT
    {
        float4 Color   : COLOR0;
    };
    sampler2D Tex0;
    POUT ps_main(in PIN In)
    {
        POUT Out;
        Out.Color = tex2D(Tex0, In.Texture);
        // Out.Color *= float4(0.9f, 0.8f, 0.4, 1);
        Out.Color.a = alpha * Out.Color.a;  // keep any alpha from texture?
        return Out;
    }
);


bool d3d_load()
{
    HINSTANCE dll = LoadLibrary("D3dcompiler_47.dll");
    if (!dll) { MessageBox(0, "Unable to load dll", 0, 0); return false; }

    myD3DCompile = (COMPILE_FUNC)GetProcAddress(dll, "D3DCompile");

    return true;
}


IDirect3D9* context;
IDirect3DDevice9* device;

IDirect3DVertexShader9 *vs;
IDirect3DPixelShader9 *ps;

void d3d_compile_shaders()
{
    ID3DBlob *code;
    ID3DBlob *errors;

    myD3DCompile(vertex_shader, strlen(vertex_shader), 0, 0, 0, "vs_main", "vs_1_1", 0, 0, &code, &errors);
    if (errors) {
        OutputDebugString("\nvs errors\n");
        OutputDebugString((char*)errors->GetBufferPointer());
        OutputDebugString("\n");
    }
    device->CreateVertexShader((DWORD*)code->GetBufferPointer(), &vs);

    myD3DCompile(pixel_shader, strlen(pixel_shader), 0, 0, 0, "ps_main", "ps_2_0", 0, 0, &code, &errors);
    if (errors) {
        OutputDebugString("\nps errors\n");
        OutputDebugString((char*)errors->GetBufferPointer());
        OutputDebugString("\n");
    }
    device->CreatePixelShader((DWORD*)code->GetBufferPointer(), &ps);
}

void enable_alpha_blending()
{
    device->SetRenderState(D3DRS_ALPHABLENDENABLE, true);
    // device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD); // default
    device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
}



IDirect3DSurface9   *cached_bb;
IDirect3DSurface9   *cached_db;
// IDirect3DSwapChain9 *cached_sc;
IDirect3DSurface9   *our_bb;
IDirect3DSurface9   *our_db;
IDirect3DSwapChain9 *our_sc;
void d3d_create_swapchain_and_depthbuffer(int w, int h)
{
    if (device)
    {
        // store our defaults
        device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &cached_bb);
        device->GetDepthStencilSurface(&cached_db);

        // get our current pp (better way?)
        D3DPRESENT_PARAMETERS pp;
        IDirect3DSwapChain9 *temp_sc;
        device->GetSwapChain(0, &temp_sc);
        temp_sc->GetPresentParameters(&pp);
        pp.BackBufferWidth = w;
        pp.BackBufferHeight = h;
        temp_sc->Release();

        // create new ones of the right size
        device->CreateDepthStencilSurface(
            w, h,
            pp.BackBufferFormat,
            pp.MultiSampleType,
            pp.MultiSampleQuality,
            pp.Flags & D3DPRESENTFLAG_DISCARD_DEPTHSTENCIL,
            &our_db, 0);
        HRESULT res = device->CreateAdditionalSwapChain(&pp, &our_sc);
        // if (res == D3DERR_NOTAVAILABLE) MsgBox("D3DERR_NOTAVAILABLE");
        // if (res == D3DERR_DEVICELOST) MsgBox("D3DERR_DEVICELOST");
        // if (res == D3DERR_INVALIDCALL) MsgBox("D3DERR_INVALIDCALL");
        // if (res == D3DERR_OUTOFVIDEOMEMORY) MsgBox("D3DERR_OUTOFVIDEOMEMORY");
        // if (res == E_OUTOFMEMORY) MsgBox("E_OUTOFMEMORY");
        // if (res == E_INVALIDARG) MsgBox("E_INVALIDARG");
        // if (res == D3D_OK) MsgBox("D3D_OK");
        our_sc->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &our_bb);

        device->SetRenderTarget(0, our_bb);
        device->SetDepthStencilSurface(our_db);

        // D3DVIEWPORT9 vp = {0};
        // vp.Width = w;
        // vp.Height = h;
        // device->SetViewport(&vp);
    }
}
void d3d_destroy_swapchain_and_depthbuffer()
{
    if (device)
    {
        // swap back to our cached buffers
        device->SetRenderTarget(0, cached_bb);
        device->SetDepthStencilSurface(cached_db);

        // now we can release these handles
        if (cached_bb) cached_bb->Release();
        if (cached_db) cached_db->Release();

        // release our current buffers
        if (our_bb) our_bb->Release();
        if (our_db) our_db->Release();

        // now our device should be in its initial state and we can re-create our buffers
    }
}
void d3d_resize(int w, int h)
{
    if (device)
    {
        d3d_destroy_swapchain_and_depthbuffer();
        d3d_create_swapchain_and_depthbuffer(w, h);
    }
}



bool d3d_init(HWND win, int w, int h)
{
    context = Direct3DCreate9(D3D_SDK_VERSION);
    if (!context) { MessageBox(0, "Error creating direct3D context", 0, 0); return false; }

    D3DPRESENT_PARAMETERS params = {0};
    params.BackBufferWidth = 10;  // we never use this buffer for rendering so we can keep it small
    params.BackBufferHeight = 10; // instead we swap in our own bb so we can more easily change the size
    params.BackBufferFormat = D3DFMT_A8R8G8B8;
    params.BackBufferCount = 1;
    params.MultiSampleType = D3DMULTISAMPLE_NONE;
    params.MultiSampleQuality = 0;
    params.SwapEffect = D3DSWAPEFFECT_DISCARD; //D3DSWAPEFFECT_COPY ?
    params.hDeviceWindow = win;
    params.Windowed = true;
    params.EnableAutoDepthStencil = true;
    params.AutoDepthStencilFormat = D3DFMT_D16;
    params.Flags = 0;
    params.FullScreen_RefreshRateInHz = 0; // must be 0 for windowed
    params.PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;

    context->CreateDevice(
        D3DADAPTER_DEFAULT,
        D3DDEVTYPE_HAL,
        win,
        D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED, // supposedly multithread degrades perf
        &params,
        &device);


    // for now just put these here...

    d3d_compile_shaders();

    enable_alpha_blending();


    d3d_create_swapchain_and_depthbuffer(w, h);


    return true;
}


void d3d_clear(int r = 0, int g = 0, int b = 0, int a = 255)
{
    if (device) device->Clear(0, 0, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_RGBA(r,g,b,a), 1.0f, 0);
}

// todo: move to util or renderer or somewhere?
float px2ndc(int pixel, int size)
{
    return ((float)pixel / (float)size)*2.0f - 1.0f;
}

struct d3d_textured_quad
{
    IDirect3DVertexBuffer9 *vb;
    IDirect3DTexture9 *tex;
    IDirect3DVertexDeclaration9 *vertexDecl;
    bool created;
    int texW;
    int texH;

    void destroy()
    {
        if (vb) vb->Release();
        if (tex) tex->Release();
        if (vertexDecl) vertexDecl->Release();
        vb = 0;
        tex = 0;
        vertexDecl = 0;
        created = false;
        texW = 0;
        texH = 0;
    }

    void create_tex(int w, int h)
    {
        HRESULT res = device->CreateTexture(w, h, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &tex, 0);
        if (res != D3D_OK) MessageBox(0,"error CreateTexture", 0, 0);
        texW = w;
        texH = h;
    }

    void fill_tex_with_mem(u8 *mem, int w, int h)
    {
        if (w != texW || h != texH)
        {
            create_tex(w, h);
        }
        D3DLOCKED_RECT rect;
        HRESULT res = tex->LockRect(0, &rect, 0, 0);
        if (res != D3D_OK) MessageBox(0,"error LockRect", 0, 0);
        memcpy(rect.pBits, mem, w*h*sizeof(u32));
        tex->UnlockRect(0);
    }

    void fill_vb_with_rect(float dl, float dt, float dr, float db, float z)
    {
        // i think z=0 is top, z=1 is bottom
        float verts[] = {
        //   x  y   z   u  v
            dl, dt, z,  0, 1,
            dl, db, z,  0, 0,
            dr, dt, z,  1, 1,
            dr, db, z,  1, 0
        };

        void *where_to_copy_to;
        vb->Lock(0, 0, &where_to_copy_to, 0);
        memcpy(where_to_copy_to, verts, 5*4*sizeof(float));
        vb->Unlock();
    }
    void update_custom_verts(float *verts)
    {
        void *where_to_copy_to;
        vb->Lock(0, 0, &where_to_copy_to, 0);
        memcpy(where_to_copy_to, verts, 5*4*sizeof(float));
        vb->Unlock();
    }
    void move_to_pixel_coords(int qx, int qy, int qw, int qh, int sw, int sh)
    {
        if (!created) return;

        float verts[] = {
        //  x                      y            z   u  v
            px2ndc(qx   ,sw), px2ndc(qy   ,sh), 0,  0, 1,
            px2ndc(qx   ,sw), px2ndc(qy+qh,sh), 0,  0, 0,
            px2ndc(qx+qw,sw), px2ndc(qy   ,sh), 0,  1, 1,
            px2ndc(qx+qw,sw), px2ndc(qy+qh,sh), 0,  1, 0,
        };
        update_custom_verts(verts);
    }
    void move_to_pixel_coords(int qx, int qy, int sw, int sh) { move_to_pixel_coords(qx, qy, texW, texH, sw, sh); }
    void move_to_pixel_coords_TL(int qx, int qy, int sw, int sh) { move_to_pixel_coords(qx, sh-texH-qy, texW, texH, sw, sh); }
    void move_to_pixel_coords_center(int qx, int qy, int sw, int sh) { move_to_pixel_coords(qx-texW/2, qy-texH/2, texW, texH, sw, sh); }


    void create(u8 *qmem, int qw, int qh, float dl, float dt, float dr, float db, float z)
    {
        if (!device) return;


        HRESULT res = device->CreateVertexBuffer(5*4*sizeof(float),
                                                 D3DUSAGE_DYNAMIC|D3DUSAGE_WRITEONLY,
                                                 0, D3DPOOL_DEFAULT, &vb, 0);
        if (res != D3D_OK) MessageBox(0,"error creating vb", 0, 0);

        fill_vb_with_rect(dl, dt, dr, db, z);


        D3DVERTEXELEMENT9 decl[] =
        {
            {
                0, 0,
                D3DDECLTYPE_FLOAT3,
                D3DDECLMETHOD_DEFAULT,
                D3DDECLUSAGE_POSITION, 0
            },
            {
                0, 3*sizeof(float),
                D3DDECLTYPE_FLOAT2,
                D3DDECLMETHOD_DEFAULT,
                D3DDECLUSAGE_TEXCOORD, 0
            },
            D3DDECL_END()
        };
        res = device->CreateVertexDeclaration(decl, &vertexDecl);
        if (res != D3D_OK) OutputDebugString("CreateVertexDeclaration error!\n");


        create_tex(qw, qh);
        fill_tex_with_mem(qmem, qw, qh);


        device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
        device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
        // device->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);  // this is the default value

        // without this, linear filtering will give us a bad pixel row on top/left
        // does this actually fix the issue or is our whole image off a pixel and this just covers it up?
        device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP); // doesn't seem to help our
        device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP); // top/left bad pixel edge

        created = true;
    }

    void update(u8 *qmem, int qw, int qh, float dl = -1, float dt = -1, float dr = 1, float db = 1, float z = 1)
    {
        if (!created) create(qmem,qw,qh, dl,dt,dr,db, z);

        fill_tex_with_mem(qmem, qw, qh);
        fill_vb_with_rect(dl, dt, dr, db, z);
    }

    void render(float alpha = 1)
    {
        device->BeginScene();

        device->SetPixelShaderConstantF(0, &alpha, 1);

        device->SetVertexDeclaration(vertexDecl);
        device->SetVertexShader(vs);
        device->SetPixelShader(ps);
        device->SetStreamSource(0, vb, 0, 5*sizeof(float));
        device->SetTexture(0, tex);
        device->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);

        device->EndScene();
    }
};


void d3d_swap()
{
    if (our_sc) our_sc->Present(0, 0, 0, 0, 0);
    // if (device) device->Present(0, 0, 0, 0);
}


void d3d_cleanup()
{
    if (device) device->Release();
    if (context) context->Release();
    if (vs) vs->Release();
    if (ps) ps->Release();
}