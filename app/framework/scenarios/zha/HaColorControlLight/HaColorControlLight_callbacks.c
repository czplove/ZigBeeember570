//

// This callback file is created for your convenience. You may add application
// code to this file. If you regenerate this file over a previous version, the
// previous version will be overwritten and any code you have added will be
// lost.

#include "app/framework/include/af.h"


// The following code implements a fix for issue EMAPPFWKV2-1294 where the 
// identify code fails to call the stop identifiy feedback callback after 
// receiving a reset to factory defaults command during an identify operation.
void emberAfPluginBasicResetToFactoryDefaultsCallback(uint8_t endpoint)
{
  emberAfPluginIdentifyStopFeedbackCallback(endpoint);
}


