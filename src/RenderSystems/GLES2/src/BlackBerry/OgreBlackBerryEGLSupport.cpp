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

#include "OgreException.h"
#include "OgreLogManager.h"
#include "OgreStringConverter.h"
#include "OgreRoot.h"

#include "OgreGLES2Prerequisites.h"
#include "OgreGLES2RenderSystem.h"

#include "OgreBlackBerryEGLSupport.h"
#include "OgreBlackBerryEGLWindow.h"
#include "OgreBlackBerryEGLContext.h"


namespace Ogre {

BlackBerryEGLSupport::BlackBerryEGLSupport() {

}

BlackBerryEGLSupport::~BlackBerryEGLSupport() {

}

String BlackBerryEGLSupport::getDisplayName(void) {
	return "BlackBerry GLES2 Support";
}

void BlackBerryEGLSupport::start(void) {
	LogManager::getSingleton().logMessage("\tGLSupport start called");
}

void BlackBerryEGLSupport::stop(void) {
	LogManager::getSingleton().logMessage("\tGLSupport stop called");
}

BlackBerryEGLContext* BlackBerryEGLSupport::createNewContext()
{
	return new BlackBerryEGLContext(this);
}

RenderWindow* BlackBerryEGLSupport::createWindow(bool autoCreateWindow,
		GLES2RenderSystem *renderSystem, const String& windowTitle) {
	LogManager::getSingleton().logMessage("\tGLSupport createWindow called");

	RenderWindow *window = 0;

	if (autoCreateWindow) {
		ConfigOptionMap::iterator opt;
		ConfigOptionMap::iterator end = mOptions.end();
		NameValuePairList miscParams;

		bool fullscreen = true;

		unsigned int w = atoi(getenv("WIDTH"));
		unsigned int h = atoi(getenv("HEIGHT"));

		if ((opt = mOptions.find("Display Frequency")) != end) {
			miscParams["displayFrequency"] = opt->second.currentValue;
		}

		window = renderSystem->_createRenderWindow(windowTitle, w, h,
				fullscreen, &miscParams);
	}

	return window;
}

RenderWindow* BlackBerryEGLSupport::newWindow(const String &name, unsigned int width,
		unsigned int height, bool fullScreen,
		const NameValuePairList *miscParams)
{
	LogManager::getSingleton().logMessage("\tGLSupport newWindow called");

	BlackBerryEGLWindow* window = new BlackBerryEGLWindow(this);
	window->create(name, width, height, fullScreen, miscParams);

	return window;
}


void BlackBerryEGLSupport::addConfig(void) {
	LogManager::getSingleton().logMessage("\tGLSupport addConfig called");

	// Currently no config options supported
	//refreshConfig();
}


String BlackBerryEGLSupport::validateConfig(void) {
	return StringUtil::BLANK;
}

void* BlackBerryEGLSupport::getProcAddress(const Ogre::String& name) {
	return 0;
}

}
