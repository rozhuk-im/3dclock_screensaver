/*
 *  Copyright (c) 2013 - 2018 Naezzhy Petr(Наезжий Пётр) <petn@mail.ru>
 *  Copyright (c) 2020 Rozhuk Ivan <rozhuk.im@gmail.com>
 *  All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* 
 * File:   glxwindow.h
 * Author: Naezzhy Petr(Наезжий Пётр) <petn@mail.ru>
 *
 * Created on 7 апреля 2018 г., 11:36
 */

#ifndef GLXWINDOW_H
#define GLXWINDOW_H


#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include <errno.h>

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xfixes.h>

#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glu.h>
#include <GL/glext.h>


typedef struct window_state_s {
	uint32_t	width;
	uint32_t	height;
} wnd_state_t, *wnd_state_p;

typedef struct x_mouse_curs_pos_s {
	int32_t		root_x;
	int32_t		root_y;
	int32_t		win_x;
	int32_t		win_y;
} mcur_pos_t, *mcur_pos_p;


#define GLX_WND_REDRAW_F_INIT		(((uint32_t)1) << 0)
#define GLX_WND_REDRAW_F_DESTROY	(((uint32_t)1) << 1)
#define GLX_WND_REDRAW_F_RESIZE		(((uint32_t)1) << 2)

typedef struct gl_x_window_s *glx_wnd_p;
typedef void (*glx_wnd_redraw_cb)(glx_wnd_p glx_wnd, const uint32_t flags,
    const wnd_state_p ws, const mcur_pos_p mcur_pos, void *udata);
typedef void (*glx_wnd_events_cb)(glx_wnd_p glx_wnd, const XEvent *event,
    void *udata);


typedef struct gl_x_window_s {
	glx_wnd_redraw_cb	redraw_cb;
	glx_wnd_events_cb	events_cb;
	void			*udata;

	Display			*display;
	Window			window;
	int			screen;

	XVisualInfo		*vi;
	XSetWindowAttributes	swa;
	GLXContext		glc;
	Atom			wm_delete;

	wnd_state_t		ws;
	mcur_pos_t		mcur_pos;
} glx_wnd_t;


static PFNGLXCREATECONTEXTATTRIBSARBPROC glXCreateContextAttribs = NULL;



static inline int
get_screen_resolution(uint32_t *width, uint32_t *height) {
	Display *display = NULL;
	Screen *screen = NULL;

	if (width == NULL && height == NULL)
		return (EINVAL);

	display = XOpenDisplay(NULL);
	if (NULL == display)
		return (-1);
	screen = DefaultScreenOfDisplay(display);
	if (NULL == screen) {
		XCloseDisplay(display);
		return (-1);
	}
	if (NULL != height) {
		(*height) = (uint32_t)screen->height;
	}
	if (NULL != width) {
		(*width) = (uint32_t)screen->width;
	}
	XCloseDisplay(display);
	return (0);
}


static inline void
glx_wnd_destroy(glx_wnd_p glx_wnd) {

	if (NULL == glx_wnd)
		return;

	if (glx_wnd->glc) {
		glx_wnd->redraw_cb(glx_wnd,
		    GLX_WND_REDRAW_F_DESTROY, &glx_wnd->ws,
		    &glx_wnd->mcur_pos, glx_wnd->udata);
		if (!glXMakeCurrent(glx_wnd->display, None, NULL)) {
			fprintf(stderr, "Could not release drawing context.\n");
		}
		glXDestroyContext(glx_wnd->display, glx_wnd->glc);
	}
	if (glx_wnd->vi) {
		XFree(glx_wnd->vi);
	}
	if (0 != glx_wnd->window) {
		XDestroyWindow(glx_wnd->display, glx_wnd->window);
	}
	if (0 != glx_wnd->swa.colormap) {
		XFreeColormap(glx_wnd->display, glx_wnd->swa.colormap); 
	}
	if (NULL != glx_wnd->display) {
		XCloseDisplay(glx_wnd->display);
	}
	memset(glx_wnd, 0x00, sizeof(glx_wnd_t));
}

