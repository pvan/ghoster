

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
        // Out.Position = (In.Position - float4(0.5,0.5,0,0)) * float4(2,-2,1,1); // to ndc
        Out.Position = In.Position;
        Out.Texture  = In.Texture;
        return Out;
    }
);

char *pixel_shader = MULTILINE_STRING
(
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
        Out.Color.a = 0.5;
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

void d3d_clear(int r = 0, int g = 0, int b = 0, int a = 255)
{
    if (device) device->Clear(0, 0, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_RGBA(r,g,b,a), 1.0f, 0);
}

struct textured_quad
{
    IDirect3DVertexBuffer9 *vb;
    IDirect3DTexture9 *tex;
    IDirect3DVertexDeclaration9 *vertexDecl;

    void destroy()
    {
        vb->Release();
        tex->Release();
        vertexDecl->Release();
        vb = 0;
        tex = 0;
        vertexDecl = 0;
    }

    void fill_tex_with_mem(u8 *mem, int w, int h)
    {
        D3DLOCKED_RECT rect;
        HRESULT res = tex->LockRect(0, &rect, 0, 0);
        if (res != D3D_OK) MessageBox(0,"error LockRect", 0, 0);
        memcpy(rect.pBits, mem, w*h*sizeof(u32));
        tex->UnlockRect(0);
    }

    void fill_vb_will_rect(float dl, float dt, float dr, float db)
    {
        float verts[] = {
        //   x  y   z   u  v
            dl, dt, 0,  0, 1,
            dl, db, 0,  0, 0,
            dr, dt, 0,  1, 1,
            dr, db, 0,  1, 0
        };

        void *where_to_copy_to;
        vb->Lock(0, 0, &where_to_copy_to, 0);
        memcpy(where_to_copy_to, verts, 5*4*sizeof(float));
        vb->Unlock();
    }

    void update(u8 *qmem, int qw, int qh, float dl = -1, float dt = -1, float dr = 1, float db = 1)
    {
        fill_tex_with_mem(qmem, qw, qh);
        fill_vb_will_rect(dl, dt, dr, db);
    }

    void create(u8 *qmem, int qw, int qh, float dl = -1, float dt = -1, float dr = 1, float db = 1)
    {
        if (!device) return;

        HRESULT res = device->CreateVertexBuffer(5*4*sizeof(float),
                                                 D3DUSAGE_DYNAMIC|D3DUSAGE_WRITEONLY,
                                                 0, D3DPOOL_DEFAULT, &vb, 0);
        if (res != D3D_OK) MessageBox(0,"error creating vb", 0, 0);

        fill_vb_will_rect(dl, dt, dr, db);


        // float verts[] = {
        // //   x  y   z   u  v
        //     dl, dt, 0,  0, 1,
        //     dl, db, 0,  0, 0,
        //     dr, dt, 0,  1, 1,
        //     dr, db, 0,  1, 0
        // };

        // void *where_to_copy_to;
        // vb->Lock(0, 0, &where_to_copy_to, 0);
        // memcpy(where_to_copy_to, verts, 5*4*sizeof(float));
        // vb->Unlock();


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


        res = device->CreateTexture(qw, qh, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &tex, 0);
        if (res != D3D_OK) MessageBox(0,"error CreateTexture", 0, 0);
        fill_tex_with_mem(qmem, qw, qh);


        device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
        device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
        // device->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);  // this is the default value

        // without this, linear filtering will give us a bad pixel row on top/left
        // does this actually fix the issue or is our whole image off a pixel and this just covers it up?
        device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP); // doesn't seem to help our
        device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP); // top/left bad pixel edge
    }

    void render()
    {
        device->BeginScene();

d3d_clear(255, 0, 0, 255);

        device->SetVertexDeclaration(vertexDecl);
        device->SetVertexShader(vs);
        device->SetPixelShader(ps);
        device->SetStreamSource(0, vb, 0, 5*sizeof(float));
        device->SetTexture(0, tex);
        device->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);

        device->EndScene();
    }
};


