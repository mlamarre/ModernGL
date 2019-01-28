#include "GLContext.hpp"

#include "Error.hpp"

struct GLVersion {
	int major;
	int minor;
};

GLVersion version[] = {
	{4, 6},
	{4, 5},
	{4, 4},
	{4, 3},
	{4, 2},
	{4, 1},
	{4, 0},
	{3, 3},
	{3, 2},
	{3, 1},
	{0, 0},
};

int versions = sizeof(version) / sizeof(GLVersion);

#if defined(_WIN32) || defined(_WIN64)

#include <Windows.h>

#define WGL_ACCELERATION 0x2003
#define WGL_FULL_ACCELERATION 0x2027
#define WGL_CONTEXT_MAJOR_VERSION 0x2091
#define WGL_CONTEXT_MINOR_VERSION 0x2092
#define WGL_CONTEXT_PROFILE_MASK 0x9126
#define WGL_CONTEXT_CORE_PROFILE_BIT 0x0001

typedef int (WINAPI * my_ChoosePixelFormat_type)(HDC hdc, const int * piAttribIList, const float * pfAttribFList, unsigned nMaxFormats, int * piFormats, unsigned * nNumFormats);
typedef HGLRC (WINAPI * my_CreateContextAttribs_type)(HDC hdc, HGLRC hglrc, const int * attribList);

my_ChoosePixelFormat_type my_ChoosePixelFormat;
my_CreateContextAttribs_type my_CreateContextAttribs;

PIXELFORMATDESCRIPTOR pfd = {
	sizeof(PIXELFORMATDESCRIPTOR),  // nSize
	1,                              // nVersion
	PFD_DRAW_TO_WINDOW |
	PFD_SUPPORT_OPENGL |
	PFD_GENERIC_ACCELERATED |
	PFD_DOUBLEBUFFER,               // dwFlags
	0,                              // iPixelType
	32,                             // cColorBits
	0,                              // cRedBits
	0,                              // cRedShift
	0,                              // cGreenBits
	0,                              // cGreenShift
	0,                              // cBlueBits
	0,                              // cBlueShift
	0,                              // cAlphaBits
	0,                              // cAlphaShift
	0,                              // cAccumBits
	0,                              // cAccumRedBits
	0,                              // cAccumGreenBits
	0,                              // cAccumBlueBits
	0,                              // cAccumAlphaBits
	24,                             // cDepthBits
	0,                              // cStencilBits
	0,                              // cAuxBuffers
	0,                              // iLayerType
	0,                              // bReserved
	0,                              // dwLayerMask
	0,                              // dwVisibleMask
	0,                              // dwDamageMask
};

GLContext LoadCurrentGLContext() {
	GLContext context = {};
	context.standalone = false;

	HGLRC hrc = wglGetCurrentContext();

	if (!hrc) {
		MGLError_Set("cannot detect context");
		return context;
	}

	HDC hdc = wglGetCurrentDC();

	if (!hdc) {
		MGLError_Set("cannot detect device content");
		return context;
	}

	HWND hwnd = WindowFromDC(hdc);

	if (!hwnd) {
		MGLError_Set("cannot detect window");
		return context;
	}

	context.hwnd = (void *)hwnd;
	context.hdc = (void *)hdc;
	context.hrc = (void *)hrc;

	return context;
}

