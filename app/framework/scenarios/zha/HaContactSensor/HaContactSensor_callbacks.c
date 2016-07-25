// *******************************************************************
// * HaContactSensor_callbacks.c
// *
// * This file implements non-plugin functionality required for the Silicon Labs
// * contact sensor reference design to properly function.
// *
// * Copyright 2015 Silicon Laboratories, Inc.                              *80*
// *******************************************************************


#include "app/framework/include/af.h"
#include "app/framework/plugin/reporting/reporting.h"
#include "app/framework/plugin-soc/temperature-measurement-server/temperature-measurement-server.h"

// This callback will execute any time the reporting intervals are modified.
// In order to verify the contact sensor is polling the temperature sensor
// frequently enough, it is necessary to call the
// emberAfPluginTemperatureMeasurementServerSetMeasurementRate function any time
// the reporting intervals are changed.
EmberAfStatus emberAfPluginReportingConfiguredCallback(
  const EmberAfPluginReportingEntry *entry)
{
  if ((entry->clusterId == ZCL_TEMP_MEASUREMENT_CLUSTER_ID)
      && (entry->attributeId == ZCL_TEMP_MEASURED_VALUE_ATTRIBUTE_ID)
      && (entry->direction == EMBER_ZCL_REPORTING_DIRECTION_REPORTED)
      && (entry->endpoint != EMBER_AF_PLUGIN_REPORTING_UNUSED_ENDPOINT_ID)) {
    //Max interval is set in seconds, which is the same unit of time the
    //emberAfPluginTemperatureMeasurementServerSetMeasurementRate expects in
    //this API.
    emberAfPluginTemperatureMeasurementServerSetMeasurementRate(
      entry->data.reported.maxInterval);
  }
  
  return EMBER_ZCL_STATUS_SUCCESS;
}

void emberAfMainInitCallback(void)
{
  uint8_t i;
  EmberAfPluginReportingEntry entry;
  
  // On initialization, cycle through the reporting table to determine if a
  // reporting interval was set for the device before powering down.  If so,
  // update the Temperature Server's hardware polling rate to match the
  // attribute defined maxInterval.  Otherwise, the plugin will use the plugin's
  // option defined hardware polling interval.
  for (i = 0; i < EMBER_AF_PLUGIN_REPORTING_TABLE_SIZE; i++) {
    emAfPluginReportingGetEntry(i, &entry);
    if ((entry.clusterId == ZCL_TEMP_MEASUREMENT_CLUSTER_ID)
        && (entry.attributeId == ZCL_TEMP_MEASURED_VALUE_ATTRIBUTE_ID)
        && (entry.direction == EMBER_ZCL_REPORTING_DIRECTION_REPORTED)
        && (entry.endpoint != EMBER_AF_PLUGIN_REPORTING_UNUSED_ENDPOINT_ID)) {
      //Max interval is set in seconds, which is the same unit of time the
      //emberAfPluginTemperatureMeasurementServerSetMeasurementRate expects in
      //this API.
      emberAfPluginTemperatureMeasurementServerSetMeasurementRate(
        entry.data.reported.maxInterval);
      return;
    }
  }
}

// The following code implements a fix for issue EMAPPFWKV2-1294 where the
// identify code fails to call the stop identifiy feedback callback after
// receiving a reset to factory defaults command during an identify operation.
void emberAfPluginBasicResetToFactoryDefaultsCallback(uint8_t endpoint)
{
  emberAfPluginIdentifyStopFeedbackCallback(endpoint);
}

