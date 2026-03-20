/* GLES2/gl2.h — OpenGL ES 2.0 API header for AINE (macOS ARM64)
 * Minimal subset for typical Android apps. Based on Khronos gl2.h.
 */
#ifndef AINE_GLES2_GL2_H
#define AINE_GLES2_GL2_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------
 * GL types
 * ---------------------------------------------------------------------- */
typedef unsigned int   GLenum;
typedef unsigned char  GLboolean;
typedef unsigned int   GLbitfield;
typedef signed char    GLbyte;
typedef short          GLshort;
typedef int            GLint;
typedef int            GLsizei;
typedef unsigned char  GLubyte;
typedef unsigned short GLushort;
typedef unsigned int   GLuint;
typedef float          GLfloat;
typedef float          GLclampf;
typedef int            GLfixed;
typedef intptr_t       GLintptr;
typedef intptr_t       GLsizeiptr;
typedef char           GLchar;
typedef void           GLvoid;

/* -----------------------------------------------------------------------
 * GL constants
 * ---------------------------------------------------------------------- */

/* Boolean */
#define GL_FALSE                            0
#define GL_TRUE                             1

/* BeginMode */
#define GL_POINTS                           0x0000
#define GL_LINES                            0x0001
#define GL_LINE_LOOP                        0x0002
#define GL_LINE_STRIP                       0x0003
#define GL_TRIANGLES                        0x0004
#define GL_TRIANGLE_STRIP                   0x0005
#define GL_TRIANGLE_FAN                     0x0006

/* BlendingFactorDest */
#define GL_ZERO                             0
#define GL_ONE                              1
#define GL_SRC_COLOR                        0x0300
#define GL_ONE_MINUS_SRC_COLOR              0x0301
#define GL_SRC_ALPHA                        0x0302
#define GL_ONE_MINUS_SRC_ALPHA              0x0303
#define GL_DST_ALPHA                        0x0304
#define GL_ONE_MINUS_DST_ALPHA              0x0305
#define GL_DST_COLOR                        0x0306
#define GL_ONE_MINUS_DST_COLOR              0x0307
#define GL_SRC_ALPHA_SATURATE               0x0308

/* ClearBufferMask */
#define GL_DEPTH_BUFFER_BIT                 0x00000100
#define GL_STENCIL_BUFFER_BIT               0x00000400
#define GL_COLOR_BUFFER_BIT                 0x00004000

/* Errors */
#define GL_NO_ERROR                         0
#define GL_INVALID_ENUM                     0x0500
#define GL_INVALID_VALUE                    0x0501
#define GL_INVALID_OPERATION               0x0502
#define GL_OUT_OF_MEMORY                    0x0505

/* FrontFaceDirection */
#define GL_CW                               0x0900
#define GL_CCW                              0x0901

/* Framebuffer/renderbuffer */
#define GL_FRAMEBUFFER                      0x8D40
#define GL_RENDERBUFFER                     0x8D41
#define GL_FRAMEBUFFER_COMPLETE             0x8CD5
#define GL_COLOR_ATTACHMENT0                0x8CE0
#define GL_DEPTH_ATTACHMENT                 0x8D00
#define GL_STENCIL_ATTACHMENT               0x8D20

/* Data types */
#define GL_BYTE                             0x1400
#define GL_UNSIGNED_BYTE                    0x1401
#define GL_SHORT                            0x1402
#define GL_UNSIGNED_SHORT                   0x1403
#define GL_INT                              0x1404
#define GL_UNSIGNED_INT                     0x1405
#define GL_FLOAT                            0x1406
#define GL_FIXED                            0x140C

/* Pixel formats */
#define GL_ALPHA                            0x1906
#define GL_RGB                              0x1907
#define GL_RGBA                             0x1908
#define GL_LUMINANCE                        0x1909
#define GL_LUMINANCE_ALPHA                  0x190A

