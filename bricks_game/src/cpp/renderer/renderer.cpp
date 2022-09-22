//
// Copyright 2011 Tero Saarni
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include <stdint.h>
#include <unistd.h>
#include <vector>
#include <pthread.h>

#include <android/native_window.h> // requires ndk r5 or newer
#include <android/asset_manager.h>


#include <EGL/egl.h> // requires ndk r5 or newer

#include "engine.h"

#include "logger.h"
#include "renderer.h"

#define LOG_TAG "EglSample"


Renderer::Renderer()
    : _msg(MSG_NONE), _display(0), _surface(0), _context(0), _angle(0)
{
    LOG_INFO("Renderer instance created");
    pthread_mutex_init(&_mutex, 0);    
    return;
}

Renderer::~Renderer()
{
    LOG_INFO("Renderer instance destroyed");
    pthread_mutex_destroy(&_mutex);
    return;
}

void Renderer::start()
{
	data_retrieved_flag = 0;
    LOG_INFO("Creating renderer thread");
    pthread_create(&_threadId, 0, threadStartCallback, this);
    return;
}

void Renderer::stop()
{
	renderloopexit = true;
    LOG_INFO("Stopping renderer thread");
	// send message to render thread to stop rendering
    pthread_mutex_lock(&_mutex);
    _msg = MSG_RENDER_LOOP_EXIT;
    pthread_mutex_unlock(&_mutex);    

    pthread_join(_threadId, 0);
    LOG_INFO("Renderer thread stopped");

    return;
}

void Renderer::setTouchCoordinates(int x, int y)
{
	x_down = x;
	y_down = y;
	
	speed_controller(engine_width, engine_height, x_down, y_down);
	bar_position = bar_x_pos(engine_width, engine_height, x_down, y_down);
}

void Renderer::setWindow(ANativeWindow *window)
{
    // notify render thread that window has changed
    pthread_mutex_lock(&_mutex);
    _msg = MSG_WINDOW_SET;
    _window = window;
	pthread_mutex_unlock(&_mutex);
    return;
}


void Renderer::setAssetManager(AAssetManager* massetmanager)
{
	myassetManager = massetmanager;    
	//Prepare asset manager
	android_fopen_set_asset_manager(myassetManager);
	
	//Load 3d model
	GLint cha;
	//brick
	mbrick.LoadFile("obj/brick.obj");	
	stbi_set_flip_vertically_on_load(1);
	brick_image_data = stbi_load("obj/container.jpg", &brick_widt, &brick_heigh, &cha, 0);
	//bar
	mbar.LoadFile("obj/bar/bar.obj");
	stbi_set_flip_vertically_on_load(1);
	bar_image_data = stbi_load("obj/bar/bar.png", &bar_widt, &bar_heigh, &cha, 0);
	//sphere
	msphere.LoadFile("obj/sphere/sphere.obj");
	stbi_set_flip_vertically_on_load(1);
	sphere_image_data = stbi_load("obj/sphere/sphere.png", &sphere_widt, &sphere_heigh, &cha, 0);
	//speed
	mspeed.LoadFile("obj/speed/speedcontroller.obj");
	stbi_set_flip_vertically_on_load(1);
	speed_image_data = stbi_load("obj/speed/speed.jpg", &speed_widt, &speed_heigh, &cha, 0);
	
    //set the bricks to be rendered in the first bunch
	if(total_hit_bricks == 0)
		reset_hit_list(hit_list, bricks_to_be_rendered, total_hit_bricks);
	
    return;
}

