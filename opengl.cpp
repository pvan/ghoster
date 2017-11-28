
#include <gl/gl.h>
#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "gdi32.lib")



#define GL_READ_FRAMEBUFFER               0x8CA8
#define GL_COLOR_ATTACHMENT0              0x8CE0
#define GL_VERTEX_SHADER                  0x8B31
#define GL_FRAGMENT_SHADER                0x8B30
#define GL_ARRAY_BUFFER                   0x8892
#define GL_DYNAMIC_DRAW                   0x88E8
#define GL_STATIC_DRAW                    0x88E4
#define GL_COMPILE_STATUS                 0x8B81
#define GL_LINK_STATUS                    0x8B82
#define GL_TEXTURE0                       0x84C0

// or just dl glext.h
typedef void (APIENTRY * PFGL_GEN_FBO) (GLsizei n, GLuint *ids);
typedef void (APIENTRY * PFGL_BIND_FBO) (GLenum target, GLuint framebuffer);
typedef void (APIENTRY * PFGL_FBO_TEX2D) (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
typedef void (APIENTRY * PFGL_BLIT_FBO) (GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1,
                                         GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter);
typedef void (APIENTRY * PFGL_DEL_FBO) (GLsizei n, const GLuint * framebuffers);
typedef GLuint (APIENTRY * PFGL_CS) (GLenum shaderType);
typedef void (APIENTRY * PFGL_SS) (GLuint shader, GLsizei count, const char **string, const GLint *length);
typedef GLuint (APIENTRY * PFGL_CP) (void);
typedef void (APIENTRY * PFGL_AS) (GLuint program, GLuint shader);
typedef void (APIENTRY * PFGL_LP) (GLuint program);
typedef void (APIENTRY * PFGL_VA) (GLsizei n, GLuint *arrays);
typedef void (APIENTRY * PFGL_GB) (GLsizei n, GLuint *buffers);
typedef void (APIENTRY * PFGL_UP) (GLuint program);
typedef void (APIENTRY * PFGL_BB) (GLenum target, GLuint buffer);
typedef void (APIENTRY * PFGL_BA) (GLuint array);
typedef void (APIENTRY * PFGL_BD) (GLenum target, ptrdiff_t size, const GLvoid *data, GLenum usage);
typedef void (APIENTRY * PFGL_BS) (GLenum target, int* offset, ptrdiff_t size, const GLvoid * data);
typedef void (APIENTRY * PFGL_CMS) (GLuint shader);
typedef void (APIENTRY * PFGL_SL) (GLuint shader, GLsizei maxLength, GLsizei *length, char *infoLog);
typedef void (APIENTRY * PFGL_GS) (GLuint shader, GLenum pname, GLint *params);
typedef void (APIENTRY * PFGL_PL) (GLuint program, GLsizei maxLength, GLsizei *length, char *infoLog);
typedef void (APIENTRY * PFGL_GP) (GLuint shader, GLenum pname, GLint *params);
typedef GLint (APIENTRY * PFGL_GUL) (GLuint program, const char *name);
typedef GLint (APIENTRY * PFGL_GAL) (GLuint program, const char *name);
typedef void (APIENTRY * PFGL_VAP) (GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid * pointer);
typedef void (APIENTRY * PFGL_EVA) (GLuint index);
typedef void (APIENTRY * PFGL_UNI) (GLint location, GLint v0);
typedef void (APIENTRY * PFGL_UNIF) (GLint location, GLfloat v0);
typedef void (APIENTRY * PFGL_TEX) (GLenum texture);


static PFGL_GEN_FBO glGenFramebuffers;
static PFGL_BIND_FBO glBindFramebuffer;
static PFGL_FBO_TEX2D glFramebufferTexture2D;
static PFGL_BLIT_FBO glBlitFramebuffer;
static PFGL_DEL_FBO glDeleteFramebuffers;
static PFGL_CS glCreateShader;
static PFGL_SS glShaderSource;
static PFGL_CP glCreateProgram;
static PFGL_AS glAttachShader;
static PFGL_LP glLinkProgram;
static PFGL_VA glGenVertexArrays;
static PFGL_GB glGenBuffers;
static PFGL_UP glUseProgram;
static PFGL_BB glBindBuffer;
static PFGL_BA glBindVertexArray;
static PFGL_BD glBufferData;
static PFGL_BS glBufferSubData;
static PFGL_CMS glCompileShader;
static PFGL_SL glGetShaderInfoLog;
static PFGL_GS glGetShaderiv;
static PFGL_PL glGetProgramInfoLog;
static PFGL_GP glGetProgramiv;
static PFGL_GUL glGetUniformLocation;
static PFGL_GAL glGetAttribLocation;
static PFGL_VAP glVertexAttribPointer;
static PFGL_EVA glEnableVertexAttribArray;
static PFGL_UNI glUniform1i;
static PFGL_UNIF glUniform1f;
static PFGL_TEX glActiveTexture;




typedef void (*GetLogFunc)(GLuint, GLsizei, GLsizei *, char *);
typedef void (*GetParamFunc)(GLuint, GLenum, GLint *);
void shader_error_check(GLuint object, const char *kind, GetLogFunc getLog, GetParamFunc getParam, GLenum param)
{
    char log[1024];
    GLsizei length;
    getLog(object, 1024, &length, log);

    GLint status;
    getParam(object, param, &status);


    if (length || status == GL_FALSE)
    {
        if (length == 0)
        {
            sprintf(log, "No error log: forgot to compile?");
        }
        char err[1024];
        sprintf(err, "\n\n%s log:\n%s\n\n", kind, log);
        OutputDebugString(err);
    }


    if (status == GL_FALSE)
    {
        exit(1);
    }
}


void check_gl_error(char *lastCall)
{
    GLenum err = glGetError();
    if (err != GL_NO_ERROR)
    {
        char errbuf[123];
        sprintf(errbuf,"GL ERR: 0x%x (%s)\n", err, lastCall);
        OutputDebugString(errbuf);
    }
}

static GLuint vao;
static GLuint vbo;
static GLuint shader_program;
static GLuint tex;

// trick for easy shader strings
#define MULTILINE_STRING(...) #__VA_ARGS__


HGLRC gl_rendering_context;


// static HDC g_hdc;

void InitOpenGL(HWND window)
{
    HDC hdc = GetDC(window);

    PIXELFORMATDESCRIPTOR pixel_format = {};
    pixel_format.nSize = sizeof(pixel_format);
    pixel_format.nVersion = 1;
    pixel_format.dwFlags = PFD_SUPPORT_OPENGL | PFD_DRAW_TO_WINDOW | PFD_DOUBLEBUFFER;
    pixel_format.iPixelType = PFD_TYPE_RGBA;
    pixel_format.cColorBits = 32;
    pixel_format.cAlphaBits = 8;

    int format_index = ChoosePixelFormat(hdc, &pixel_format);
    SetPixelFormat(hdc, format_index, &pixel_format);

    gl_rendering_context = wglCreateContext(hdc);
    wglMakeCurrent(hdc, gl_rendering_context); // map future gl calls to our hdc

    ReleaseDC(window, hdc);


    // seem to be context dependent? so load after it?
    glGenFramebuffers = (PFGL_GEN_FBO)wglGetProcAddress("glGenFramebuffers");
    glBindFramebuffer = (PFGL_BIND_FBO)wglGetProcAddress("glBindFramebuffer");
    glFramebufferTexture2D = (PFGL_FBO_TEX2D)wglGetProcAddress("glFramebufferTexture2D");
    glBlitFramebuffer = (PFGL_BLIT_FBO)wglGetProcAddress("glBlitFramebuffer");
    glDeleteFramebuffers = (PFGL_DEL_FBO)wglGetProcAddress("glDeleteFramebuffers");
    glCreateShader = (PFGL_CS)wglGetProcAddress("glCreateShader");
    glShaderSource = (PFGL_SS)wglGetProcAddress("glShaderSource");
    glCreateProgram = (PFGL_CP)wglGetProcAddress("glCreateProgram");
    glAttachShader = (PFGL_AS)wglGetProcAddress("glAttachShader");
    glLinkProgram = (PFGL_LP)wglGetProcAddress("glLinkProgram");
    glGenVertexArrays = (PFGL_VA)wglGetProcAddress("glGenVertexArrays");
    glGenBuffers = (PFGL_GB)wglGetProcAddress("glGenBuffers");
    glUseProgram = (PFGL_UP)wglGetProcAddress("glUseProgram");
    glBindBuffer = (PFGL_BB)wglGetProcAddress("glBindBuffer");
    glBindVertexArray = (PFGL_BA)wglGetProcAddress("glBindVertexArray");
    glBufferData = (PFGL_BD)wglGetProcAddress("glBufferData");
    glBufferSubData = (PFGL_BS)wglGetProcAddress("glBufferSubData");
    glCompileShader = (PFGL_CMS)wglGetProcAddress("glCompileShader");
    glGetShaderInfoLog = (PFGL_SL)wglGetProcAddress("glGetShaderInfoLog");
    glGetShaderiv = (PFGL_GS)wglGetProcAddress("glGetShaderiv");
    glGetProgramInfoLog = (PFGL_PL)wglGetProcAddress("glGetProgramInfoLog");
    glGetProgramiv = (PFGL_GP)wglGetProcAddress("glGetProgramiv");
    glGetUniformLocation = (PFGL_GUL)wglGetProcAddress("glGetUniformLocation");
    glGetAttribLocation = (PFGL_GUL)wglGetProcAddress("glGetAttribLocation");
    glVertexAttribPointer = (PFGL_VAP)wglGetProcAddress("glVertexAttribPointer");
    glEnableVertexAttribArray = (PFGL_EVA)wglGetProcAddress("glEnableVertexAttribArray");
    glUniform1i = (PFGL_UNI)wglGetProcAddress("glUniform1i");
    glUniform1f = (PFGL_UNIF)wglGetProcAddress("glUniform1f");
    glActiveTexture = (PFGL_TEX)wglGetProcAddress("glActiveTexture");


    const char *vertex_shader = MULTILINE_STRING
    (
        #version 330 core \n
        layout(location = 0) in vec2 position;
        out vec2 texCoord;
        void main() {
            texCoord = position.xy*vec2(0.5,-0.5)+vec2(0.5,0.5);
            gl_Position = vec4(position, 0, 1);
        }
    );
    // OutputDebugString(vertex_shader);
    // OutputDebugString("\n");

    const char *fragment_shader = MULTILINE_STRING
    (
        #version 330 core \n
        out vec4 color;
        in vec2 texCoord;
        uniform sampler2D tex;
        uniform float alpha;
        void main()
        {
            color = texture2D(tex, texCoord);
            color.a = alpha;
        }
    );


    // some odd lag when launching a second exe
    // unless these are in a pretty particular order

    // seems like generating everything first works best
    GLuint vshader = glCreateShader(GL_VERTEX_SHADER);
    GLuint fshader = glCreateShader(GL_FRAGMENT_SHADER);
    shader_program = glCreateProgram();
    glGenVertexArrays(1, &vao);
            check_gl_error("glGenVertexArrays");
    glGenBuffers(1, &vbo);
            check_gl_error("glGenBuffers");
    glGenTextures(1, &tex);


    // then this first
    glBindVertexArray(vao);


    // the rest i'm not so sure how particular the order is
    glShaderSource(vshader, 1, &vertex_shader, 0);
    glShaderSource(fshader, 1, &fragment_shader, 0);
    glCompileShader(vshader);
    // shader_error_check(vshader, "vertex shader", glGetShaderInfoLog, glGetShaderiv, GL_COMPILE_STATUS);
    glCompileShader(fshader);
    // shader_error_check(fshader, "fragment shader", glGetShaderInfoLog, glGetShaderiv, GL_COMPILE_STATUS);

    // create program that sitches shaders together
    glAttachShader(shader_program, vshader);
    glAttachShader(shader_program, fshader);
    glLinkProgram(shader_program);
    // shader_error_check(shader_program, "program", glGetProgramInfoLog, glGetProgramiv, GL_LINK_STATUS);

    // we need this after the shader is compiled, but having it down here re-introduces our laggy window bug
    GLuint loc_position = glGetAttribLocation(shader_program, "position");

    // vbo stuff
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    float points[2*4] = {-1,1, -1,-1, 1,-1, 1,1};
    glBufferData(GL_ARRAY_BUFFER, sizeof(points), points, GL_DYNAMIC_DRAW); //GL_DYNAMIC_DRAW

    // vao stuff
    glVertexAttribPointer(loc_position, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(loc_position);




    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);



    // g_hdc = GetDC(window);

    // // pretty sure these could just cause more context switching
    // glBindVertexArray(0);
    // glBindBuffer(GL_ARRAY_BUFFER, 0);

    // wglMakeCurrent(NULL, NULL);

}




// TODO: pull out progress bar rendering from this function
// need to render to fbo to do so?
//dfdf
void RenderToScreenGL(void *memory, int sWID, int sHEI, int dWID, int dHEI, HWND window, bool letterbox, double aspect_ratio,
                      float proportion, bool drawProgressBar, bool drawBuffering)
{
    HDC hdc = GetDC(window);


    wglMakeCurrent(hdc, gl_rendering_context); // map future gl calls to our hdc


    // if window size changed.. could also call in WM_SIZE and not pass dWID here
    // or get  dWID dHEI from destination window?
    glViewport(0, 0, dWID, dHEI);

    glUseProgram(shader_program);

    if (drawBuffering)
    {
        static float t = 0;
        t++;
        // float col = sin(t*M_PI*2 / 100);
        // col = (col + 1) / 2; // 0-1
        // col = 0.9*col + 0.4*(1-col); //lerp

        // e^sin(x) very interesting shape, via
        // http://sean.voisen.org/blog/2011/10/breathing-led-with-arduino/
        float col = pow(M_E, sin(t*M_PI*2 / 100));
        float min = 1/M_E;
        float max = M_E;
        col = (col-min) / (max-min); // 0-1
        col = 0.75*col + 0.2*(1-col); //lerp

        glClear(GL_COLOR_BUFFER_BIT);
        glClearColor(col, col, col, 0);  // r g b a  looks like
    }
    else
    {

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tex);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, sWID, sHEI,
                     0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, memory);
            check_gl_error("glTexImage2D");

        GLuint tex_loc = glGetUniformLocation(shader_program, "tex");
        glUniform1i(tex_loc, 0);   // texture id of 0

        GLuint alpha_loc = glGetUniformLocation(shader_program, "alpha");
        glUniform1f(alpha_loc, 1);


        // todo: is it better to change the vbo or the viewport? maybe doesn't matter?
        if (letterbox)
        {
            int calcWID = (int)((double)dHEI * aspect_ratio);
            int calcHEI = (int)((double)dWID / aspect_ratio);

            if (calcWID > dWID)  // letterbox
                calcWID = dWID;
            else
                calcHEI = dHEI;  // pillarbox


            int posX = ((double)dWID - (double)calcWID) / 2.0;
            int posY = ((double)dHEI - (double)calcHEI) / 2.0;

            glViewport(posX, posY, calcWID, calcHEI);
        }


        glBindBuffer(GL_ARRAY_BUFFER, vbo);

        glClear(GL_COLOR_BUFFER_BIT);
        glClearColor(0, 0, 0, 0);  // r g b a  looks like

        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);


        if (drawProgressBar)
        {
            // todo? mimic youtube size adjustment?? (looks funny full screen.. just go back to drawing onto source???)
            // fakey way to draw rects
            int pos = (int)(proportion * (double)dWID);

            glViewport(pos, PROGRESS_BAR_B, dWID, PROGRESS_BAR_H);
            glUniform1f(alpha_loc, 0.4);
            u32 gray = 0xaaaaaaaa;
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, &gray);
            glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

            glViewport(0, PROGRESS_BAR_B, pos, PROGRESS_BAR_H);
            glUniform1f(alpha_loc, 0.6);
            u32 red = 0xffff0000;
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, &red);
            glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        }

        // i think this will only cause unnecessary context switching
        // glBindBuffer(GL_ARRAY_BUFFER, 0);
        // glBindVertexArray(0);

    }

    SwapBuffers(hdc);

    // wglMakeCurrent(NULL, NULL);

    ReleaseDC(window, hdc);
}