/* Texture */
#define GL_TEXTURE_2D                       0x0DE1
#define GL_TEXTURE0                         0x84C0
#define GL_TEXTURE_WRAP_S                   0x2802
#define GL_TEXTURE_WRAP_T                   0x2803
#define GL_TEXTURE_MIN_FILTER               0x2801
#define GL_TEXTURE_MAG_FILTER               0x2800
#define GL_LINEAR                           0x2601
#define GL_NEAREST                          0x2600
#define GL_REPEAT                           0x2901
#define GL_CLAMP_TO_EDGE                    0x812F

/* Shaders */
#define GL_VERTEX_SHADER                    0x8B31
#define GL_FRAGMENT_SHADER                  0x8B30
#define GL_COMPILE_STATUS                   0x8B81
#define GL_LINK_STATUS                      0x8B82
#define GL_INFO_LOG_LENGTH                  0x8B84

/* Buffer objects */
#define GL_ARRAY_BUFFER                     0x8892
#define GL_ELEMENT_ARRAY_BUFFER             0x8893
#define GL_STATIC_DRAW                      0x88B4
#define GL_DYNAMIC_DRAW                     0x88E8
#define GL_STREAM_DRAW                      0x88E0

/* glGet params */
#define GL_VIEWPORT                         0x0BA2
#define GL_MAX_VERTEX_ATTRIBS               0x8869
#define GL_MAX_TEXTURE_SIZE                 0x0D33
#define GL_VENDOR                           0x1F00
#define GL_RENDERER                         0x1F01
#define GL_VERSION                          0x1F02
#define GL_EXTENSIONS                       0x1F03
#define GL_SHADING_LANGUAGE_VERSION         0x8B8C

/* Enable/Disable */
#define GL_CULL_FACE                        0x0B44
#define GL_BLEND                            0x0BE2
#define GL_DITHER                           0x0BD0
#define GL_STENCIL_TEST                     0x0B90
#define GL_DEPTH_TEST                       0x0B71
#define GL_SCISSOR_TEST                     0x0C11
#define GL_POLYGON_OFFSET_FILL              0x8037
#define GL_SAMPLE_ALPHA_TO_COVERAGE         0x809E
#define GL_SAMPLE_COVERAGE                  0x80A0

/* -----------------------------------------------------------------------
 * GL ES 2.0 function declarations
 * ---------------------------------------------------------------------- */