static inline int
glx_wnd_create(uint32_t width, uint32_t height, const char *caption, 
    glx_wnd_redraw_cb redraw_cb, glx_wnd_events_cb events_cb, void *udata,
    glx_wnd_p glx_wnd) {
	int error;
	int glmaj, glmin;
	int i, fbc_cnt, smpl_buf, smpl_cnt, bidx, bsmpl_cnt;
	GLXFBConfig *fbc;
	Window rootWindow = 0;
	GLint attr_legacy[] = {
		GLX_RGBA,
		GLX_DOUBLEBUFFER, True,
		GLX_DEPTH_SIZE, 24,
		None
	};
	GLint attr_base[] = {
		GLX_X_RENDERABLE, True,
		GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
		GLX_RENDER_TYPE, GLX_RGBA_BIT,
		GLX_X_VISUAL_TYPE, GLX_TRUE_COLOR,
		GLX_RED_SIZE, 8,
		GLX_GREEN_SIZE, 8,
		GLX_BLUE_SIZE, 8,
		GLX_ALPHA_SIZE, 8,
		GLX_DEPTH_SIZE, 24,
		GLX_STENCIL_SIZE, 8,
		None
	};
	GLint attr_modern[] = {
		GLX_CONTEXT_MAJOR_VERSION_ARB, 2,
		GLX_CONTEXT_MINOR_VERSION_ARB, 1,
		None
	};


	if (((0 == width || 0 == height) && width != height) ||
	    NULL == redraw_cb || NULL == events_cb || NULL == glx_wnd)
		return (EINVAL);

	if (0 == width && 0 == height) {
		error = get_screen_resolution(&width, &height);
		if (0 != error)
			return (error);
	}
	memset(glx_wnd, 0x00, sizeof(glx_wnd_t));
	glx_wnd->ws.width = width;
	glx_wnd->ws.height = height;
	glx_wnd->redraw_cb = redraw_cb;
	glx_wnd->events_cb = events_cb;
	glx_wnd->udata = udata;

	glx_wnd->display = XOpenDisplay(NULL);
	if (NULL == glx_wnd->display) {
		fprintf(stderr, "Cannot open display.\n");
		return (-1);
	}
	rootWindow = DefaultRootWindow(glx_wnd->display);
	if (0 == rootWindow) {
		fprintf(stderr, "Cannot get root window XID.\n");
		goto err_out;
	}
	glx_wnd->screen = DefaultScreen(glx_wnd->display);

	/* FBConfigs were added in GLX version 1.3. */
	if (!glXQueryVersion(glx_wnd->display, &glmaj, &glmin)) {
		fprintf(stderr, "glXQueryVersion error.\n");
		goto err_out;
	}
	if (0 == glmaj) {
		fprintf(stderr, "Invalid GLX version.\n");
		goto err_out;
	}

	if (1 == glmaj && 3 > glmin) {
legacy_init:
		glx_wnd->vi = glXChooseVisual(glx_wnd->display,
		    glx_wnd->screen, attr_legacy);
		glx_wnd->glc = glXCreateContext(glx_wnd->display,
		    glx_wnd->vi, NULL, GL_TRUE);
	} else { /* FBconfig supported by video subsystem. */
		if (NULL == glXCreateContextAttribs) {
			glXCreateContextAttribs =
			    (PFNGLXCREATECONTEXTATTRIBSARBPROC)glXGetProcAddress(
				(GLubyte*)"glXCreateContextAttribsARB");
		}
		if (NULL == glXCreateContextAttribs)
			goto legacy_init;

		fbc = glXChooseFBConfig(glx_wnd->display, glx_wnd->screen,
		    attr_base, &fbc_cnt);
		if (NULL == fbc || 0 == fbc_cnt) {
			fprintf(stderr, "Failed to retrieve a framebuffer config.\n" );
			goto err_out;
		}
		/* Looking for best visual ID from frame buffer object. */
		for (i = 0, bidx = -1, bsmpl_cnt = 0; i < fbc_cnt; i ++) {
			glXGetFBConfigAttrib(glx_wnd->display, fbc[i],
			    GLX_SAMPLE_BUFFERS, &smpl_buf);
			glXGetFBConfigAttrib(glx_wnd->display, fbc[i],
			    GLX_SAMPLES, &smpl_cnt);
			if (bidx < 0 ||
			    (0 < smpl_buf && smpl_cnt > bsmpl_cnt)) {
				bidx = i;
				bsmpl_cnt = smpl_cnt;
			}
		}
		glx_wnd->vi = glXGetVisualFromFBConfig(glx_wnd->display,
		    fbc[bidx]);
		glx_wnd->glc = glXCreateContextAttribs(glx_wnd->display,
		    fbc[bidx], 0, True, attr_modern);
		XFree(fbc);
	}
	if (NULL == glx_wnd->vi) {
		fprintf(stderr, "No appropriate visual found.\n");
		goto err_out;
	}
	if (NULL == glx_wnd->glc) {
		fprintf(stderr, "Cannot create OpenGL context.\n");
		goto err_out;
	}

	glx_wnd->swa.colormap = XCreateColormap(glx_wnd->display,
	    rootWindow, glx_wnd->vi->visual, AllocNone);
	if (0 == glx_wnd->swa.colormap) {
		fprintf(stderr, "Cannot create colormap.\n");
		goto err_out;
	}
	glx_wnd->swa.event_mask = (ExposureMask | KeyPressMask |
	    StructureNotifyMask | ButtonPressMask);

	glx_wnd->window = XCreateWindow(
	    glx_wnd->display,
	    rootWindow,
	    0,
	    0,
	    width,
	    height,
	    0,
	    glx_wnd->vi->depth,
	    InputOutput,
	    glx_wnd->vi->visual,
	    (CWColormap | CWEventMask | CWColormap),
	    &glx_wnd->swa);
	if (0 == glx_wnd->window) {
		fprintf(stderr, "Cannot create window.\n");
		goto err_out;
	}

#if 0
	XSetStandardProperties(glx_wnd->display, glx_wnd->window, caption,
	    caption, None, NULL, 0, NULL);
	XMapRaised(glx_wnd->display, glx_wnd->window);
#endif

	glx_wnd->wm_delete = XInternAtom(glx_wnd->display,
	    "WM_DELETE_WINDOW", True);
	XSetWMProtocols(glx_wnd->display, glx_wnd->window,
	    &glx_wnd->wm_delete, 1);

	if (caption != NULL) {
		XStoreName(glx_wnd->display, glx_wnd->window, caption);
	}

	XMapWindow(glx_wnd->display, glx_wnd->window);

	XSync(glx_wnd->display, False);

	glXMakeCurrent(glx_wnd->display, glx_wnd->window, glx_wnd->glc);

	if (!glXIsDirect(glx_wnd->display, glx_wnd->glc)) {
		fprintf(stderr, "Direct Rendering is not supported.\n");
	}

#if 0
	XFlush(glx_wnd->display);
	XGrabKeyboard(glx_wnd->display, glx_wnd->window, True,
	    GrabModeAsync, GrabModeAsync, CurrentTime);
	XGrabPointer(glx_wnd->display, glx_wnd->window, True,
	    ButtonPressMask, GrabModeAsync, GrabModeAsync,
	    glx_wnd->window, None, CurrentTime);
#endif
	glx_wnd->redraw_cb(glx_wnd,
	    (GLX_WND_REDRAW_F_INIT | GLX_WND_REDRAW_F_RESIZE),
	    &glx_wnd->ws, &glx_wnd->mcur_pos, glx_wnd->udata);
	glXSwapBuffers(glx_wnd->display, glx_wnd->window);

	return (0);

err_out:

	glx_wnd_destroy(glx_wnd);

	return (-1);
}


