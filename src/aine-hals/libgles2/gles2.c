/*
 * aine-hals/libgles2/gles2.c — OpenGL ES 2.0 stub for AINE (macOS ARM64)
 *
 * Current state: functional stubs that return GL_NO_ERROR and basic
 * introspection strings.  Rendering calls are recorded but not dispatched
 * to Metal yet (F7 follow-up: attach to MTLRenderCommandEncoder).
 *
 * The dylib is loaded by aine-loader when an Android app opens libGLESv2.so.
 */

#include "GLES2/gl2.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* -------------------------------------------------------------------------
 * Error state (per-thread would be ideal; for now a simple global suffices
 * since aine-dalvik is single-threaded in F7)
 * ---------------------------------------------------------------------- */
static GLenum s_error = GL_NO_ERROR;

static void record_error(GLenum e) {
    if (s_error == GL_NO_ERROR) s_error = e;
}

GLenum glGetError(void)
{
    GLenum e = s_error;
    s_error = GL_NO_ERROR;
    return e;
}

/* -------------------------------------------------------------------------
 * Object name allocator (simple monotonically-increasing counter)
 * ---------------------------------------------------------------------- */
static GLuint s_next_id = 1;
static GLuint new_id(void) { return s_next_id++; }

/* -------------------------------------------------------------------------
 * String queries
 * ---------------------------------------------------------------------- */
const GLubyte *glGetString(GLenum name)
{
    switch (name) {
        case GL_VENDOR:                   return (const GLubyte *)"AINE Project";
        case GL_RENDERER:                 return (const GLubyte *)"AINE/Metal";
        case GL_VERSION:                  return (const GLubyte *)"OpenGL ES 2.0 AINE";
        case GL_SHADING_LANGUAGE_VERSION: return (const GLubyte *)"OpenGL ES GLSL ES 1.00 AINE";
        case GL_EXTENSIONS:               return (const GLubyte *)"";
        default:
            record_error(GL_INVALID_ENUM);
            return NULL;
    }
}

void glGetIntegerv(GLenum pname, GLint *params)
{
    if (!params) return;
    switch (pname) {
        case GL_MAX_TEXTURE_SIZE:    *params = 4096; break;
        case GL_MAX_VERTEX_ATTRIBS:  *params = 16;   break;
        case GL_VIEWPORT:
            params[0] = 0; params[1] = 0;
            params[2] = 1920; params[3] = 1080; /* default viewport */
            break;
        default:
            *params = 0;
            break;
    }
}

void glGetFloatv(GLenum pname, GLfloat *params) { (void)pname; if (params) *params = 0.0f; }
void glGetBooleanv(GLenum pname, GLboolean *params) { (void)pname; if (params) *params = GL_FALSE; }

/* -------------------------------------------------------------------------
 * Shader / Program objects
 * (stubs: compilation + linking always succeed with empty log)
 * ---------------------------------------------------------------------- */
GLuint glCreateShader(GLenum type)
{
    (void)type;
    return new_id();
}

GLuint glCreateProgram(void)
{
    return new_id();
}

void glShaderSource(GLuint shader, GLsizei count, const GLchar *const *string, const GLint *length)
{
    (void)shader; (void)count; (void)string; (void)length;
}

void glCompileShader(GLuint shader) { (void)shader; }

void glGetShaderiv(GLuint shader, GLenum pname, GLint *params)
{
    (void)shader;
    if (!params) return;
    switch (pname) {
        case GL_COMPILE_STATUS:  *params = GL_TRUE; break;
        case GL_INFO_LOG_LENGTH: *params = 0;       break;
        default: *params = 0; break;
    }
}

void glGetShaderInfoLog(GLuint shader, GLsizei bufsize, GLsizei *length, GLchar *infolog)
{
    (void)shader; (void)bufsize;
    if (length)  *length  = 0;
    if (infolog) *infolog = '\0';
}

void glAttachShader(GLuint program, GLuint shader) { (void)program; (void)shader; }
void glDetachShader(GLuint program, GLuint shader) { (void)program; (void)shader; }

void glLinkProgram(GLuint program) { (void)program; }

