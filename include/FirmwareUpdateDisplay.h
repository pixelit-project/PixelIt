#pragma once
#include <stddef.h>

// Called only by the authenticated, synchronous HTTP update handler.
void FirmwareUpdateUpload(int status, size_t received, size_t requestBytes, bool failed);

// Successful completion only, immediately before the updater restarts.
void FirmwareUpdateReboot();