//



GLuint tex_FF;

void InitOpenGL_FF(HWND window)
{
    HDC hdc = GetDC(window);

    PIXELFORMATDESCRIPTOR pixel_format = {};
    pixel_format.nSize = sizeof(pixel_format);
    pixel_format.nVersion = 1;
    pixel_format.dwFlags = PFD_SUPPORT_OPENGL | PFD_DRAW_TO_WINDOW | PFD_DOUBLEBUFFER;
    pixel_format.iPixelType = PFD_TYPE_RGBA;
    pixel_format.cColorBits = 32;
    pixel_format.cAlphaBits = 8;

    int format_index = ChoosePixelFormat(hdc, &pixel_format);
    SetPixelFormat(hdc, format_index, &pixel_format);

    HGLRC gl_rendering_context = wglCreateContext(hdc);
    wglMakeCurrent(hdc, gl_rendering_context); // map future gl calls to our hdc


    glGenTextures(1, &tex_FF); // not actually needed?


    ReleaseDC(window, hdc);
}

void RenderToScreen_FF(void *memory, int width, int height, HWND window)
{

    HDC deviceContext = GetDC(window);


    glViewport(0,0, width, height);


    glBindTexture(GL_TEXTURE_2D, tex_FF);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width, height, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, memory);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glEnable(GL_TEXTURE_2D);


    glClearColor(0.5f, 0.8f, 1.0f, 0.0f);
    // glClearColor(1.0f, 0.0f, 1.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);  // some offscreen buffer



    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();


    glBegin(GL_TRIANGLES);


    // note the texture coords are upside down
    // to get our texture right side up
    glTexCoord2f(0, 1); glVertex2f(-1, -1);
    glTexCoord2f(0, 0); glVertex2f(-1, 1);
    glTexCoord2f(1, 1); glVertex2f(1, -1);

    glTexCoord2f(0, 0); glVertex2f(-1, 1);
    glTexCoord2f(1, 0); glVertex2f(1, 1);
    glTexCoord2f(1, 1); glVertex2f(1, -1);
    glEnd();


    SwapBuffers(deviceContext);

    ReleaseDC(window, deviceContext);

}



