
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
#define GL_CLAMP_TO_EDGE                  0x812F

#define GLchar char

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
typedef void (APIENTRY * PFGL_DELVA) (GLsizei n, const GLuint *arrays);
typedef void (APIENTRY * PFGL_DELBUF) (GLsizei n, const GLuint * buffers);
typedef void (APIENTRY * PFGL_UNI4F) (GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
typedef void (APIENTRY * PFGL_UNIFV) (GLuint program, GLint location, GLfloat *params);
typedef void (APIENTRY * PFGL_DELPROG) (GLuint program);
typedef void (APIENTRY * PFGL_DELSHAD) (GLuint shader);
typedef void (APIENTRY * PFGL_BINDAL) (GLuint program, GLuint index, const GLchar *name);
typedef void (APIENTRY * PFGL_BINDFDL) (GLuint program, GLuint colorNumber, const char * name);
typedef void (APIENTRY * PFGL_DETS) (GLuint program, GLuint shader);
typedef void (APIENTRY * PFGL_UNI4FV) (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);


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
static PFGL_DELVA glDeleteVertexArrays;
static PFGL_DELBUF glDeleteBuffers;
static PFGL_UNI4F glUniform4f;
static PFGL_UNIFV glGetUniformfv;
static PFGL_DELPROG glDeleteProgram;
static PFGL_DELSHAD glDeleteShader;
static PFGL_BINDAL glBindAttribLocation;
static PFGL_BINDFDL glBindFragDataLocation;
static PFGL_DETS glDetachShader;
static PFGL_UNI4FV glUniformMatrix4fv;




#include "lib\gltext.h"



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
        char buf[1024];
        sprintf(buf, "\n\n%s log:\n%s\n\n", kind, log);
        LogMessage(buf);
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
        LogError(errbuf);
    }
}

// trick for easy shader strings
#define MULTILINE_STRING(...) #__VA_ARGS__


// setup once on init
static GLuint vao;
static GLuint vbo;
static GLuint shader_program;
static GLuint tex;
HGLRC rendering_context;
GLuint alpha_location;
GLuint tex_location;


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

    rendering_context = wglCreateContext(hdc);
    wglMakeCurrent(hdc, rendering_context); // map future gl calls to our hdc

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
    glDeleteVertexArrays = (PFGL_DELVA)wglGetProcAddress("glDeleteVertexArrays");
    glDeleteBuffers = (PFGL_DELBUF)wglGetProcAddress("glDeleteBuffers");
    glUniform4f=  (PFGL_UNI4F)wglGetProcAddress("glUniform4f");
    glGetUniformfv = (PFGL_UNIFV)wglGetProcAddress("glGetUniformfv");
    glDeleteProgram = (PFGL_DELPROG)wglGetProcAddress("glDeleteProgram");
    glDeleteShader = (PFGL_DELSHAD)wglGetProcAddress("glDeleteShader");
    glBindAttribLocation = (PFGL_BINDAL)wglGetProcAddress("glBindAttribLocation");
    glBindFragDataLocation = (PFGL_BINDFDL)wglGetProcAddress("glBindFragDataLocation");
    glDetachShader = (PFGL_DETS)wglGetProcAddress("glDetachShader");
    glUniformMatrix4fv = (PFGL_UNI4FV)wglGetProcAddress("glUniformMatrix4fv");


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
            // color.a = alpha;  \n
            color.a = alpha * color.a;  \n
            // color.a = alpha * color.a - alpha * color.a;  \n
        }
    );



    // for a while it seems like generating everything first worked best,
    // but we can only generate loc_position after compiling the shader
    // and it wouldn't matter as soon as we swapbuffer every frame anyway
    // so it's kind of a moot point now
    // my new theory on the laggy second window is it's some kind of nvidia bug
    // it doesn't happen on an intel card
    // and is similar to the "slow start" of other opengl apps
    // see https://stackoverflow.com/questions/43378891/multiple-instances-of-opengl-exe-take-longer-and-longer-to-initialize

    GLuint vshader = glCreateShader(GL_VERTEX_SHADER);
    GLuint fshader = glCreateShader(GL_FRAGMENT_SHADER);
    shader_program = glCreateProgram();

    glGenVertexArrays(1, &vao);
            check_gl_error("glGenVertexArrays");
    glGenBuffers(1, &vbo);
            check_gl_error("glGenBuffers");
    glGenTextures(1, &tex);


    // previously it seems like this first helped us, but too many things have changed since then
    glBindVertexArray(vao);


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

    // we need this after the shader is compiled,
    // but having it down here re-introduces our laggy window bug
    // (which is back once we swapbuffer every frame anyway)
    GLuint position_location = glGetAttribLocation(shader_program, "position");

    // vbo stuff
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    float points[2*4] = {-1,1, -1,-1, 1,-1, 1,1};
    glBufferData(GL_ARRAY_BUFFER, sizeof(points), points, GL_DYNAMIC_DRAW);

    // vao stuff
    glVertexAttribPointer(position_location, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(position_location);




    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);



    alpha_location = glGetUniformLocation(shader_program, "alpha");
    tex_location = glGetUniformLocation(shader_program, "tex");





    // Initialize glText
    if(!gltInit())
    {
        LogError("Failed to initialize glText\n");
        // glfwTerminate();
        // return EXIT_FAILURE;
    }

}



