/*
 -----------------------------------------------------------------------------
 This source file is part of OGRE
 (Object-oriented Graphics Rendering Engine)
 For the latest info, see http://www.ogre3d.org/

 Copyright (c) 2000-2012 Torus Knot Software Ltd

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 -----------------------------------------------------------------------------
 */

#include "OgreQnxEGLContext.h"
#include "OgreGLES2RenderSystem.h"
#include "OgreRoot.h"


namespace Ogre {

static EGLint attrib_list[] = { EGL_ALPHA_SIZE, 8, EGL_RED_SIZE, 8,
		EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_DEPTH_SIZE, 24, EGL_STENCIL_SIZE, 8, EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_NONE };

static EGLint attributes[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };

QnxEGLContext::QnxEGLContext(const QnxEGLSupport *glsupport) :
		mGLSupport(glsupport), mBackingWidth(0), mBackingHeight(0) {

	//create  a glContext
	int num_configs;

	mGLDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY );
	if (mGLDisplay == EGL_NO_DISPLAY ) {

		//error
	}

	int rc = eglInitialize(mGLDisplay, NULL, NULL);
	if (rc != EGL_TRUE) {
		//error
	}

	rc = eglBindAPI(EGL_OPENGL_ES_API);

	if (rc != EGL_TRUE) {
		//log error
		return;
	}

	if (!::eglChooseConfig(mGLDisplay, attrib_list, &mEglConf, 1,
			&num_configs)) {
		//log error
		return;
	}

	mContext = eglCreateContext(mGLDisplay, mEglConf, NULL, attributes);
	if (mContext == NULL) {

		//log error
	}
}

void QnxEGLContext::createGlSurface(EGLNativeWindowType nativeWindow) {
	mGlSurf = eglCreateWindowSurface(mGLDisplay, mEglConf,
			(EGLNativeWindowType) nativeWindow, NULL);
	if (mGlSurf == EGL_NO_SURFACE ) {

		//log error;
		LogManager::getSingleton().logMessage(
				"\t QnxEGLContext::createGlSurface failed");
		return;
	}
	eglSwapInterval(mGLDisplay, 1);

}

QnxEGLContext::~QnxEGLContext() {
	GLES2RenderSystem *rs =
			static_cast<GLES2RenderSystem*>(Root::getSingleton().getRenderSystem());

	rs->_unregisterContext(this);

	if (mGlSurf != EGL_NO_SURFACE ) {
		eglDestroySurface(mGLDisplay, mEglConf);
	}

	if (mContext == eglGetCurrentContext()) {
		endCurrent();
	}

	if (mContext != NULL) {
		eglDestroyContext(mGLDisplay, mContext);
	}
	eglTerminate(mGLDisplay);
}

void QnxEGLContext::setCurrent() {

	eglMakeCurrent(mGLDisplay, mGlSurf, mGlSurf, mContext);
	GL_CHECK_ERROR
}

void QnxEGLContext::endCurrent() {

	LogManager::getSingleton().logMessage(
			"\t QnxEGLContext::endCurrent() called");
	eglMakeCurrent(mGLDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT );
	GL_CHECK_ERROR
}

GLES2Context* QnxEGLContext::clone() const {
	return new QnxEGLContext(mGLSupport);
}

}