void        glActiveTexture(GLenum texture);
void        glAttachShader(GLuint program, GLuint shader);
void        glBindAttribLocation(GLuint program, GLuint index, const GLchar *name);
void        glBindBuffer(GLenum target, GLuint buffer);
void        glBindFramebuffer(GLenum target, GLuint framebuffer);
void        glBindRenderbuffer(GLenum target, GLuint renderbuffer);
void        glBindTexture(GLenum target, GLuint texture);
void        glBlendColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
void        glBlendEquation(GLenum mode);
void        glBlendEquationSeparate(GLenum modeRGB, GLenum modeAlpha);
void        glBlendFunc(GLenum sfactor, GLenum dfactor);
void        glBlendFuncSeparate(GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha);
void        glBufferData(GLenum target, GLsizeiptr size, const void *data, GLenum usage);
void        glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void *data);
GLenum      glCheckFramebufferStatus(GLenum target);
void        glClear(GLbitfield mask);
void        glClearColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
void        glClearDepthf(GLfloat depth);
void        glClearStencil(GLint s);
void        glColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);
void        glCompileShader(GLuint shader);
GLuint      glCreateProgram(void);
GLuint      glCreateShader(GLenum type);
void        glCullFace(GLenum mode);
void        glDeleteBuffers(GLsizei n, const GLuint *buffers);
void        glDeleteFramebuffers(GLsizei n, const GLuint *framebuffers);
void        glDeleteProgram(GLuint program);
void        glDeleteRenderbuffers(GLsizei n, const GLuint *renderbuffers);
void        glDeleteShader(GLuint shader);
void        glDeleteTextures(GLsizei n, const GLuint *textures);
void        glDepthFunc(GLenum func);
void        glDepthMask(GLboolean flag);
void        glDepthRangef(GLfloat n, GLfloat f);
void        glDetachShader(GLuint program, GLuint shader);
void        glDisable(GLenum cap);
void        glDisableVertexAttribArray(GLuint index);
void        glDrawArrays(GLenum mode, GLint first, GLsizei count);
void        glDrawElements(GLenum mode, GLsizei count, GLenum type, const void *indices);
void        glEnable(GLenum cap);
void        glEnableVertexAttribArray(GLuint index);
void        glFinish(void);
void        glFlush(void);
void        glFramebufferRenderbuffer(GLenum target, GLenum attachment, GLenum rbtype, GLuint renderbuffer);
void        glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
void        glFrontFace(GLenum mode);
void        glGenBuffers(GLsizei n, GLuint *buffers);
void        glGenerateMipmap(GLenum target);
void        glGenFramebuffers(GLsizei n, GLuint *framebuffers);
void        glGenRenderbuffers(GLsizei n, GLuint *renderbuffers);
void        glGenTextures(GLsizei n, GLuint *textures);
void        glGetActiveAttrib(GLuint program, GLuint index, GLsizei bufsize, GLsizei *length, GLint *size, GLenum *type, GLchar *name);
void        glGetActiveUniform(GLuint program, GLuint index, GLsizei bufsize, GLsizei *length, GLint *size, GLenum *type, GLchar *name);
void        glGetAttachedShaders(GLuint program, GLsizei maxcount, GLsizei *count, GLuint *shaders);
GLint       glGetAttribLocation(GLuint program, const GLchar *name);
void        glGetBooleanv(GLenum pname, GLboolean *params);
void        glGetBufferParameteriv(GLenum target, GLenum pname, GLint *params);
GLenum      glGetError(void);
void        glGetFloatv(GLenum pname, GLfloat *params);
void        glGetFramebufferAttachmentParameteriv(GLenum target, GLenum attachment, GLenum pname, GLint *params);
void        glGetIntegerv(GLenum pname, GLint *params);
void        glGetProgramiv(GLuint program, GLenum pname, GLint *params);
void        glGetProgramInfoLog(GLuint program, GLsizei bufsize, GLsizei *length, GLchar *infolog);
void        glGetRenderbufferParameteriv(GLenum target, GLenum pname, GLint *params);
void        glGetShaderiv(GLuint shader, GLenum pname, GLint *params);
void        glGetShaderInfoLog(GLuint shader, GLsizei bufsize, GLsizei *length, GLchar *infolog);
void        glGetShaderPrecisionFormat(GLenum shadertype, GLenum precisiontype, GLint *range, GLint *precision);
void        glGetShaderSource(GLuint shader, GLsizei bufsize, GLsizei *length, GLchar *source);
const GLubyte* glGetString(GLenum name);
void        glGetTexParameterfv(GLenum target, GLenum pname, GLfloat *params);
void        glGetTexParameteriv(GLenum target, GLenum pname, GLint *params);
void        glGetUniformfv(GLuint program, GLint location, GLfloat *params);
void        glGetUniformiv(GLuint program, GLint location, GLint *params);
GLint       glGetUniformLocation(GLuint program, const GLchar *name);
void        glGetVertexAttribfv(GLuint index, GLenum pname, GLfloat *params);
void        glGetVertexAttribiv(GLuint index, GLenum pname, GLint *params);
void        glGetVertexAttribPointerv(GLuint index, GLenum pname, void **pointer);
void        glHint(GLenum target, GLenum mode);
GLboolean   glIsBuffer(GLuint buffer);
GLboolean   glIsEnabled(GLenum cap);
GLboolean   glIsFramebuffer(GLuint framebuffer);
GLboolean   glIsProgram(GLuint program);
GLboolean   glIsRenderbuffer(GLuint renderbuffer);
GLboolean   glIsShader(GLuint shader);
GLboolean   glIsTexture(GLuint texture);
void        glLineWidth(GLfloat width);
void        glLinkProgram(GLuint program);
void        glPixelStorei(GLenum pname, GLint param);
void        glPolygonOffset(GLfloat factor, GLfloat units);
void        glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void *pixels);
void        glReleaseShaderCompiler(void);
void        glRenderbufferStorage(GLenum target, GLenum internalformat, GLsizei width, GLsizei height);
void        glSampleCoverage(GLfloat value, GLboolean invert);
void        glScissor(GLint x, GLint y, GLsizei width, GLsizei height);
void        glShaderBinary(GLsizei count, const GLuint *shaders, GLenum binaryformat, const void *binary, GLsizei length);
void        glShaderSource(GLuint shader, GLsizei count, const GLchar *const *string, const GLint *length);
void        glStencilFunc(GLenum func, GLint ref, GLuint mask);
void        glStencilFuncSeparate(GLenum face, GLenum func, GLint ref, GLuint mask);
void        glStencilMask(GLuint mask);
void        glStencilMaskSeparate(GLenum face, GLuint mask);
void        glStencilOp(GLenum fail, GLenum zfail, GLenum zpass);
void        glStencilOpSeparate(GLenum face, GLenum fail, GLenum zfail, GLenum zpass);
void        glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void *pixels);
void        glTexParameterf(GLenum target, GLenum pname, GLfloat param);
void        glTexParameterfv(GLenum target, GLenum pname, const GLfloat *params);
void        glTexParameteri(GLenum target, GLenum pname, GLint param);
void        glTexParameteriv(GLenum target, GLenum pname, const GLint *params);
void        glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void *pixels);
void        glUniform1f(GLint location, GLfloat x);
void        glUniform1fv(GLint location, GLsizei count, const GLfloat *v);
void        glUniform1i(GLint location, GLint x);
void        glUniform1iv(GLint location, GLsizei count, const GLint *v);
void        glUniform2f(GLint location, GLfloat x, GLfloat y);
void        glUniform2fv(GLint location, GLsizei count, const GLfloat *v);
void        glUniform2i(GLint location, GLint x, GLint y);
void        glUniform2iv(GLint location, GLsizei count, const GLint *v);
void        glUniform3f(GLint location, GLfloat x, GLfloat y, GLfloat z);
void        glUniform3fv(GLint location, GLsizei count, const GLfloat *v);
void        glUniform3i(GLint location, GLint x, GLint y, GLint z);
void        glUniform3iv(GLint location, GLsizei count, const GLint *v);
void        glUniform4f(GLint location, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
void        glUniform4fv(GLint location, GLsizei count, const GLfloat *v);
void        glUniform4i(GLint location, GLint x, GLint y, GLint z, GLint w);
void        glUniform4iv(GLint location, GLsizei count, const GLint *v);
void        glUniformMatrix2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
void        glUniformMatrix3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
void        glUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
void        glUseProgram(GLuint program);
void        glValidateProgram(GLuint program);
void        glVertexAttrib1f(GLuint indx, GLfloat x);
void        glVertexAttrib1fv(GLuint indx, const GLfloat *values);
void        glVertexAttrib2f(GLuint indx, GLfloat x, GLfloat y);
void        glVertexAttrib2fv(GLuint indx, const GLfloat *values);
void        glVertexAttrib3f(GLuint indx, GLfloat x, GLfloat y, GLfloat z);
void        glVertexAttrib3fv(GLuint indx, const GLfloat *values);
void        glVertexAttrib4f(GLuint indx, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
void        glVertexAttrib4fv(GLuint indx, const GLfloat *values);
void        glVertexAttribPointer(GLuint indx, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *ptr);
void        glViewport(GLint x, GLint y, GLsizei width, GLsizei height);

#ifdef __cplusplus
}
#endif

#endif /* AINE_GLES2_GL2_H */