void glGetProgramiv(GLuint program, GLenum pname, GLint *params)
{
    (void)program;
    if (!params) return;
    switch (pname) {
        case GL_LINK_STATUS:     *params = GL_TRUE; break;
        case GL_INFO_LOG_LENGTH: *params = 0;       break;
        default: *params = 0; break;
    }
}

void glGetProgramInfoLog(GLuint program, GLsizei bufsize, GLsizei *length, GLchar *infolog)
{
    (void)program; (void)bufsize;
    if (length)  *length  = 0;
    if (infolog) *infolog = '\0';
}

void glUseProgram(GLuint program)    { (void)program; }
void glDeleteShader(GLuint shader)   { (void)shader;  }
void glDeleteProgram(GLuint program) { (void)program; }
void glValidateProgram(GLuint p)     { (void)p; }
void glReleaseShaderCompiler(void)   {}

void glBindAttribLocation(GLuint program, GLuint index, const GLchar *name) { (void)program; (void)index; (void)name; }

GLint glGetAttribLocation(GLuint program, const GLchar *name)
{
    (void)program; (void)name;
    return 0; /* All attribs map to location 0 in stub */
}

GLint glGetUniformLocation(GLuint program, const GLchar *name)
{
    (void)program; (void)name;
    return 0;
}

void glGetActiveAttrib(GLuint p, GLuint i, GLsizei bs, GLsizei *l, GLint *sz, GLenum *t, GLchar *n) {
    (void)p;(void)i;(void)bs; if(l)*l=0; if(sz)*sz=1; if(t)*t=GL_FLOAT; if(n&&bs>0)*n=0; }
void glGetActiveUniform(GLuint p, GLuint i, GLsizei bs, GLsizei *l, GLint *sz, GLenum *t, GLchar *n) {
    (void)p;(void)i;(void)bs; if(l)*l=0; if(sz)*sz=1; if(t)*t=GL_FLOAT; if(n&&bs>0)*n=0; }
void glGetAttachedShaders(GLuint p, GLsizei mc, GLsizei *c, GLuint *sh) {
    (void)p;(void)mc; if(c)*c=0; (void)sh; }

void glShaderBinary(GLsizei c, const GLuint *sh, GLenum fmt, const void *bin, GLsizei l) {
    (void)c;(void)sh;(void)fmt;(void)bin;(void)l; record_error(GL_INVALID_OPERATION); }

void glGetShaderPrecisionFormat(GLenum st, GLenum pt, GLint *range, GLint *precision) {
    (void)st;(void)pt;
    if (range) { range[0]=1; range[1]=1; }
    if (precision) *precision=1;
}

/* -------------------------------------------------------------------------
 * Uniforms — stubs accept all types quietly
 * ---------------------------------------------------------------------- */
void glUniform1f(GLint location, GLfloat x)   { (void)location;(void)x; }
void glUniform2f(GLint location, GLfloat x, GLfloat y)   { (void)location;(void)x;(void)y; }
void glUniform3f(GLint location, GLfloat x, GLfloat y, GLfloat z) { (void)location;(void)x;(void)y;(void)z; }
void glUniform4f(GLint location, GLfloat x, GLfloat y, GLfloat z, GLfloat w) { (void)location;(void)x;(void)y;(void)z;(void)w; }
void glUniform1i(GLint location, GLint x) { (void)location;(void)x; }
void glUniform2i(GLint location, GLint x, GLint y) { (void)location;(void)x;(void)y; }
void glUniform3i(GLint location, GLint x, GLint y, GLint z) { (void)location;(void)x;(void)y;(void)z; }
void glUniform4i(GLint location, GLint x, GLint y, GLint z, GLint w) { (void)location;(void)x;(void)y;(void)z;(void)w; }
void glUniform1fv(GLint l, GLsizei c, const GLfloat *v) { (void)l;(void)c;(void)v; }
void glUniform2fv(GLint l, GLsizei c, const GLfloat *v) { (void)l;(void)c;(void)v; }
void glUniform3fv(GLint l, GLsizei c, const GLfloat *v) { (void)l;(void)c;(void)v; }
void glUniform4fv(GLint l, GLsizei c, const GLfloat *v) { (void)l;(void)c;(void)v; }
void glUniform1iv(GLint l, GLsizei c, const GLint *v)   { (void)l;(void)c;(void)v; }
void glUniform2iv(GLint l, GLsizei c, const GLint *v)   { (void)l;(void)c;(void)v; }
void glUniform3iv(GLint l, GLsizei c, const GLint *v)   { (void)l;(void)c;(void)v; }
void glUniform4iv(GLint l, GLsizei c, const GLint *v)   { (void)l;(void)c;(void)v; }
void glUniformMatrix2fv(GLint l, GLsizei c, GLboolean t, const GLfloat *v) { (void)l;(void)c;(void)t;(void)v; }
void glUniformMatrix3fv(GLint l, GLsizei c, GLboolean t, const GLfloat *v) { (void)l;(void)c;(void)t;(void)v; }
void glUniformMatrix4fv(GLint l, GLsizei c, GLboolean t, const GLfloat *v) { (void)l;(void)c;(void)t;(void)v; }
void glGetUniformfv(GLuint p, GLint l, GLfloat *v) { (void)p;(void)l; if(v)*v=0.0f; }
void glGetUniformiv(GLuint p, GLint l, GLint *v)   { (void)p;(void)l; if(v)*v=0; }

