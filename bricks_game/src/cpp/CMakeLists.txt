					   
cmake_minimum_required(VERSION 3.4.1)

include_directories("${CMAKE_SOURCE_DIR}"
					"${CMAKE_SOURCE_DIR}/jni"
					"${CMAKE_SOURCE_DIR}/renderer"
					"${CMAKE_SOURCE_DIR}/vendor"
					"${CMAKE_SOURCE_DIR}/vendor/glm"
					"${CMAKE_SOURCE_DIR}/stb_image"
					"${CMAKE_SOURCE_DIR}/androidfopen"
					
# now build app's shared lib
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -Wall")

add_library(nativecodelib SHARED
            jni/jniapi.cpp
			renderer/renderer.cpp
			stb_image/stb_image.cpp
			androidfopen/android_fopen.c
			)
			
# add lib dependencies
target_link_libraries(nativecodelib
                      android
                      log 
                      EGL
					  GLESv1_CM
                      GLESv2
					  )
					   