// IDirect3DVertexBuffer9 *vb;
// IDirect3DVertexBuffer9 *vb2;
// IDirect3DTexture9 *tex;
// IDirect3DVertexDeclaration9 *vertexDecl;


void d3d_render_pattern_to_texture(IDirect3DTexture9 *t, float dt)
{
    static int running_t = 0; running_t += dt;
    D3DLOCKED_RECT rect;
    HRESULT res = t->LockRect(0, &rect, 0, 0);
    if (res != D3D_OK) MessageBox(0,"error LockRect", 0, 0);
    for (int x = 0; x < 400; x++)
    {
        for (int y = 0; y < 400; y++)
        {
            byte *b = (byte*)rect.pBits + ((400*y)+x)*4 + 0;
            byte *g = (byte*)rect.pBits + ((400*y)+x)*4 + 1;
            byte *r = (byte*)rect.pBits + ((400*y)+x)*4 + 2;
            byte *a = (byte*)rect.pBits + ((400*y)+x)*4 + 3;

            *a = 255;
            *r = y*(255.0/400.0);
            *g = ((-cos(running_t*2*3.141592 / 3000) + 1) / 2) * 255.0;
            *b = x*(255.0/400.0);
        }
    }
    t->UnlockRect(0);
}

void d3d_render_mem_to_texture(IDirect3DTexture9 *t, u8* mem, int w, int h)
{
    D3DLOCKED_RECT rect;
    HRESULT res = t->LockRect(0, &rect, 0, 0);
    if (res != D3D_OK) MessageBox(0,"error LockRect", 0, 0);
    memcpy(rect.pBits, mem, w*h*sizeof(u32));
    t->UnlockRect(0);
}


// void d3d_create_quad(int w, int h)
// {
//     float verts[] = {
//     //   x   y  z   u  v
//         -1, -1, 0,  0, 1,
//         -1,  1, 0,  0, 0,
//          1, -1, 0,  1, 1,
//          1,  1, 0,  1, 0
//     };

//     HRESULT res = device->CreateVertexBuffer(5*4*sizeof(float),
//                                              D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, 0,
//                                              D3DPOOL_DEFAULT, &vb, 0);
//     if (res != D3D_OK) MessageBox(0,"error creating vb", 0, 0);

//     void *where_to_copy_to;
//     vb->Lock(0, 0, &where_to_copy_to, D3DLOCK_DISCARD);
//     memcpy(where_to_copy_to, verts, 5*4*sizeof(float));
//     vb->Unlock();


//     D3DVERTEXELEMENT9 decl[] =
//     {
//         {
//             0, 0,
//             D3DDECLTYPE_FLOAT3,
//             D3DDECLMETHOD_DEFAULT,
//             D3DDECLUSAGE_POSITION, 0
//         },
//         {
//             0, 3*sizeof(float),
//             D3DDECLTYPE_FLOAT2,
//             D3DDECLMETHOD_DEFAULT,
//             D3DDECLUSAGE_TEXCOORD, 0
//         },
//         D3DDECL_END()
//     };
//     res = device->CreateVertexDeclaration(decl, &vertexDecl);
//     if (res != D3D_OK) OutputDebugString("CreateVertexDeclaration error!\n");


//     // todo: or D3DPOOL_DEFAULT + D3DUSAGE_DYNAMIC & D3DUSAGE_WRITEONLY
//     // apparently D3DPOOL_MANAGED is needed to lock the texture?
//     res = device->CreateTexture(w, h, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &tex, 0);
//     if (res != D3D_OK) MessageBox(0,"error CreateTexture", 0, 0);
//     d3d_render_pattern_to_texture(tex, 0);


//     device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
//     device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
//     // device->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);  // this is the default value

//     // without this, linear filtering will give us a bad pixel row on top/left
//     // does this actually fix the issue or is our whole image off a pixel and this just covers it up?
//     device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP); // doesn't seem to help our
//     device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP); // top/left bad pixel edge


//     device->SetRenderState(D3DRS_ALPHABLENDENABLE, true);
//     device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD); // default
//     device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCCOLOR);
//     device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCCOLOR);
// }