void InitModernContext() {
	static bool initialized = false;

	if (initialized) {
		return;
	}

	initialized = true;

	HMODULE hinst = GetModuleHandle(0);

	if (!hinst) {
		return;
	}

	WNDCLASS extClass = {
		CS_OWNDC,                       // style
		DefWindowProc,                  // lpfnWndProc
		0,                              // cbClsExtra
		0,                              // cbWndExtra
		hinst,                          // hInstance
		0,                              // hIcon
		0,                              // hCursor
		0,                              // hbrBackground
		0,                              // lpszMenuName
		"ContextLoader",                // lpszClassName
	};

	if (!RegisterClass(&extClass)) {
		return;
	}

	HWND loader_hwnd = CreateWindow(
		"ContextLoader",                // lpClassName
		0,                              // lpWindowName
		0,                              // dwStyle
		0,                              // x
		0,                              // y
		0,                              // nWidth
		0,                              // nHeight
		0,                              // hWndParent
		0,                              // hMenu
		hinst,                          // hInstance
		0                               // lpParam
	);

	if (!loader_hwnd) {
		return;
	}

	HDC loader_hdc = GetDC(loader_hwnd);

	if (!loader_hdc) {
		return;
	}

	int loader_pixelformat = ChoosePixelFormat(loader_hdc, &pfd);

	if (!loader_pixelformat) {
		return;
	}

	if (!SetPixelFormat(loader_hdc, loader_pixelformat, &pfd)) {
		return;
	}

	HGLRC loader_hglrc = wglCreateContext(loader_hdc);

	if (!loader_hglrc) {
		return;
	}

	if (!wglMakeCurrent(loader_hdc, loader_hglrc)) {
		return;
	}

	my_CreateContextAttribs = (my_CreateContextAttribs_type)wglGetProcAddress("wglCreateContextAttribsARB");

	if (!my_CreateContextAttribs) {
		return;
	}

	my_ChoosePixelFormat = (my_ChoosePixelFormat_type)wglGetProcAddress("wglChoosePixelFormatARB");

	if (!my_ChoosePixelFormat) {
		return;
	}

	if (!wglMakeCurrent(0, 0)) {
		return;
	}

	if (!wglDeleteContext(loader_hglrc)) {
		return;
	}

	if (!ReleaseDC(loader_hwnd, loader_hdc)) {
		return;
	}

	if (!DestroyWindow(loader_hwnd)) {
		return;
	}

	if (!UnregisterClass("ContextLoader", hinst)) {
		return;
	}
}

GLContext CreateGLContext(PyObject * settings) {
	GLContext context = {};
	context.standalone = true;

	int width = 1;
	int height = 1;
	PyObject * size_hint = (settings != Py_None) ? PyDict_GetItemString(settings, "size") : 0;
	if (size_hint && Py_TYPE(size_hint) == &PyTuple_Type && PyTuple_GET_SIZE(size_hint) == 2) {
		width = PyLong_AsLong(PyTuple_GET_ITEM(size_hint, 0));
		height = PyLong_AsLong(PyTuple_GET_ITEM(size_hint, 1));
		width = width < 1 ? width : 1;
		height = height < 1 ? height : 1;
	}

	InitModernContext();

	HINSTANCE inst = GetModuleHandle(0);

	if (!inst) {
		MGLError_Set("module handle is null");
		return context;
	}

	static bool registered = false;

	if (!registered) {
		WNDCLASS wndClass = {
			CS_OWNDC,                            // style
			DefWindowProc,                       // lpfnWndProc
			0,                                   // cbClsExtra
			0,                                   // cbWndExtra
			inst,                                // hInstance
			0,                                   // hIcon
			0,                                   // hCursor
			0,                                   // hbrBackground
			0,                                   // lpszMenuName
			"StandaloneContext",                 // lpszClassName
		};

		if (!RegisterClass(&wndClass)) {
			MGLError_Set("cannot register window class");
			return context;
		}

		registered = true;
	}

	HWND hwnd = CreateWindowEx(
		0,                                   // exStyle
		"StandaloneContext",                 // lpClassName
		0,                                   // lpWindowName
		0,                                   // dwStyle
		0,                                   // x
		0,                                   // y
		width,                               // nWidth
		height,                              // nHeight
		0,                                   // hWndParent
		0,                                   // hMenu
		inst,                                // hInstance
		0                                    // lpParam
	);

	if (!hwnd) {
		MGLError_Set("cannot create window");
		return context;
	}

	HDC hdc = GetDC(hwnd);

	if (!hdc) {
		MGLError_Set("cannot create device content");
		return context;
	}

	HGLRC hrc = 0;

	if (my_CreateContextAttribs && my_ChoosePixelFormat) {

		int pixelformat = 0;
		unsigned num_formats = 0;

		int zeroes1[4] = {};
		float zeroes2[4] = {};

		// WGL_ACCELERATION_ARB, WGL_FULL_ACCELERATION_ARB

		if (!my_ChoosePixelFormat(hdc, zeroes1, zeroes2, 1, &pixelformat, &num_formats)) {
			MGLError_Set("cannot choose pixel format");
			return context;
		}

		if (!num_formats) {
			MGLError_Set("no pixel formats available");
			return context;
		}

		if (!SetPixelFormat(hdc, pixelformat, &pfd)) {
			MGLError_Set("cannot set pixel format");
			return context;
		}

		for (int i = 0; i < versions; ++i) {
			int attribs[] = {
				WGL_CONTEXT_PROFILE_MASK, WGL_CONTEXT_CORE_PROFILE_BIT,
				WGL_CONTEXT_MAJOR_VERSION, version[i].major,
				WGL_CONTEXT_MINOR_VERSION, version[i].minor,
				0, 0,
			};

			hrc = my_CreateContextAttribs(hdc, 0, attribs);

			if (hrc) {
				break;
			}
		}

	} else {

		int pf = ChoosePixelFormat(hdc, &pfd);

		if (!pf) {
			MGLError_Set("cannot choose pixel format");
			return context;
		}

		int set_pixel_format = SetPixelFormat(hdc, pf, &pfd);

		if (!set_pixel_format) {
			MGLError_Set("cannot set pixel format");
			return context;
		}

		hrc = wglCreateContext(hdc);

	}

	if (!hrc) {
		MGLError_Set("cannot create OpenGL context");
		return context;
	}

	int make_current = wglMakeCurrent(hdc, hrc);

	if (!make_current) {
		MGLError_Set("cannot select OpenGL context");
		return context;
	}

	context.hwnd = (void *)hwnd;
	context.hrc = (void *)hrc;
	context.hdc = (void *)hdc;

	return context;
}

