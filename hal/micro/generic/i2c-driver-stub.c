// Copyright 2015 Silicon Laboratories, Inc.

#include PLATFORM_HEADER

void halI2cInitialize(void)
{
}

void emberAfPluginI2cDriverInitCallback(void)
{
}

int8u halI2cWriteBytes(int8u address, const int8u *buffer, int8u count)
{
}

int8u halI2cWriteBytesDelay(int8u address,
                            const int8u *buffer,
                            int8u count,
                            int8u delay)
{
}

int8u halI2cReadBytes(int8u address, int8u *buffer, int8u count)
{
}