// // hdc for this frame.. maybe cache per or pass in?
static HDC hdcCurrent;
static HWND winCurrent; // need this for when we release hdc.. better way?
// static RECT winRectCurrent;
static int currentWid;
static int currentHei;

void RendererStartFrame(HWND window)
{
    hdcCurrent = GetDC(window);
    winCurrent = window;
    wglMakeCurrent(hdcCurrent, rendering_context); // map future gl calls to our hdc

    RECT winRect;
    GetWindowRect(window, &winRect);
    currentWid = winRect.right-winRect.left;
    currentHei = winRect.bottom-winRect.top;
}

void RendererClear(double r = 0, double g = 0, double b = 0, double a = 1)
{
    // glViewport(0, 0, currentWid, currentHei); // not sure if needed or wanted
    glClear(GL_COLOR_BUFFER_BIT);
    glClearColor(r, g, b, a);
}

void RenderQuadToRect(u8 *quadMem, int quadWid, int quadHei, double quadAlpha, RECT dest = {0})
{
    if (dest.left == 0 && dest.top == 0 && dest.right == 0 && dest.bottom == 0)
        glViewport(0, 0, currentWid, currentHei);
    else
        glViewport(dest.left, dest.top, dest.right-dest.left-0, dest.bottom-dest.top-0);

    glUseProgram(shader_program);
    glUniform1f(alpha_location, quadAlpha);
    glUniform1i(tex_location, 0);   // 0 = GL_TEXTURE0

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, quadWid, quadHei, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, quadMem);
    check_gl_error("glTexImage2D");

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBindVertexArray(vao);

    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

void RendererSwap(HWND window)
{
    SwapBuffers(hdcCurrent);
    ReleaseDC(winCurrent, hdcCurrent);
}

void RenderMsgOverlay(MessageOverlay overlay, int x, int y, int scale, int horizAlign, int vertAlign)
{
    // todo: add option for dest rect here like quadToRect?
    glViewport(0, 0, currentWid, currentHei);

    if (overlay.msLeftOfDisplay > 0)
    {
        GLTtext *text = gltCreateText();
        gltSetText(text, overlay.text.memory);

        gltColor(1.0f, 1.0f, 1.0f, overlay.alpha);
        gltDrawText2DAligned(text, x, y, scale, horizAlign, vertAlign);

        gltDeleteText(text);
    }
}

RECT RendererCalcLetterBoxRect(int dWID, int dHEI, double aspect_ratio)
{
    int calcWID = (int)((double)dHEI * aspect_ratio);
    int calcHEI = (int)((double)dWID / aspect_ratio);

    if (calcWID > dWID)  // letterbox
        calcWID = dWID;
    else
        calcHEI = dHEI;  // pillarbox

    int posX = ((double)dWID - (double)calcWID) / 2.0;
    int posY = ((double)dHEI - (double)calcHEI) / 2.0;

    return {posX, posY, calcWID, calcHEI};
}