void Renderer::renderLoop()
{
    bool renderingEnabled = true;
    
    LOG_INFO("renderLoop()");

    while (renderingEnabled) {
	LOG_INFO("In renderLoop()");
	if(!m_paused || resumed) //if we are not paused or we have just resumed from a pause
	{
        pthread_mutex_lock(&_mutex);

        // process incoming messages
        switch (_msg) {

            case MSG_WINDOW_SET:
			{
				initialize();
                resumed = false; //this is done here to ensure that the display is valid for the first frame to be rendered
				break;
			}
               
		   case MSG_RENDER_LOOP_EXIT:
                renderingEnabled = false;
                destroy();
                break;

            default:
                break;
        }
        _msg = MSG_NONE;
        
        if (_display) {
            //drawFrame();
			mrender();
            if (!eglSwapBuffers(_display, _surface)) 
                 LOG_ERROR("eglSwapBuffers() returned error %d", eglGetError());
            
        }
        
		score = m_score;
				
        pthread_mutex_unlock(&_mutex);
	  }
	  else if(m_paused == true && renderloopexit == true) //the game is paused and the user wants to use another app
	  {
		  renderingEnabled = false;
          destroy();
	  }
    };
    LOG_INFO("Render loop exits");
    
    return;
}

bool Renderer::initialize()
{
    const EGLint attribs[] = 
	{
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_BLUE_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_RED_SIZE, 8,
        EGL_NONE
    };
    EGLDisplay display;
	
    EGLConfig config;    
    EGLint numConfigs;
	
    EGLint format;
	
    EGLSurface surface;
    EGLContext context;
	
    EGLint width;
    EGLint height;
	
    GLfloat ratio;
    
		
    LOG_INFO("Initializing context");
	    
    if ((display = eglGetDisplay(EGL_DEFAULT_DISPLAY)) == EGL_NO_DISPLAY) {
        LOG_ERROR("eglGetDisplay() returned error %d", eglGetError());
        return false;
    }
    if (!eglInitialize(display, 0, 0)) {
        LOG_ERROR("eglInitialize() returned error %d", eglGetError());
        return false;
    }

    if (!eglChooseConfig(display, attribs, &config, 1, &numConfigs)) {
        LOG_ERROR("eglChooseConfig() returned error %d", eglGetError());
        destroy();
        return false;
    }

    if (!eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &format)) {
        LOG_ERROR("eglGetConfigAttrib() returned error %d", eglGetError());
        destroy();
        return false;
    }
		
    ANativeWindow_setBuffersGeometry(_window, 0, 0, format);

    if (!(surface = eglCreateWindowSurface(display, config, _window, 0))) {
        LOG_ERROR("eglCreateWindowSurface() returned error %d", eglGetError());
        destroy();
        return false;
    }
    const EGLint context_attribs[] = { EGL_CONTEXT_CLIENT_VERSION,2,EGL_NONE };
    if (!(context = eglCreateContext(display, config, 0, context_attribs))) {
        LOG_ERROR("eglCreateContext() returned error %d", eglGetError());
        destroy();
        return false;
    }
    
    if (!eglMakeCurrent(display, surface, surface, context)) {
        LOG_ERROR("eglMakeCurrent() returned error %d", eglGetError());
        destroy();
        return false;
    }

    if (!eglQuerySurface(display, surface, EGL_WIDTH, &width) ||
        !eglQuerySurface(display, surface, EGL_HEIGHT, &height)) {
        LOG_ERROR("eglQuerySurface() returned error %d", eglGetError());
        destroy();
        return false;
    }

    _display = display;
    _surface = surface;
    _context = context;

    	
	glDisable(GL_DITHER);
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
    glClearColor(0, 1, 0, 0);
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
	
	glViewport(0, 0, width, height);
	
	engine_width = width;
	engine_height = height;
	
	//bar default position
	default_user_x_touchPoint = (float)(engine_width / 2.0);
		
    return true;
}

void Renderer::destroy() {
    LOG_INFO("Destroying context");

    eglMakeCurrent(_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroyContext(_display, _context);
    eglDestroySurface(_display, _surface);
    eglTerminate(_display);
    
    _display = EGL_NO_DISPLAY;
    _surface = EGL_NO_SURFACE;
    _context = EGL_NO_CONTEXT;

    return;
}

void* Renderer::threadStartCallback(void *myself)
{
    Renderer *renderer = (Renderer*)myself;
	renderer->resumed = true;
    renderer->renderLoop();
    pthread_exit(0);
    
    return 0;
}


void Renderer::setpausestate(bool paused)
{
	m_paused = paused;
}
