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

#ifndef __SampleBrowser_QNX_H__
#define __SampleBrowser_QNX_H__


#if OGRE_PLATFORM != OGRE_PLATFORM_QNX
#error This header is for use with QNX only
#endif

#include "OgrePlatform.h"
#include "SampleBrowser.h"

#include <screen/screen.h>
#include <bps/navigator.h>
#include <bps/screen.h>
#include <bps/bps.h>
#include <bps/event.h>

static OgreBites::SampleBrowser* theBrowser=0;
int exit_application =0;
static int orientation = NAVIGATOR_PORTRAIT;
static int angle=0;

// Blackberry Implemenation of QIS Multitouch event
class QnxMultiTouch: public OIS::MultiTouch {
public:
	QnxMultiTouch() :
			OIS::MultiTouch("DWM", false, 0, 0) {
		OIS::MultiTouchState portraitState, landscapeState;
		int width = atoi(getenv("WIDTH"));
		int height = atoi(getenv("HEIGHT"));
		portraitState.width = width;
		portraitState.height = height;
		landscapeState.width = height;
		landscapeState.height = width;

		mStates.push_back(portraitState);
		mStates.push_back(landscapeState);

	}

	/** @copydoc Object::setBuffered */
	virtual void setBuffered(bool buffered) {
	}

	/** @copydoc Object::capture */
	virtual void capture() {
	}

	/** @copydoc Object::queryInterface */
	virtual OIS::Interface* queryInterface(OIS::Interface::IType type) {
		return 0;
	}

	/** @copydoc Object::_initialize */
	virtual void _initialize() {
	}

	OIS::MultiTouchState &getMultiTouchState(int i) {
		while (i >= mStates.size()) {
			OIS::MultiTouchState state;
			if (theBrowser) {
				int width = atoi(getenv("WIDTH"));
				int height = atoi(getenv("HEIGHT"));

				state.width =
						orientation == NAVIGATOR_PORTRAIT ? width : height;
				state.height =
						orientation == NAVIGATOR_PORTRAIT ? height : width;
			}
			mStates.push_back(state);
		}
		return mStates[i];
	}
};

static void transformInputState(OIS::MultiTouchState &state) {
	int w = atoi(getenv("WIDTH"));
	int h = atoi(getenv("HEIGHT"));
	int absX = state.X.abs;
	int absY = state.Y.abs;
	int relX = state.X.rel;
	int relY = state.Y.rel;

	switch (angle) {
	case 0:
		break;
	case 90:
		state.X.abs = absX * w / h;
		state.Y.abs = absY * h / w;
		break;
	case 180:
		break;
	case 270:
		state.X.abs = absX * w / h;
		state.Y.abs = absY * h / w;

		break;
	}

}

static QnxMultiTouch* qnxMultiTouch = 0;

//a helper function to translate Blackberry Multitouch event into a OIS MultiTouch event
void handleScreenEvent(bps_event_t *event, OgreBites::SampleBrowser* browser) {
	screen_event_t screen_event = screen_event_get_event(event);

	int screen_val;
	screen_get_event_property_iv(screen_event, SCREEN_PROPERTY_TYPE,
			&screen_val);
	OIS::MultiTouchState &state = qnxMultiTouch->getMultiTouchState(
			orientation == NAVIGATOR_PORTRAIT ? 0 : 1);

	switch (screen_val) {
	case SCREEN_EVENT_MTOUCH_TOUCH:
		state.touchType = OIS::MT_Pressed;

		break;
	case SCREEN_EVENT_MTOUCH_MOVE:
		state.touchType = OIS::MT_Moved;
		break;
	case SCREEN_EVENT_MTOUCH_RELEASE:
		state.touchType = OIS::MT_Released;
		break;
	default:
		state.touchType = OIS::MT_None;
	}
	if (state.touchType != OIS::MT_None) {
		int pos[2];
		screen_get_event_property_iv(screen_event, SCREEN_PROPERTY_POSITION,
				pos);

		int last = state.X.abs;
		state.X.abs = 0 + pos[0];
		state.X.rel = state.X.abs - last;

		last = state.Y.abs;
		state.Y.abs = 0 + pos[1];
		state.Y.rel = state.Y.abs - last;

		//last = state.Z.abs;
		state.Z.abs = 0;
		state.Z.rel = 0;

		transformInputState(state);
		OIS::MultiTouchEvent evt(qnxMultiTouch, state);

		switch (state.touchType) {
		case OIS::MT_Pressed:
			browser->touchPressed(evt);
			break;
		case OIS::MT_Released:
			browser->touchReleased(evt);
			break;
		case OIS::MT_Moved:
			browser->touchMoved(evt);
			break;
		case OIS::MT_Cancelled:
			browser->touchCancelled(evt);
			break;
		}
	}
}

bool handleNavigatorEvent(bps_event_t *event,
		OgreBites::SampleBrowser* browser) {
	switch (bps_event_get_code(event)) {
	case NAVIGATOR_ORIENTATION_CHECK: {

		navigator_orientation_check_response(event, true);
		break;
	}

	case NAVIGATOR_ORIENTATION: {
		angle = navigator_event_get_orientation_angle(event);
		orientation = navigator_event_get_orientation_mode(event);

		int width = atoi(getenv("WIDTH"));
		int height = atoi(getenv("HEIGHT"));
		if (orientation == NAVIGATOR_PORTRAIT) {
			browser->mWindow->resize(width, height);
		} else if (orientation == NAVIGATOR_LANDSCAPE) {
			browser->mWindow->resize(height, width);
		}

		browser->windowResized(browser->mWindow);

		navigator_done_orientation(event);
		break;
	}

	case NAVIGATOR_SWIPE_DOWN:
		//do nothing
		break;

	case NAVIGATOR_EXIT:

		exit_application = 1;
		break;

	default:

		return false;
	}

	return true;
}

void startQnxApp() {

	qnxMultiTouch = new QnxMultiTouch();

	OgreBites::SampleBrowser brows(false);

	theBrowser = &brows;
	screen_create_context(&brows.mscreen, 0);
	//Initialize BPS library
	bps_initialize();

	screen_request_events(brows.mscreen);
	navigator_request_events(0);
	navigator_rotation_lock(false);

	brows.go();

	//Initialize BPS library
	while (!exit_application) {
		//Request and process all available BPS events
		bps_event_t *event = NULL;

		for (;;) {
			if (BPS_SUCCESS != bps_get_event(&event, 0)) {
				fprintf(stderr, "bps_get_event failed\n");
				break;
			}

			if (event) {
				int domain = bps_event_get_domain(event);

				if (domain == screen_get_domain()) {
					handleScreenEvent(event,&brows);
				} else if (domain == navigator_get_domain()) {
					handleNavigatorEvent(event,&brows);
				}
			} else {
				break;
			}
		}

		if (brows.getOgreRoot() && brows.getOgreRoot()->getRenderSystem() != NULL) {
			brows.getOgreRoot()->renderOneFrame(); // start the render loop
		}

	}

	//Stop requesting events from libscreen
	screen_stop_events(brows.mscreen);

	//Shut down BPS library for this process
	bps_shutdown();
	brows.closeApp();

}




#endif