/* -------------------------------------------------------------------------
 * Vertex attributes
 * ---------------------------------------------------------------------- */
void glVertexAttribPointer(GLuint indx, GLint size, GLenum type, GLboolean normalized,
                           GLsizei stride, const void *ptr) {
    (void)indx;(void)size;(void)type;(void)normalized;(void)stride;(void)ptr; }
void glEnableVertexAttribArray(GLuint index)  { (void)index; }
void glDisableVertexAttribArray(GLuint index) { (void)index; }
void glVertexAttrib1f(GLuint i, GLfloat x)    { (void)i;(void)x; }
void glVertexAttrib2f(GLuint i, GLfloat x, GLfloat y) { (void)i;(void)x;(void)y; }
void glVertexAttrib3f(GLuint i, GLfloat x, GLfloat y, GLfloat z) { (void)i;(void)x;(void)y;(void)z; }
void glVertexAttrib4f(GLuint i, GLfloat x, GLfloat y, GLfloat z, GLfloat w) { (void)i;(void)x;(void)y;(void)z;(void)w; }
void glVertexAttrib1fv(GLuint i, const GLfloat *v) { (void)i;(void)v; }
void glVertexAttrib2fv(GLuint i, const GLfloat *v) { (void)i;(void)v; }
void glVertexAttrib3fv(GLuint i, const GLfloat *v) { (void)i;(void)v; }
void glVertexAttrib4fv(GLuint i, const GLfloat *v) { (void)i;(void)v; }
void glGetVertexAttribfv(GLuint i, GLenum p, GLfloat *v) { (void)i;(void)p; if(v)*v=0; }
void glGetVertexAttribiv(GLuint i, GLenum p, GLint  *v)  { (void)i;(void)p; if(v)*v=0; }
void glGetVertexAttribPointerv(GLuint i, GLenum p, void **v) { (void)i;(void)p; if(v)*v=NULL; }

/* -------------------------------------------------------------------------
 * Buffer objects
 * ---------------------------------------------------------------------- */
void glGenBuffers(GLsizei n, GLuint *buffers) {
    for (GLsizei i = 0; i < n; i++) buffers[i] = new_id(); }
void glDeleteBuffers(GLsizei n, const GLuint *b) { (void)n;(void)b; }
void glBindBuffer(GLenum target, GLuint buffer) { (void)target;(void)buffer; }
void glBufferData(GLenum t, GLsizeiptr size, const void *data, GLenum usage) {
    (void)t;(void)size;(void)data;(void)usage; }
void glBufferSubData(GLenum t, GLintptr off, GLsizeiptr sz, const void *d) {
    (void)t;(void)off;(void)sz;(void)d; }
void glGetBufferParameteriv(GLenum t, GLenum p, GLint *params) {
    (void)t;(void)p; if(params)*params=0; }
GLboolean glIsBuffer(GLuint b) { (void)b; return GL_FALSE; }

/* -------------------------------------------------------------------------
 * Texture objects
 * ---------------------------------------------------------------------- */
void glGenTextures(GLsizei n, GLuint *textures) {
    for (GLsizei i = 0; i < n; i++) textures[i] = new_id(); }