//




void Render_GDI(void *memory, int width, int height, HWND window)
{
    HDC hdc = GetDC(window);

    RECT clientRect;
    GetClientRect(window, &clientRect);
    int winWID = clientRect.right - clientRect.left;
    int winHEI = clientRect.bottom - clientRect.top;

    // // here we clear out the edges
    // PatBlt(hdc, width, 0, winWID - width, winHEI, BLACKNESS);
    // PatBlt(hdc, 0, height, width, winHEI - height, BLACKNESS);


    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFO);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height;  // negative to set origin in top left
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;


    SetStretchBltMode(hdc, HALFTONE);

    int result = StretchDIBits(
                  hdc,
                  0,
                  0,
                  winWID,
                  winHEI,
                  0,
                  0,
                  width,
                  height,
                  memory,
                  &bmi,
                  DIB_RGB_COLORS,
                  SRCCOPY);

    if (!result)
    {
        // todo: seems to error here once at vid start, no memory yet?
        // MsgBox("error with StretchDIBits");
        // GetLastError();
    }
}


void RenderProgressBar_GDI(void *memory, int width, int height, HWND window)
{

    HDC hdc = GetDC(window);

    RECT clientRect;
    GetClientRect(window, &clientRect);
    int winWID = clientRect.right - clientRect.left;
    int winHEI = clientRect.bottom - clientRect.top;

    // // here we clear out the edges
    // PatBlt(hdc, width, 0, winWID - width, winHEI, BLACKNESS);
    // PatBlt(hdc, 0, height, width, winHEI - height, BLACKNESS);


    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFO);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height;  // negative to set origin in top left
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;


    SetStretchBltMode(hdc, HALFTONE);

    int result = StretchDIBits(
                  hdc,
                  0,
                  0,
                  winWID,
                  winHEI,
                  0,
                  0,
                  width,
                  height,
                  memory,
                  &bmi,
                  DIB_RGB_COLORS,
                  SRCCOPY);

    if (!result)
    {
        // todo: seems to error here once at vid start, no memory yet?
        // MsgBox("error with StretchDIBits");
        // GetLastError();
    }
}