/* Updating window events, running callbacks.
 * Returns 0 on success. */
static inline int
glx_wnd_update_window(glx_wnd_p glx_wnd) {
	XEvent event;
	Window returnedWindow;
	uint32_t mask;

	if (NULL == glx_wnd ||
	    NULL == glx_wnd->display ||
	    0 == glx_wnd->window ||
	    NULL == glx_wnd->glc)
		return (EINVAL);

	/* Handle the events in the queue. */
	if (XPending(glx_wnd->display) <= 0) {
		/* Simple redraw GL window. */
		/* Getting mouse cursor position. */
		XQueryPointer(glx_wnd->display, glx_wnd->window,
		    &returnedWindow, &returnedWindow,
		    &glx_wnd->mcur_pos.root_x, &glx_wnd->mcur_pos.root_y,
		    &glx_wnd->mcur_pos.win_x, &glx_wnd->mcur_pos.win_y,
		    &mask);
		glx_wnd->redraw_cb(glx_wnd, 0, &glx_wnd->ws,
		    &glx_wnd->mcur_pos, glx_wnd->udata);
		glXSwapBuffers(glx_wnd->display, glx_wnd->window);
		return (0);
	}

	XNextEvent(glx_wnd->display, &event);
	if (glx_wnd->events_cb) {
		glx_wnd->events_cb(glx_wnd, &event, glx_wnd->udata);
	}

	switch (event.type) {
	case Expose:
		if (event.xexpose.count != 0)
			break;
		glx_wnd->redraw_cb(glx_wnd, 0, &glx_wnd->ws,
		    &glx_wnd->mcur_pos, glx_wnd->udata);
		glXSwapBuffers(glx_wnd->display, glx_wnd->window);
		break;
	case ConfigureNotify:
		/* Set resize flag only if window size was changed. */
		if (((uint32_t)event.xconfigure.width != glx_wnd->ws.width) || 
		    ((uint32_t)event.xconfigure.height != glx_wnd->ws.height)) {
			glx_wnd->ws.width = (uint32_t)event.xconfigure.width;
			glx_wnd->ws.height = (uint32_t)event.xconfigure.height;
			glx_wnd->redraw_cb(glx_wnd, GLX_WND_REDRAW_F_RESIZE,
			    &glx_wnd->ws, &glx_wnd->mcur_pos, glx_wnd->udata);
			glXSwapBuffers(glx_wnd->display, glx_wnd->window);
		}
		break;
	case ClientMessage:
		if ((Atom)event.xclient.data.l[0] == glx_wnd->wm_delete) {
			glx_wnd_destroy(glx_wnd);
			return (-1);
		}
	case ButtonPress:
	case ButtonRelease:
	case KeyRelease:
		break;
	case KeyPress:
		if (glx_wnd->events_cb)
			break;
		if (XLookupKeysym(&event.xkey, 0) == XK_Escape) {
			glx_wnd_destroy(glx_wnd);
			return (-1);
		}
		break;
	}

	return (0);
}