void DestroyGLContext(const GLContext & context) {
	if (!context.standalone) {
		return;
	}

	if (wglGetCurrentContext() == context.hrc) {
		wglMakeCurrent(0, 0);
	}

	wglDeleteContext((HGLRC)context.hrc);
	ReleaseDC((HWND)context.hwnd, (HDC)context.hdc);
	DestroyWindow((HWND)context.hwnd);
}

#elif defined(__APPLE__)

#include <OpenGL/OpenGL.h>
#include <ApplicationServices/ApplicationServices.h>

#include <OpenGL/gl.h>
#include <OpenGL/glext.h>

GLContext LoadCurrentGLContext() {
	GLContext context = {};
	context.standalone = false;

	CGLContextObj cgl_context = CGLGetCurrentContext();

	if (!cgl_context) {
		MGLError_Set("cannot detect OpenGL context");
		return context;
	}

	context.context = (void *)cgl_context;

	return context;
}

GLContext CreateGLContext(PyObject * settings) {
	GLContext context = {};
	context.standalone = true;

	int width = 1;
	int height = 1;
	PyObject * size_hint = (settings != Py_None) ? PyDict_GetItemString(settings, "size") : 0;
	if (size_hint && Py_TYPE(size_hint) == &PyTuple_Type && PyTuple_GET_SIZE(size_hint) == 2) {
		width = PyLong_AsLong(PyTuple_GET_ITEM(size_hint, 0));
		height = PyLong_AsLong(PyTuple_GET_ITEM(size_hint, 1));
		width = width < 1 ? width : 1;
		height = height < 1 ? height : 1;
	}

	// CGDirectDisplayID display = CGMainDisplayID();
	// CGOpenGLDisplayMask cglDisplayMask = CGDisplayIDToOpenGLDisplayMask(display);
	// CGLContextObj curr_ctx = CGLGetCurrentContext();

	GLint num_pixelformats = 0;
	CGLPixelFormatObj pixelformat = 0;

	// kCGLPFAAccelerated
	CGLPixelFormatAttribute attribs[] = {
		kCGLPFAOpenGLProfile,
		(CGLPixelFormatAttribute)kCGLOGLPVersion_GL4_Core,
		(CGLPixelFormatAttribute)0,
	};

	CGLChoosePixelFormat(attribs, &pixelformat, &num_pixelformats);

	if (!pixelformat) {

		CGLPixelFormatAttribute attribs[] = {
			kCGLPFAOpenGLProfile,
			(CGLPixelFormatAttribute)kCGLOGLPVersion_GL3_Core,
			(CGLPixelFormatAttribute)0,
		};

		CGLChoosePixelFormat(attribs, &pixelformat, &num_pixelformats);

		if (!pixelformat) {
			CGLPixelFormatAttribute attribs[] = {
				(CGLPixelFormatAttribute)0,
			};

			CGLChoosePixelFormat(attribs, &pixelformat, &num_pixelformats);
		}
	}

	if (!pixelformat) {
		MGLError_Set("cannot choose pixel format");
		return context;
	}

	CGLContextObj cgl_context = 0;

	CGLCreateContext(pixelformat, 0, &cgl_context);
	CGLDestroyPixelFormat(pixelformat);

	if (!cgl_context) {
		MGLError_Set("cannot create OpenGL context");
		return context;
	}

	CGLSetCurrentContext(cgl_context);

	int fbo = 0;
	int color_rbo = 0;
	int depth_rbo = 0;

	glGenFramebuffers(1, (GLuint *)&fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);

	glGenRenderbuffers(1, (GLuint *)&color_rbo);
	glBindRenderbuffer(GL_RENDERBUFFER, color_rbo);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA, width, height);

	glGenRenderbuffers(1, (GLuint *)&depth_rbo);
	glBindRenderbuffer(GL_RENDERBUFFER, depth_rbo);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);

	glFramebufferRenderbuffer(
		GL_FRAMEBUFFER,
		GL_COLOR_ATTACHMENT0,
		GL_RENDERBUFFER,
		color_rbo
	);

	glFramebufferRenderbuffer(
		GL_FRAMEBUFFER,
		GL_DEPTH_ATTACHMENT,
		GL_RENDERBUFFER,
		depth_rbo
	);

	int status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

	if (status != GL_FRAMEBUFFER_COMPLETE) {
		const char * message = "the framebuffer is not complete";

		switch (status) {
			case GL_FRAMEBUFFER_UNDEFINED:
				message = "the framebuffer is not complete (UNDEFINED)";
				break;

			case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
				message = "the framebuffer is not complete (INCOMPLETE_ATTACHMENT)";
				break;

			case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
				message = "the framebuffer is not complete (INCOMPLETE_MISSING_ATTACHMENT)";
				break;

			case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
				message = "the framebuffer is not complete (INCOMPLETE_DRAW_BUFFER)";
				break;

			case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
				message = "the framebuffer is not complete (INCOMPLETE_READ_BUFFER)";
				break;

			case GL_FRAMEBUFFER_UNSUPPORTED:
				message = "the framebuffer is not complete (UNSUPPORTED)";
				break;

			case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
				message = "the framebuffer is not complete (INCOMPLETE_MULTISAMPLE)";
				break;
		}

		MGLError_Set(message);
		return context;
	}

	context.context = (void *)cgl_context;

	return context;
}

