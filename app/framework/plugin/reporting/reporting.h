// *******************************************************************
// * reporting.h
// *
// *
// * Copyright 2011 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

typedef struct {
  uint32_t lastReportTimeMs;
  uint32_t lastReportValue;
  bool reportableChange;
} EmAfPluginReportVolatileData;
extern EmAfPluginReportVolatileData emAfPluginReportVolatileData[];
EmberAfStatus emberAfPluginReportingConfigureReportedAttribute(const EmberAfPluginReportingEntry *newEntry);
void emAfPluginReportingGetEntry(uint8_t index, EmberAfPluginReportingEntry *result);
void emAfPluginReportingSetEntry(uint8_t index, EmberAfPluginReportingEntry *value);
EmberStatus emAfPluginReportingRemoveEntry(uint8_t index);