void glDeleteTextures(GLsizei n, const GLuint *t) { (void)n;(void)t; }
void glBindTexture(GLenum target, GLuint texture) { (void)target;(void)texture; }
void glActiveTexture(GLenum texture) { (void)texture; }
void glTexImage2D(GLenum tgt, GLint level, GLint ifmt, GLsizei w, GLsizei h,
                  GLint border, GLenum fmt, GLenum type, const void *pix) {
    (void)tgt;(void)level;(void)ifmt;(void)w;(void)h;(void)border;(void)fmt;(void)type;(void)pix; }
void glTexSubImage2D(GLenum tgt, GLint level, GLint xo, GLint yo, GLsizei w, GLsizei h,
                     GLenum fmt, GLenum type, const void *pix) {
    (void)tgt;(void)level;(void)xo;(void)yo;(void)w;(void)h;(void)fmt;(void)type;(void)pix; }
void glTexParameterf(GLenum t, GLenum p, GLfloat v)       { (void)t;(void)p;(void)v; }
void glTexParameterfv(GLenum t, GLenum p, const GLfloat *v){ (void)t;(void)p;(void)v; }
void glTexParameteri(GLenum t, GLenum p, GLint v)          { (void)t;(void)p;(void)v; }
void glTexParameteriv(GLenum t, GLenum p, const GLint *v)  { (void)t;(void)p;(void)v; }
void glGetTexParameterfv(GLenum t, GLenum p, GLfloat *v) { (void)t;(void)p; if(v)*v=0; }
void glGetTexParameteriv(GLenum t, GLenum p, GLint  *v)  { (void)t;(void)p; if(v)*v=0; }
void glGenerateMipmap(GLenum target) { (void)target; }
GLboolean glIsTexture(GLuint t) { (void)t; return GL_FALSE; }

/* -------------------------------------------------------------------------
 * Framebuffer / renderbuffer
 * ---------------------------------------------------------------------- */
void glGenFramebuffers(GLsizei n, GLuint *fb)   { for(GLsizei i=0;i<n;i++) fb[i]=new_id(); }
void glGenRenderbuffers(GLsizei n, GLuint *rb)  { for(GLsizei i=0;i<n;i++) rb[i]=new_id(); }
void glDeleteFramebuffers(GLsizei n, const GLuint *fb)  { (void)n;(void)fb; }
void glDeleteRenderbuffers(GLsizei n, const GLuint *rb) { (void)n;(void)rb; }
void glBindFramebuffer(GLenum t, GLuint fb)    { (void)t;(void)fb; }
void glBindRenderbuffer(GLenum t, GLuint rb)   { (void)t;(void)rb; }
void glRenderbufferStorage(GLenum t, GLenum ifmt, GLsizei w, GLsizei h) {
    (void)t;(void)ifmt;(void)w;(void)h; }
void glFramebufferRenderbuffer(GLenum t, GLenum att, GLenum rbt, GLuint rb) {
    (void)t;(void)att;(void)rbt;(void)rb; }
void glFramebufferTexture2D(GLenum t, GLenum att, GLenum txT, GLuint tx, GLint lvl) {
    (void)t;(void)att;(void)txT;(void)tx;(void)lvl; }
GLenum glCheckFramebufferStatus(GLenum target) { (void)target; return GL_FRAMEBUFFER_COMPLETE; }
void glGetRenderbufferParameteriv(GLenum t, GLenum p, GLint *v) { (void)t;(void)p; if(v)*v=0; }
void glGetFramebufferAttachmentParameteriv(GLenum t, GLenum att, GLenum p, GLint *v) {
    (void)t;(void)att;(void)p; if(v)*v=0; }
GLboolean glIsFramebuffer(GLuint f)   { (void)f; return GL_FALSE; }
GLboolean glIsRenderbuffer(GLuint r)  { (void)r; return GL_FALSE; }

/* -------------------------------------------------------------------------
 * Draw operations (recorded but not yet dispatched to Metal)
 * ---------------------------------------------------------------------- */
static GLfloat s_clear_r=0, s_clear_g=0, s_clear_b=0, s_clear_a=1;
static GLfloat s_clear_depth=1;
static GLint   s_clear_stencil=0;

