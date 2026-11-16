#define DMOD_ENABLE_REGISTRATION ON
#include "dmod.h"
#include "dmnotify.h"
#include "dmdrvi.h"
#include "dmlist.h"

#define DMNOTIFY_MAGIC      0x6E6F7469


//===============================================================================================
/**
 * @brief stores context of the driver
 */
//===============================================================================================
struct dmdrvi_context
{
    uint32_t        magic;          //!< Magic number to check if the context is valid
};

/**
 * @brief Create a DMDRVI context
 *
 * The driver will assign device numbers based on the configuration and return them
 * via the dev_num parameter. The driver also sets flags to indicate which numbering
 * scheme it uses (none, major only, or major+minor).
 *
 * @param config Pointer to dmini_context object with configuration parameters (dmini module required to parse them)
 * @param dev_num Output pointer to device number structure - driver fills in major, minor, and flags (must not be NULL)
 * 
 * @return dmdrvi_context_t Created DMDRVI context
 */
dmod_dmdrvi_dif_api_declaration(2.0, dmnotify, dmdrvi_context_t, _create, ( dmini_context_t config, dmdrvi_dev_num_t* dev_num ))
{
    dmdrvi_context_t context = Dmod_Malloc( sizeof(struct dmdrvi_context) );


    return context;
}

/**
 * @brief Free a DMDRVI context
 *
 * @param context DMDRVI context to free
 */
dmod_dmdrvi_dif_api_declaration(2.0, dmnotify, void, _free, ( dmdrvi_context_t context ))
{

}

/**
 * @brief Open a device
 *
 * A single driver context can expose more than one device (e.g. a
 * dynamically discovered sub-device announced via dmdrvi_device_available()),
 * so dev_num identifies which one of the context's devices is being opened.
 *
 * @param context DMDRVI context
 * @param flags Open flags  (DMDRVI_O_RDONLY, DMDRVI_O_WRONLY, DMDRVI_O_RDWR)
 * @param dev_num Device number identifying which device within the context to open
 *
 * @return void* Device handle
 */
dmod_dmdrvi_dif_api_declaration(2.0, dmnotify, void*, _open, ( dmdrvi_context_t context, int flags, const dmdrvi_dev_num_t* dev_num ))
{

}

/**
 * @brief Close a device
 *
 * @param context DMDRVI context
 * @param handle Device handle
 */
dmod_dmdrvi_dif_api_declaration(2.0, dmnotify, void, _close, ( dmdrvi_context_t context, void* handle ))
{

}

/**
 * @brief Read from a device
 *
 * @param context DMDRVI context
 * @param handle Device handle
 * @param buffer Buffer to read data into
 * @param size Number of bytes to read; values greater than INT64_MAX must fail
 * with -EOVERFLOW because they cannot be represented by dmdrvi_ssize_t
 * @param offset Non-negative byte offset from the beginning of the device
 * 
 * @return Number of bytes read, zero at end of device, or a negative
 * errno-compatible error. A zero-length request returns zero.
 */
dmod_dmdrvi_dif_api_declaration(2.0, dmnotify, dmdrvi_ssize_t, _read, ( dmdrvi_context_t context, void* handle, void* buffer, size_t size, dmdrvi_offset_t offset ))
{

}

/**
 * @brief Write to a device
 *
 * @param context DMDRVI context
 * @param handle Device handle
 * @param buffer Buffer with data to write
 * @param size Number of bytes to write; values greater than INT64_MAX must fail
 * with -EOVERFLOW because they cannot be represented by dmdrvi_ssize_t
 * @param offset Non-negative byte offset from the beginning of the device
 * 
 * @return Number of bytes written or a negative errno-compatible error. A
 * zero-length request returns zero.
 */
dmod_dmdrvi_dif_api_declaration(2.0, dmnotify, dmdrvi_ssize_t, _write, ( dmdrvi_context_t context, void* handle, const void* buffer, size_t size, dmdrvi_offset_t offset ))
{

}

/**
 * @brief Ioctl operation on a device
 *
 * @param context DMDRVI context
 * @param handle Device handle
 * @param command Ioctl command
 * @param arg Argument for the ioctl command
 * 
 * @return int Result of the ioctl operation (errno)
 */
dmod_dmdrvi_dif_api_declaration(2.0, dmnotify, int, _ioctl, ( dmdrvi_context_t context, void* handle, int command, void* arg ))
{

}

/**
 * @brief Flush device buffers
 *
 * @param context DMDRVI context
 * @param handle Device handle
 * 
 * @return int Result of the flush operation (errno)
 */
dmod_dmdrvi_dif_api_declaration(2.0, dmnotify, int, _flush, ( dmdrvi_context_t context, void* handle ))
{

}

/**
 * @brief Get device status
 *
 * Gets status information for the specified device path without requiring
 * the device to be opened first (similar to POSIX stat() which works with
 * a path without requiring fopen()).
 *
 * @param context DMDRVI context
 * @param path Device path (e.g., "/dev/dmuart0", "/dev/dmspi0/0")
 * @param stat Pointer to dmdrvi_stat_t structure to fill with status information
 * 
 * @return int Result of the stat operation (errno)
 */
dmod_dmdrvi_dif_api_declaration(2.0, dmnotify, int, _stat, ( dmdrvi_context_t context, const char* path, dmdrvi_stat_t* stat ))
{

}


/**
 * @brief Initialization function for the module.
 *
 * This function is called when the module is enabled.
 * Please use this function to initialize the module, for instance:
 * - initialize the module variables
 * - initialize the module hardware
 * - allocate memory
 */
int dmod_init(const Dmod_Config_t *Config)
{
    Dmod_Printf("Hello, World!\n");
    return 0;
}

/**
 * @brief De-initialization function for the module.
 *
 * This function is called when the module is disabled.
 * Please use this function to de-initialize the module, for instance:
 * - free memory
 * - de-initialize the module hardware
 * - de-initialize the module variables
 */
int dmod_deinit(void)
{
    Dmod_Printf("Goodbye, World!\n");
    return 0;
}