// void d3d_update_quad(float l, float t, float r, float b, float z = 0)
// {
//     float verts[] = {
//     //  x  y  z   u  v
//         l, t, z,  0, 1,
//         l, b, z,  0, 0,
//         r, t, z,  1, 1,
//         r, b, z,  1, 0
//     };

//     void *where_to_copy_to;
//     vb->Lock(0, 0, &where_to_copy_to, D3DLOCK_DISCARD);
//     memcpy(where_to_copy_to, verts, 5 * 4 * sizeof(float));
// }


// void d3d_update_texture(IDirect3DTexture9 *t, int w, int h)
// {
//     if (tex) tex->Release();
//     HRESULT res = device->CreateTexture(w, h, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &tex, 0);
//     if (res != D3D_OK) OutputDebugString("error CreateTexture");
// }


bool d3d_init(HWND win, int w, int h)
{
    context = Direct3DCreate9(D3D_SDK_VERSION);
    if (!context) { MessageBox(0, "Error creating direct3D context", 0, 0); return false; }

    D3DPRESENT_PARAMETERS params = {0};
    params.BackBufferWidth = w;
    params.BackBufferWidth = h;
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


    // for not just put these here...

    d3d_compile_shaders();

    // d3d_create_quad(w, h);

    return true;
}


// void d3d_viewport(RECT rect)
// {
//     if (!device) return;

//     D3DVIEWPORT9 vp;
//     vp.X      = rect.left;
//     vp.Y      = rect.top;
//     vp.Width  = rect.right-rect.left;
//     vp.Height = rect.bottom-rect.top;
//     vp.MinZ   = 0.0f;
//     vp.MaxZ   = 1.0f;
//     device->SetViewport(&vp);
// }


// flicker, hmm
// void d3d_clear_rect(int r, int g, int b, int a, RECT dest)
// {
//     if (device) device->Clear(1, (D3DRECT*)&dest, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_RGBA(r,g,b,a), 1.0f, 0);

//     // if (!device) return;

//     // // cache
//     // D3DVIEWPORT9 orig;
//     // device->GetViewport(&orig);

//     // d3d_viewport(dest);
//     // d3d_clear(r, g, b, alpha);

//     // // restore
//     // device->SetViewport(&orig);
// }

// void d3d_render()
// {
//     // device->Clear(0, 0, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_XRGB(0,150,200), 1.0f, 0);
//     device->BeginScene();

//     device->SetVertexDeclaration(vertexDecl);
//     device->SetVertexShader(vs);
//     device->SetPixelShader(ps);
//     device->SetStreamSource(0, vb, 0, 5*sizeof(float));
//     device->SetTexture(0, tex);
//     device->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);


//     // d3d_update_quad(0, 0, 1, 1, 0);
//     device->SetStreamSource(0, vb2, 0, 5*sizeof(float));
//     device->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);

//                 // d3d_clear(100, 0, 0, 100);

//     device->EndScene();
//     // device->Present(0, 0, 0, 0);
// }

// void d3d_render_quad(u8* qmem, int qw, int qh, double dx, double dy, double dw, double dh)
// {

// }

void d3d_swap()
{
    if (device) device->Present(0, 0, 0, 0);
}

// void d3d_viewport(RECT rect)
// {
//     if (!device) return;

//     D3DVIEWPORT9 vp;
//     vp.X      = rect.left;
//     vp.Y      = rect.top;
//     vp.Width  = rect.right-rect.left;
//     vp.Height = rect.bottom-rect.top;
//     vp.MinZ   = 0.0f;
//     vp.MaxZ   = 1.0f;
//     device->SetViewport(&vp);
// }

void d3d_cleanup()
{
    if (device) device->Release();
    if (context) context->Release();
    if (vs) vs->Release();
    if (ps) ps->Release();
    // if (tex) tex->Release();
    // if (vertexDecl) vertexDecl->Release();
    // if (vb) vb->Release();
    // if (vb2) vb2->Release();
}