void glClearColor(GLfloat r, GLfloat g, GLfloat b, GLfloat a) {
    s_clear_r=r; s_clear_g=g; s_clear_b=b; s_clear_a=a; }
void glClearDepthf(GLfloat d)  { s_clear_depth=d; }
void glClearStencil(GLint s)   { s_clear_stencil=s; }
void glClear(GLbitfield mask)  { (void)mask; }  /* no-op: Metal clear in swap */

void glViewport(GLint x, GLint y, GLsizei w, GLsizei h) { (void)x;(void)y;(void)w;(void)h; }
void glScissor(GLint x, GLint y, GLsizei w, GLsizei h)  { (void)x;(void)y;(void)w;(void)h; }

void glDrawArrays(GLenum mode, GLint first, GLsizei count) { (void)mode;(void)first;(void)count; }
void glDrawElements(GLenum mode, GLsizei count, GLenum type, const void *indices) {
    (void)mode;(void)count;(void)type;(void)indices; }

void glReadPixels(GLint x, GLint y, GLsizei w, GLsizei h, GLenum fmt, GLenum type, void *pix) {
    (void)x;(void)y;(void)w;(void)h;(void)fmt;(void)type;
    if (pix) memset(pix, 0, (size_t)w*(size_t)h*4); }

/* -------------------------------------------------------------------------
 * State enable/disable
 * ---------------------------------------------------------------------- */
void glEnable(GLenum cap)          { (void)cap; }
void glDisable(GLenum cap)         { (void)cap; }
GLboolean glIsEnabled(GLenum cap)  { (void)cap; return GL_FALSE; }

void glBlendFunc(GLenum sf, GLenum df)  { (void)sf;(void)df; }
void glBlendFuncSeparate(GLenum sRGB, GLenum dRGB, GLenum sA, GLenum dA) {
    (void)sRGB;(void)dRGB;(void)sA;(void)dA; }
void glBlendEquation(GLenum mode)           { (void)mode; }
void glBlendEquationSeparate(GLenum m1, GLenum m2) { (void)m1;(void)m2; }
void glBlendColor(GLfloat r, GLfloat g, GLfloat b, GLfloat a) { (void)r;(void)g;(void)b;(void)a; }
void glColorMask(GLboolean r, GLboolean g, GLboolean b, GLboolean a) { (void)r;(void)g;(void)b;(void)a; }
void glDepthFunc(GLenum f)   { (void)f; }
void glDepthMask(GLboolean f){ (void)f; }
void glDepthRangef(GLfloat n, GLfloat f) { (void)n;(void)f; }
void glCullFace(GLenum m)    { (void)m; }
void glFrontFace(GLenum m)   { (void)m; }
void glLineWidth(GLfloat w)  { (void)w; }
void glPolygonOffset(GLfloat f, GLfloat u) { (void)f;(void)u; }
void glSampleCoverage(GLfloat v, GLboolean inv) { (void)v;(void)inv; }
void glStencilFunc(GLenum fn, GLint ref, GLuint mask) { (void)fn;(void)ref;(void)mask; }
void glStencilFuncSeparate(GLenum face, GLenum fn, GLint ref, GLuint mask) {
    (void)face;(void)fn;(void)ref;(void)mask; }
void glStencilMask(GLuint m) { (void)m; }
void glStencilMaskSeparate(GLenum face, GLuint m) { (void)face;(void)m; }
void glStencilOp(GLenum f, GLenum zf, GLenum zp) { (void)f;(void)zf;(void)zp; }
void glStencilOpSeparate(GLenum face, GLenum f, GLenum zf, GLenum zp) {
    (void)face;(void)f;(void)zf;(void)zp; }
void glHint(GLenum t, GLenum m) { (void)t;(void)m; }
void glPixelStorei(GLenum p, GLint v) { (void)p;(void)v; }

void glFinish(void) {}
void glFlush(void)  {}

/* -------------------------------------------------------------------------
 * GLProgram query stubs
 * ---------------------------------------------------------------------- */
GLboolean glIsProgram(GLuint p) { (void)p; return GL_FALSE; }
GLboolean glIsShader(GLuint s)  { (void)s; return GL_FALSE; }
