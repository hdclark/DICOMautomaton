// GL/glew.h - Android shim for GLEW (GL Extension Wrangler Library).
//
// On Android, OpenGL ES is used instead of desktop OpenGL, and GLEW is not
// available. This shim header replaces GLEW with the equivalent OpenGL ES 3
// headers and provides stub implementations of the GLEW functions used by
// SDL_Viewer.cc.
//
// This shim is only included when building for Android (detected via __ANDROID__).
// When the compiler finds this header before the system GLEW header, it uses
// these stubs instead.

#pragma once

#ifndef __ANDROID__
#error "This GLEW shim is only intended for Android builds."
#endif

// Include OpenGL ES 3.0 instead of desktop OpenGL.
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>

// GLenum, GLuint, GLboolean etc. are already defined by GLES3/gl3.h.

#define GLEW_OK 0

// glewExperimental is a global flag; use inline to avoid ODR violations when
// this header is included in multiple translation units.
inline GLboolean glewExperimental = GL_FALSE;

// glewInit() is a no-op on Android because OpenGL ES loaders are not needed —
// all ES function pointers are resolved at link time by the NDK.
static inline GLenum glewInit(void) {
    return GLEW_OK;
}

// glewGetErrorString() returns a human-readable string for a GLEW error code.
static inline const unsigned char* glewGetErrorString(GLenum error) {
    if(error == GLEW_OK) return (const unsigned char*)"No error";
    return (const unsigned char*)"Unknown GLEW error (Android shim)";
}

// glewGetString() is unused in SDL_Viewer but present for completeness.
#define GLEW_VERSION 1
static inline const unsigned char* glewGetString(GLenum /*name*/) {
    return (const unsigned char*)"Android GLEW shim 1.0";
}