static inline int
glx_wnd_hide_cursor(glx_wnd_p glx_wnd) {

	if (NULL == glx_wnd || NULL == glx_wnd->display || 0 == glx_wnd->window)
		return (EINVAL);

	XFixesHideCursor(glx_wnd->display, glx_wnd->window);
	XFlush(glx_wnd->display);

	return (0);
}

static inline int
glx_wnd_show_cursor(glx_wnd_p glx_wnd) {

	if (NULL == glx_wnd || NULL == glx_wnd->display || 0 == glx_wnd->window)
		return (EINVAL);

	XFixesShowCursor(glx_wnd->display, glx_wnd->window);
	XFlush(glx_wnd->display);

	return (0);
}

/* Set window to fullscreen popup mode without borders.
 * Returns 0 on success. */
static inline int
glx_wnd_set_window_fullscreen_popup(glx_wnd_p glx_wnd) {
	int error;
	XEvent e;
	uint32_t width, height;
	Atom net_wm_state, net_wm_fullscreen_monitors, net_wm_state_fullscreen;

	if (NULL == glx_wnd ||
	    NULL == glx_wnd->display ||
	    0 == glx_wnd->window ||
	    NULL == glx_wnd->glc)
		return (EINVAL);

	error = get_screen_resolution(&width, &height);
	if (0 != error) {
		fprintf(stderr, "Could not get screen resolution.\n");
		return (error);
	}
	net_wm_state = XInternAtom(glx_wnd->display, "_NET_WM_STATE", 0);
	net_wm_fullscreen_monitors = XInternAtom(glx_wnd->display,
	    "_NET_WM_FULLSCREEN_MONITORS", 0);
	net_wm_state_fullscreen = XInternAtom(glx_wnd->display,
	    "_NET_WM_STATE_FULLSCREEN", 0);

	e.xany.type = ClientMessage;
	e.xany.window = glx_wnd->window;
	e.xclient.message_type = net_wm_fullscreen_monitors;
	e.xclient.format = 32;
	e.xclient.data.l[0] = 0;
	e.xclient.data.l[1] = 0;
	e.xclient.data.l[2] = width;
	e.xclient.data.l[3] = height;
	e.xclient.data.l[4] = 0;
	XSendEvent(glx_wnd->display,
	    RootWindow(glx_wnd->display, glx_wnd->screen), 0,
	    (SubstructureNotifyMask | SubstructureRedirectMask), &e);

	e.xany.type = ClientMessage;
	e.xany.window = glx_wnd->window;
	e.xclient.message_type = net_wm_state;
	e.xclient.format = 32;
	e.xclient.data.l[0] = 1; /* Flag to set or unset fullscreen state. */
	e.xclient.data.l[1] = (long)net_wm_state_fullscreen;
	e.xclient.data.l[2] = 0;
	e.xclient.data.l[3] = 0;
	e.xclient.data.l[4] = 0;
	XSendEvent(glx_wnd->display,
	    RootWindow(glx_wnd->display, glx_wnd->screen), 0,
	    (SubstructureNotifyMask | SubstructureRedirectMask), &e);

	XFlush(glx_wnd->display);

	return (0);
}

