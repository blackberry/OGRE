#include "BlackBerry/OgreBlackBerryLogListener.h"
//#include <android/log.h>

//#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "OGRE", __VA_ARGS__))
//#define LOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, "OGRE", __VA_ARGS__))

namespace Ogre
{
    BlackBerryLogListener::BlackBerryLogListener()
    {
    }

    void BlackBerryLogListener::messageLogged(const String& message, LogMessageLevel lml, bool maskDebug, const String &logName, bool& skipThisMessage )
    {
        if(lml < Ogre::LML_CRITICAL)
        {
         //   LOGI(message.c_str());
        }
        else
        {
           // LOGE(message.c_str());
        }
    }
}
