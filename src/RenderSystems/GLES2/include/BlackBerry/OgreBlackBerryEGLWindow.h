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

#ifndef __BlackBerryEGLWindow_H__
#define __BlackBerryEGLWindow_H__

#include "OgreBlackBerryEGLSupport.h"
#include <screen/screen.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>

namespace Ogre {
    class BlackBerryEGLContext;
    class _OgrePrivate BlackBerryEGLWindow : public RenderWindow
    {
    protected:
                bool mClosed;
                bool mVisible;

                screen_window_t  mWindow; //native window handle

                BlackBerryEGLSupport* mGLSupport;
                BlackBerryEGLContext* mContext;


                void switchFullScreen(bool fullscreen) { }
    			void getLeftAndTopFromNativeWindow(int & left, int & top, uint width, uint height);
    			void initNativeCreatedWindow(const NameValuePairList *miscParams);
    			void createNativeWindow(int &left, int &top, uint &width, uint &height, String &title);
    			void createBlackberryWindow(); //helper function to create Blackberry window
    			void reposition(int left, int top);
    			void resize(unsigned int width, unsigned int height);
    			void windowMovedOrResized();
                virtual void _beginUpdate();


    	public:
                BlackBerryEGLWindow(BlackBerryEGLSupport* glsupport);
                virtual ~BlackBerryEGLWindow();

                void create(const String& name, unsigned int width, unsigned int height,
                            bool fullScreen, const NameValuePairList *miscParams);

    			virtual void setFullscreen(bool fullscreen, uint width, uint height);
                void destroy(void);
                void destroyBlackberryWindow(void);
                bool isClosed(void) const { return mClosed; }
                bool isVisible(void) const { return mVisible; }

                void setVisible(bool visible) { mVisible = visible; }
                void setClosed(bool closed) { mClosed = closed; }
                void swapBuffers(bool waitForVSync);
                void copyContentsToMemory(const PixelBox &dst, FrameBuffer buffer);

                /**
                   @remarks
                   * Get custom attribute; the following attributes are valid:
                   * WINDOW         The NativeWindowType target for rendering.
                   * VIEW           The EAGLView object that is drawn into.
                   * VIEWCONTROLLER The UIViewController used for handling view rotation.
                   * GLCONTEXT      The Ogre GLES2Context used for rendering.
                   * SHAREGROUP     The EAGLShareGroup object associated with the main context.
                   */
                virtual void getCustomAttribute(const String& name, void* pData);

                bool requiresTextureFlipping() const { return false; }
	};
}

#endif