/* Unset window from fullscreen popup mode to normal state.
 * If width and height equal zero then window dimentions sets to fullscreen.
 * Returns 0 on success. */
static inline int
glx_wnd_unset_window_fullscreen_popup(glx_wnd_p glx_wnd, uint32_t width,
    uint32_t height) {
	int error;
	XEvent e;
	Atom net_wm_state, net_wm_state_fullscreen;

	if (NULL == glx_wnd ||
	    NULL == glx_wnd->display ||
	    0 == glx_wnd->window ||
	    NULL == glx_wnd->glc ||
	    ((0 == width || 0 == height) && width != height))
		return (EINVAL);

	if (0 == width && 0 == height) {
		error = get_screen_resolution(&width, &height);
		if (0 != error)
			return (error);
	}

	net_wm_state = XInternAtom(glx_wnd->display, "_NET_WM_STATE", 0);
	net_wm_state_fullscreen = XInternAtom(glx_wnd->display,
	    "_NET_WM_STATE_FULLSCREEN", 0);

	e.xany.type = ClientMessage;
	e.xany.window = glx_wnd->window;
	e.xclient.message_type = net_wm_state;
	e.xclient.format = 32;
	e.xclient.data.l[0] = 0; /* Flag to set or unset fullscreen state. */
	e.xclient.data.l[1] = (long)net_wm_state_fullscreen;
	e.xclient.data.l[2] = 0;
	e.xclient.data.l[3] = 0;
	e.xclient.data.l[4] = 0;
	XSendEvent(glx_wnd->display,
	    RootWindow(glx_wnd->display, glx_wnd->screen), 0,
	    (SubstructureNotifyMask | SubstructureRedirectMask), &e);

	XMoveResizeWindow(glx_wnd->display, glx_wnd->window, 0, 0,
	    width, height);
	XMapRaised(glx_wnd->display, glx_wnd->window);

	XFlush(glx_wnd->display);

	return (0);
}


#endif /* GLXWINDOW_H */
