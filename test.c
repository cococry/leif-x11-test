
// Include GLAD headers
#include "vendor/glad/include/glad/glad.h"
#include <stdio.h>
#include <stdlib.h>
#include <xcb/xcb.h>
#include <xcb/glx.h>
#include <GL/glx.h>
#include <leif/leif.h>

// GLX context attributes
#define GLX_CONTEXT_MAJOR_VERSION_ARB 0x2091
#define GLX_CONTEXT_MINOR_VERSION_ARB 0x2092
#define GLX_CONTEXT_PROFILE_MASK_ARB 0x9126
#define GLX_CONTEXT_CORE_PROFILE_BIT_ARB 0x00000001

typedef GLXContext (*glXCreateContextAttribsARBProc)(Display*, GLXFBConfig, GLXContext, Bool, const int*);

xcb_screen_t* get_screen(xcb_connection_t* connection, int screen_num) {
  xcb_screen_iterator_t iter = xcb_setup_roots_iterator(xcb_get_setup(connection));
  for (int i = 0; i < screen_num; ++i) {
    xcb_screen_next(&iter);
  }
  return iter.data;
}

xcb_window_t create_window(xcb_connection_t *connection, xcb_screen_t *screen, int width, int height) {
  xcb_window_t window = xcb_generate_id(connection);

  uint32_t value_list[] = {
    screen->black_pixel,
    XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_KEY_PRESS
  };

  xcb_create_window(connection,
                    XCB_COPY_FROM_PARENT,
                    window,
                    screen->root,
                    0, 0, width, height,
                    0,
                    XCB_WINDOW_CLASS_INPUT_OUTPUT,
                    screen->root_visual,
                    XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK,
                    value_list);

  xcb_map_window(connection, window);
  xcb_flush(connection);

  return window;
}

int main() {
  int screen_num;
  xcb_connection_t *connection = xcb_connect(NULL, &screen_num);
  if (xcb_connection_has_error(connection)) {
    fprintf(stderr, "Unable to connect to X server\n");
    return -1;
  }

  Display *display = XOpenDisplay(NULL);
  if (!display) {
    fprintf(stderr, "Unable to open X display\n");
    return -1;
  }

  xcb_screen_t *screen = get_screen(connection, screen_num);

  xcb_window_t window1 = create_window(connection, screen, 800, 600);
  xcb_window_t window2 = create_window(connection, screen, 800, 600);
  xcb_window_t window3 = create_window(connection, screen, 800, 600);

  static int visual_attribs[] = {
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
    GLX_DOUBLEBUFFER, True,
    None
  };

  int fbcount;
  GLXFBConfig* fbc = glXChooseFBConfig(display, screen_num, visual_attribs, &fbcount);
  if (!fbc) {
    fprintf(stderr, "Failed to retrieve a framebuffer config\n");
    return -1;
  }

  int best_fbc = -1, best_num_samp = -1;
  for (int i = 0; i < fbcount; i++) {
    XVisualInfo *vi = glXGetVisualFromFBConfig(display, fbc[i]);
    if (vi) {
      int samp_buf, samples;
      glXGetFBConfigAttrib(display, fbc[i], GLX_SAMPLE_BUFFERS, &samp_buf);
      glXGetFBConfigAttrib(display, fbc[i], GLX_SAMPLES, &samples);

      if (best_fbc < 0 || (samp_buf && samples > best_num_samp)) {
        best_fbc = i;
        best_num_samp = samples;
      }
    }
    XFree(vi);
  }

  GLXFBConfig bestFbc = fbc[best_fbc];
  XFree(fbc);

  XVisualInfo *vi = glXGetVisualFromFBConfig(display, bestFbc);
  if (!vi) {
    fprintf(stderr, "No appropriate visual found\n");
    return -1;
  }

  glXCreateContextAttribsARBProc glXCreateContextAttribsARB = (glXCreateContextAttribsARBProc)
    glXGetProcAddressARB((const GLubyte*)"glXCreateContextAttribsARB");

  if (!glXCreateContextAttribsARB) {
    fprintf(stderr, "glXCreateContextAttribsARB not found\n");
    return -1;
  }

  static int context_attribs[] = {
    GLX_CONTEXT_MAJOR_VERSION_ARB, 4,
    GLX_CONTEXT_MINOR_VERSION_ARB, 5,
    GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
    None
  };

  GLXContext context = glXCreateContextAttribsARB(display, bestFbc, NULL, True, context_attribs);
  if (!context) {
    fprintf(stderr, "Failed to create an OpenGL context\n");
    return -1;
  }

  GLXContext shared_context = glXCreateContextAttribsARB(display, bestFbc, context, True, context_attribs);
  if (!shared_context) {
    fprintf(stderr, "Failed to create a shared OpenGL context\n");
    return -1;
  }

  glXMakeCurrent(display, window1, context);

  // Initialize GLAD
  if (!gladLoadGLLoader((GLADloadproc)glXGetProcAddressARB)) {
    fprintf(stderr, "Failed to initialize GLAD\n");
    return -1;
  }

  LfState state = lf_init_x11(800, 600);


  glXMakeCurrent(display, window2, context);
  LfState state2 = lf_init_x11(800, 600);

  glXMakeCurrent(display, window3, context);
  LfState state3 = lf_init_x11(800, 600);

  // Main event loop
  while (1) {
    xcb_generic_event_t *event;
    while ((event = xcb_poll_for_event(connection))) {
      switch (event->response_type & ~0x80) {
        case XCB_EXPOSE:
          glXMakeCurrent(display, window1, context);
          glClear(GL_COLOR_BUFFER_BIT);

          lf_begin(&state);
          lf_text(&state, "Hello");
          lf_next_line(&state);
          lf_text(&state, "Hello");
          lf_end(&state);
          glXSwapBuffers(display, window1);


          glXMakeCurrent(display, window2, context);
          glClear(GL_COLOR_BUFFER_BIT);

          lf_begin(&state2);
          lf_text(&state2, "Lets go");
          lf_next_line(&state2);
          lf_text(&state2, "Hello");
          lf_end(&state2);
          glXSwapBuffers(display, window2);


          glXMakeCurrent(display, window3, context);
          glClear(GL_COLOR_BUFFER_BIT);

          lf_begin(&state3);
          lf_text(&state3, "Lets go 2");
          lf_next_line(&state3);
          lf_text(&state3, "Hello");
          lf_end(&state3);
          glXSwapBuffers(display, window3);
          break;
      }
      free(event);
    }
  }

  glXDestroyContext(display, shared_context);
  glXDestroyContext(display, context);
  xcb_disconnect(connection);
  XCloseDisplay(display);
  return 0;
}