void DestroyGLContext(const GLContext & context) {
	if (!context.standalone) {
		return;
	}

	if (context.context) {
		CGLDestroyContext((CGLContextObj)context.context);
		// context.context = 0;
	}
}

#else

#include <EGL/egl.h>
#include <EGL/eglext.h>

namespace
{
    constexpr EGLint attrib_list[] =
    {
        EGL_SURFACE_TYPE,       EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE,    EGL_OPENGL_BIT,
        EGL_NONE
    };
}

GLContext LoadCurrentGLContext() {
	GLContext context = {};
	context.standalone = false;

	EGLDisplay * dpy = eglGetCurrentDisplay();

	if (!dpy) {
		MGLError_Set("cannot detect display");
		return context;
	}
	
	EGLContext ctx = eglGetCurrentContext();

	if (!ctx) {
		MGLError_Set("cannot detect OpenGL context");
		return context;
	}

	context.display = (void *)dpy;
	context.window = nullptr;
	context.context = (void *)ctx;

	return context;
}

GLContext CreateGLContext(PyObject * settings) {
	GLContext context = {};
	context.standalone = true;


	int width = 1;
	int height = 1;
	PyObject * size_hint = (settings != Py_None) ? PyDict_GetItemString(settings, "size") : 0;
	if (size_hint && Py_TYPE(size_hint) == &PyTuple_Type && PyTuple_GET_SIZE(size_hint) == 2) {
		width = PyLong_AsLong(PyTuple_GET_ITEM(size_hint, 0));
		height = PyLong_AsLong(PyTuple_GET_ITEM(size_hint, 1));
		width = width < 1 ? width : 1;
		height = height < 1 ? height : 1;
	}

	auto display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if(display == EGL_NO_DISPLAY)
    {
        MGLError_Set("Failed to eglGetDisplay(EGL_DEFAULT_DISPLAY)");
        return context;
    }

    EGLint egl_major_ver;
    EGLint egl_minor_ver;
    if(eglInitialize(display, &egl_major_ver, &egl_minor_ver) == EGL_FALSE)
    {
        MGLError_Set("Failed to eglInitialize");
		EGLint error =  eglGetError();
        switch(error)
        {
        case EGL_BAD_DISPLAY:
            MGLError_Set("display is not an EGL display connection");
            break;
        case EGL_NOT_INITIALIZED:
            MGLError_Set("display cannot be initialized");
            break;
        default:
            break;
        }
        return context;
    }
    cout << "EGL version: " << egl_major_ver << "." << egl_minor_ver << endl;

    char const * client_apis = eglQueryString(display, EGL_CLIENT_APIS);
    if(!client_apis)
    {
        MGLError_Set("Failed to eglQueryString(display, EGL_CLIENT_APIS)");
        return context;
    }
    cout << "Supported client rendering APIs: " << client_apis << endl;

    EGLConfig config;
    EGLint    num_config;
    if(eglChooseConfig(display, attrib_list, &config, 1, &num_config) == EGL_FALSE)
    {
        MGLError_Set("Failed to eglChooseConfig");
        return context;
    }
    if(num_config < 1)
    {
        MGLError_Set("No matching EGL frame buffer configuration");
        return context;
    }

    if(eglBindAPI(EGL_OPENGL_API) == EGL_FALSE)
    {
        MGLError_Set("Failed to eglBindAPI(EGL_OPENGL_API)");
        return context;
    }

    const EGLint context_attrib[] =
    {
        EGL_CONTEXT_MAJOR_VERSION_KHR,  4,
        EGL_CONTEXT_MINOR_VERSION_KHR,  6,
        EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR,    EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR,
        EGL_CONTEXT_FLAGS_KHR,          EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE_BIT_KHR,
        EGL_NONE
    };

    EGLContext context = eglCreateContext(display, config, EGL_NO_CONTEXT, context_attrib);
    if(context == EGL_NO_CONTEXT)
    {
        cerr << "Failed to eglCreateContext" << endl;
        EGLint error =  eglGetError();
        switch(error)
        {
        case EGL_BAD_CONFIG:
            MGLError_Set("config is not an EGL frame buffer configuration, or does not support the current rendering API");
            break;
        case EGL_BAD_ATTRIBUTE:
            MGLError_Set("attrib_list contains an invalid context attribute or if an attribute is not recognized or out of range");
            break;
        default:
            MGLError_Set("Unknown error");
            break;
        }
        return context;
    }

    if(eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, context) == EGL_FALSE)
    {
        MGLError_Set("Failed to eglMakeCurrent");
        return context;
    }

    GLenum err = glewInit();
    if (GLEW_OK != err)
    {
        MGLError_Set("Failed to glewInit");
        return context;
    }

    if(EGL_SUCCESS != eglGetError())
    {
		MGLError_Set("eglGetError != EGL_SUCCESS");
        return context;
    }

    context.display = (void *) display;
	context.window = nullptr;
	context.context = (void *) context;

	return context;
}

void DestroyGLContext(const GLContext & context) {
	if (!context.standalone) {
		return;
	}

	if (context.display) {

		if(eglMakeCurrent(context.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT) == EGL_FALSE)
		{
			MGLError_Set("Failed to eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT)");
		}

		if(eglDestroyContext(context.display, context.context) == EGL_FALSE)
		{
			MGLError_Set("Failed to eglDestroyContext(context.display, context.context)");
		}

		if(eglTerminate(display) == EGL_FALSE)
		{
			MGLError_Set("Failed to eglTerminate");
		}
	}
}

#endif
