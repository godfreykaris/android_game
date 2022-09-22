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


//This document has been modified for particular reasons by Wambugu Godfrey Kariuki 2022

#include <stdint.h>
#include <jni.h>
#include <android/native_window.h> // requires ndk r5 or newer
#include <android/native_window_jni.h> // requires ndk r5 or newer
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>

#include "jniapi.h"
#include "logger.h"
#include "renderer.h"
#include <pthread.h>
#include <stdlib.h>
#include <jni.h>

#define LOG_TAG "EglSample"


static ANativeWindow *window = 0;
static AAssetManager *assetManager = 0;
static Renderer *renderer = 0;

static jclass cls;
static jmethodID javaMethodRef;

int previous_score = 0;

bool rendererstopped = false;

int native_score;

JNIEXPORT void JNICALL Java_com_godfrey_jni_GameCore_nativeOnStart(JNIEnv* jenv, jobject obj)
{
    LOG_INFO("nativeOnStart");
    renderer = new Renderer();
    return;
}

JNIEXPORT void JNICALL Java_com_godfrey_jni_GameCore_nativeOnResume(JNIEnv* jenv, jobject obj)
{
    LOG_INFO("nativeOnResume");
    renderer->start();
	rendererstopped = false;
    return;
}

JNIEXPORT void JNICALL Java_com_godfrey_jni_GameCore_nativeOnPause(JNIEnv* jenv, jobject obj)
{
    LOG_INFO("nativeOnPause");
    renderer->stop();
	rendererstopped = true;
    return;
}

JNIEXPORT void JNICALL Java_com_godfrey_jni_GameCore_nativeOnStop(JNIEnv* jenv, jobject obj)
{
    LOG_INFO("nativeOnStop");
    delete renderer;
    renderer = 0;
    return;
}

JNIEXPORT void JNICALL Java_com_godfrey_jni_GameCore_nativeOnInput(JNIEnv* jenv, jclass cls, jint x, jint y)
{
	renderer->setTouchCoordinates(x,y);
}


JNIEXPORT void JNICALL Java_com_godfrey_jni_GameCore_nativeSetSurface(JNIEnv* jenv, jobject obj, jobject surface)
{
    if (surface != 0) {
        window = ANativeWindow_fromSurface(jenv, surface);
		LOG_INFO("Got window %p", window);
		renderer->setWindow(window);
		
    } else {
        LOG_INFO("Releasing window");
        ANativeWindow_release(window);
    }

    return;
}

JNIEXPORT void JNICALL Java_com_godfrey_jni_GameCore_nativeSetAssetManager(JNIEnv* jenv, jobject obj, jobject _assetManager)
{
	if(_assetManager != 0)
	{
		assetManager =  AAssetManager_fromJava(jenv, _assetManager);
        LOG_INFO("Got AssetManager %p", assetManager);
		renderer->setAssetManager(assetManager);
	}
	else
	{
		LOG_INFO("Cannot get AssetManager %p", assetManager);
	}	
        
		
    return;
}

static void* scoreUpdater(void* param)
{   
	// if the score has not changed, no need to updated it in java, just exit the function
	if(previous_score == native_score)
		return 0;
	
	previous_score = native_score;
	
	// the java native environment for our application
	JNIEnv *jenv = (JNIEnv*)param;
	
    jclass dataClass = jenv->FindClass("com/godfrey/jni/GameCore");
    if (jenv->ExceptionCheck()) 
	{
      LOG_INFO("Exception");
    }

    cls = (jclass) jenv->NewGlobalRef(dataClass);
    if (jenv->ExceptionCheck()) 
	{
       LOG_INFO("Exception");
    }

    javaMethodRef = jenv->GetStaticMethodID(cls, "setScore", "(I)Z");
		        
    if (jenv->ExceptionCheck()) 
	{
       LOG_INFO("Exception");
    }
    
	bool success = true;
	
	success = jenv->CallStaticBooleanMethod(cls, javaMethodRef, native_score);
			
	return 0;
        
}

JNIEXPORT void JNICALL Java_com_godfrey_jni_GameCore_nativeScoreUpdater(JNIEnv* jenv, jobject obj)
{	
	// we check if the renderer object is destroyed before we retrieve the score from it
	if(rendererstopped == false)
		native_score = renderer->score;
	// the score updater function
	scoreUpdater(jenv);
	
	return;
}

JNIEXPORT void JNICALL Java_com_godfrey_jni_GameCore_nativePause(JNIEnv* jenv, jobject obj, jboolean pausestate)
{	
	// set the pause state that will be used in the renderer loop
	renderer->setpausestate(pausestate);	
	return;
}

