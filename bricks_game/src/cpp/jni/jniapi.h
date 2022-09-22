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

#ifndef JNIAPI_H
#define JNIAPI_H

extern "C" {
    JNIEXPORT void JNICALL Java_com_godfrey_jni_GameCore_nativeOnStart(JNIEnv* jenv, jobject obj);
    JNIEXPORT void JNICALL Java_com_godfrey_jni_GameCore_nativeOnResume(JNIEnv* jenv, jobject obj);
    JNIEXPORT void JNICALL Java_com_godfrey_jni_GameCore_nativeOnPause(JNIEnv* jenv, jobject obj);
    JNIEXPORT void JNICALL Java_com_godfrey_jni_GameCore_nativeOnStop(JNIEnv* jenv, jobject obj);
	JNIEXPORT void JNICALL Java_com_godfrey_jni_GameCore_nativeOnInput(JNIEnv* jenv, jclass cls, jint x, jint y);
    JNIEXPORT void JNICALL Java_com_godfrey_jni_GameCore_nativeSetSurface(JNIEnv* jenv, jobject obj, jobject surface);
	JNIEXPORT void JNICALL Java_com_godfrey_jni_GameCore_nativeSetAssetManager(JNIEnv* jenv, jobject obj, jobject assetManager);
	JNIEXPORT void JNICALL Java_com_godfrey_jni_GameCore_nativeScoreUpdater(JNIEnv* jenv, jobject obj);
	JNIEXPORT void JNICALL Java_com_godfrey_jni_GameCore_nativePause(JNIEnv* jenv, jobject obj, jboolean pausestate);
};

#endif // JNIAPI_H
