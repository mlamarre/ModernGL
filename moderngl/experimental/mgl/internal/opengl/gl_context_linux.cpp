#include "gl_context.hpp"
#include "../modules.hpp"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <iostream>

namespace
{
    constexpr EGLint attrib_list[] =
    {
        EGL_SURFACE_TYPE,       EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE,    EGL_OPENGL_BIT,
        EGL_NONE
    };
}

bool GLContext::load(bool standalone) {
    this->standalone = standalone;

    int width = 1;
    int height = 1;

    EGLDisplay dpy = eglGetCurrentDisplay();

    if (!dpy) {
        PyErr_Format(moderngl_error, "cannot detect the display");
        return false;
    }

    EGLint egl_major_ver;
    EGLint egl_minor_ver;
    if(eglInitialize(display, &egl_major_ver, &egl_minor_ver) == EGL_FALSE)
    {
        EGLint error = eglGetError();
        switch(error)
        {
        case EGL_BAD_DISPLAY:
            PyErr_Format(moderngl_error, "display is not an EGL display connection");
            break;
        case EGL_NOT_INITIALIZED:
            PyErr_Format(moderngl_error, "display cannot be initialized");
            break;
        default:
            break;
        }
        return false;
    }
    
    std::cout << "EGL version: " << egl_major_ver << "." << egl_minor_ver << std::endl;

    char const * client_apis = eglQueryString(display, EGL_CLIENT_APIS);
    if(!client_apis)
    {
        PyErr_Format(moderngl_error,"Failed to eglQueryString(display, EGL_CLIENT_APIS)");
        return false;
    }
    
    std::cout << "Supported client rendering APIs: " << client_apis << std::endl;

    EGLConfig config;
    EGLint    num_config;
    if(eglChooseConfig(display, attrib_list, &config, 1, &num_config) == EGL_FALSE)
    {
        PyErr_Format(moderngl_error,"Failed to eglChooseConfig");
        return false;
    }
    if(num_config < 1)
    {
        PyErr_Format(moderngl_error,"No matching EGL frame buffer configuration");
        return false;
    }

    if(eglBindAPI(EGL_OPENGL_API) == EGL_FALSE)
    {
        PyErr_Format(moderngl_error,"Failed to eglBindAPI(EGL_OPENGL_API)");
        return false;
    }

    const EGLint context_attrib[] =
    {
        EGL_CONTEXT_MAJOR_VERSION_KHR,  4,
        EGL_CONTEXT_MINOR_VERSION_KHR,  6,
        EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR,    EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR,
        EGL_CONTEXT_FLAGS_KHR,          EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE_BIT_KHR,
        EGL_NONE
    };

    EGLContext egl_ctx = eglCreateContext(display, config, EGL_NO_CONTEXT, context_attrib);
    if(egl_ctx == EGL_NO_CONTEXT)
    {
        EGLint error =  eglGetError();
        switch(error)
        {
        case EGL_BAD_CONFIG:
            PyErr_Format(moderngl_error,"config is not an EGL frame buffer configuration, or does not support the current rendering API");
            break;
        case EGL_BAD_ATTRIBUTE:
            PyErr_Format(moderngl_error,"attrib_list contains an invalid context attribute or if an attribute is not recognized or out of range");
            break;
        default:
            PyErr_Format(moderngl_error,"Unknown error");
            break;
        }
        return false;
    }

    if(eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, egl_ctx) == EGL_FALSE)
    {
        PyErr_Format(moderngl_error,"Failed to eglMakeCurrent");
        return false;
    }

    if(EGL_SUCCESS != eglGetError())
    {
        PyErr_Format(moderngl_error,"eglGetError != EGL_SUCCESS");
        return false;
    }

    this->window = nullptr; // no window!
    this->display = (void *)display;
    this->context = (void *)egl_ctx;

    return true;
}

void GLContext::enter() {
    this->old_display = (void *)eglGetCurrentDisplay();
    this->old_window = nullptr;
    this->old_context = (void *)eglGetCurrentContext();
    eglMakeCurrent((EGLDisplay) this->display, EGL_NO_SURFACE, EGL_NO_SURFACE, (EGLContext) this->context);
}

void GLContext::exit() {
    eglMakeCurrent((EGLDisplay) this->old_display, EGL_NO_SURFACE, EGL_NO_SURFACE, (EGLContext) this->old_context);
}

bool GLContext::active() {
    return this->context == eglGetCurrentContext();
}
