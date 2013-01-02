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

#include "OgreRoot.h"
#include "OgreWindowEventUtilities.h"
#include "OgreGLES2RenderSystem.h"
#include "OgreGLES2PixelFormat.h"

#include "OgreGLES2Prerequisites.h"


#include "OgreBlackBerryEGLSupport.h"
#include "OgreBlackBerryEGLWindow.h"
#include "OgreBlackBerryEGLContext.h"


namespace Ogre {

BlackBerryEGLWindow::BlackBerryEGLWindow(BlackBerryEGLSupport *glsupport) :
		mClosed(false), mVisible(false), mGLSupport(
				glsupport), mContext(NULL) {
}

BlackBerryEGLWindow::~BlackBerryEGLWindow() {
	destroy();

	destroyBlackberryWindow();
	if (mContext != NULL) {
		OGRE_DELETE mContext;
	}

}

void BlackBerryEGLWindow::getCustomAttribute(const String& name, void* pData) {
	if (name == "GLCONTEXT") {
		*static_cast<BlackBerryEGLContext**>(pData) = mContext;
		return;
	}
}

void BlackBerryEGLWindow::destroy() {
	if (mClosed) {
		return;
	}

	//reset
	mClosed = true;
	mActive = false;

	if (mIsFullScreen) {
		switchFullScreen(false);
	}

}

void BlackBerryEGLWindow::setFullscreen(bool fullscreen, uint width, uint height) {
}

void BlackBerryEGLWindow::_beginUpdate(void) {
	// Call the base class method first
	RenderTarget::_beginUpdate();

}

void BlackBerryEGLWindow::initNativeCreatedWindow(
		const NameValuePairList *miscParams) {
	// This method is called from within create() and after parameters have been parsed.
	// If the window, view or view controller objects are nil at this point, it is safe
	// to assume that external handles are either not being used or are invalid and
	// we can create our own.

	ConfigOptionMap::const_iterator opt;
	ConfigOptionMap::const_iterator end = mGLSupport->getConfigOptions().end();
	NameValuePairList::const_iterator param;

	//Fixme: need to support scaling?

	if ((opt = mGLSupport->getConfigOptions().find("Content Scaling Factor"))
			!= end) {
	}

	//create Blackberry window
	createBlackberryWindow();

}

void BlackBerryEGLWindow::createBlackberryWindow() {

	screen_context_t screen_ctx = Root::getSingleton().getBlackberryScreen();
	int nbuffers = 2; //double buffer

	//create a native window so that we can create gl surface from it.
	//we will eventually to render the framebuffer object to this surface,e.g.: window
	int rc = screen_create_window(&mWindow, screen_ctx);
	if (rc) {

		//log error
		return;
	}

	int usage = SCREEN_USAGE_OPENGL_ES2 | SCREEN_USAGE_ROTATION;
	int format = SCREEN_FORMAT_RGBA8888;

	rc = screen_set_window_property_iv(mWindow, SCREEN_PROPERTY_FORMAT,
			&format);
	if (rc) {
		//log eror
	}

	rc = screen_set_window_property_iv(mWindow, SCREEN_PROPERTY_USAGE, &usage);
	if (rc) {
		//log eror
	}

	int buffer_size[2] = { mWidth, mHeight };

	rc = screen_set_window_property_iv(mWindow, SCREEN_PROPERTY_SOURCE_SIZE,
			buffer_size);
	if (rc) {
		//log error
	}
	rc = screen_set_window_property_iv(mWindow, SCREEN_PROPERTY_BUFFER_SIZE,
			buffer_size);

	if (rc) {
		//log error
	}

	rc = screen_create_window_buffers(mWindow, nbuffers);
	if (rc) {
		//log error
	}

}

void BlackBerryEGLWindow::destroyBlackberryWindow(void) {
	if (mWindow != NULL) {
		screen_destroy_window(mWindow);
		mWindow = NULL;
	}
}

void BlackBerryEGLWindow::create(const String& name, uint width, uint height,
		bool fullScreen, const NameValuePairList *miscParams) {
	LogManager::getSingleton().logMessage("\tcreate called");

	short frequency = 0;
	bool vsync = false;
	int left = 0;
	int top = 0;

	mName = name;
	mWidth = width;
	mHeight = height;

	if (miscParams) {
		NameValuePairList::const_iterator opt;
		NameValuePairList::const_iterator end = miscParams->end();

		// Note: Some platforms support AA inside ordinary windows
		if ((opt = miscParams->find("FSAA")) != end) {
			mFSAA = StringConverter::parseUnsignedInt(opt->second);
		}

		if ((opt = miscParams->find("displayFrequency")) != end) {
			frequency = (short) StringConverter::parseInt(opt->second);
		}

		if ((opt = miscParams->find("vsync")) != end) {
			vsync = StringConverter::parseBool(opt->second);
		}

		if ((opt = miscParams->find("left")) != end) {
			left = StringConverter::parseInt(opt->second);
		}

		if ((opt = miscParams->find("top")) != end) {
			top = StringConverter::parseInt(opt->second);
		}

		if ((opt = miscParams->find("title")) != end) {
			mName = opt->second;
		}

	}

	mContext = mGLSupport->createNewContext();
	//FIXME: no scaling for now
	mContext->mBackingWidth = mWidth;
	mContext->mBackingHeight = mHeight;
	initNativeCreatedWindow(miscParams);

	mContext->createGlSurface((EGLNativeWindowType) mWindow);

	mContext->setCurrent();

	mContext->mBackingWidth = mWidth;
	mContext->mBackingHeight = mHeight;

	StringStream ss;

	ss << "BlackBerry Window created " << mWidth << " x " << mHeight
			<< " with backing store size " << mContext->mBackingWidth << " x "
			<< mContext->mBackingHeight;

	LogManager::getSingleton().logMessage(ss.str());

	left = top = 0;
	mLeft = left;
	mTop = top;

	mActive = true;
	mVisible = true;
	mClosed = false;
}

void BlackBerryEGLWindow::swapBuffers(bool waitForVSync) {
	if (mClosed) {
		return;
	}

	eglSwapBuffers(mContext->mGLDisplay, mContext->mGlSurf);

	return;

}

void BlackBerryEGLWindow::getLeftAndTopFromNativeWindow(int & left, int & top,
		uint width, uint height) {
	int pos[2];
	screen_get_window_property_iv(mWindow, SCREEN_PROPERTY_SOURCE_POSITION,
			pos);
	left = pos[0];
	top = pos[1];
}

void BlackBerryEGLWindow::reposition(int left, int top) {
	LogManager::getSingleton().logMessage("\treposition called");
	int pos[2] = { left, top };
	screen_set_window_property_iv(mWindow, SCREEN_PROPERTY_SOURCE_POSITION,
			pos);

}

void BlackBerryEGLWindow::resize(uint width, uint height) {
	LogManager::getSingleton().logMessage(
			"\t resize called! Nothing to do, handled by OS.");

	return;

}

void BlackBerryEGLWindow::windowMovedOrResized() {
	LogManager::getSingleton().logMessage("\twindowMovedOrResized called");
}

void BlackBerryEGLWindow::copyContentsToMemory(const PixelBox &dst,
		FrameBuffer buffer) {
	LogManager::getSingleton().logMessage("\twindowMovedOrResized called");
	if ((dst.left < 0) || (dst.right > mWidth) || (dst.top < 0)
			|| (dst.bottom > mHeight) || (dst.front != 0) || (dst.back != 1)) {
		OGRE_EXCEPT(Exception::ERR_INVALIDPARAMS, "Invalid box.",
				"EGLWindow::copyContentsToMemory");
	}

	if (buffer == FB_AUTO) {
		buffer = mIsFullScreen ? FB_FRONT : FB_BACK;
	}

	GLenum format = GLES2PixelUtil::getGLOriginFormat(dst.format);
	GLenum type = GLES2PixelUtil::getGLOriginDataType(dst.format);

	if ((format == 0) || (type == 0)) {
		OGRE_EXCEPT(Exception::ERR_INVALIDPARAMS, "Unsupported format.",
				"EGLWindow::copyContentsToMemory");
	}

	// Switch context if different from current one
	RenderSystem* rsys = Root::getSingleton().getRenderSystem();
	rsys->_setViewport(this->getViewport(0));

	// Must change the packing to ensure no overruns!
	glPixelStorei(GL_PACK_ALIGNMENT, 1);

	//glReadBuffer((buffer == FB_FRONT)? GL_FRONT : GL_BACK);
	glReadPixels((GLint) dst.left, (GLint) dst.top, (GLsizei) dst.getWidth(),
			(GLsizei) dst.getHeight(), format, type, dst.data);

	// restore default alignment
	glPixelStorei(GL_PACK_ALIGNMENT, 4);

	//vertical flip
	{
		size_t rowSpan = dst.getWidth()
				* PixelUtil::getNumElemBytes(dst.format);
		size_t height = dst.getHeight();
		uchar *tmpData = new uchar[rowSpan * height];
		uchar *srcRow = (uchar *) dst.data, *tmpRow = tmpData
				+ (height - 1) * rowSpan;

		while (tmpRow >= tmpData) {
			memcpy(tmpRow, srcRow, rowSpan);
			srcRow += rowSpan;
			tmpRow -= rowSpan;
		}
		memcpy(dst.data, tmpData, rowSpan * height);

		delete[] tmpData;
	}
}
}
