#pragma once

#include <content_redirection/defines.h>
#include <sys/iosupport.h>

namespace DevoptabTrampoline {
    /**
     * @brief Takes an ABI device definition, allocates a trampoline slot, and returns a fully wired devoptab_t.
     * * Does NOT call AddDevice
     * @return A pointer to the configured devoptab_t, or nullptr if validation fails or no slots are available.
     */
    devoptab_t *CreateDevoptab(const ContentRedirectionDeviceABI *device);

    /**
     * @brief Clears a devoptab to mark it as unused.
     */
    void ClearDevoptab(const devoptab_t *devoptab);

    /**
      * @brief Removes an device by name if existing.
      * Does call RemoveDevice
      * @return true if device with name was found, false no device was found. Check resultOut when true was returned.
     */
    bool RemoveDevoptab(const char *deviceName, int *resultOut);
} // namespace DevoptabTrampoline