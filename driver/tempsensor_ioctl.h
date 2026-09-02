#ifndef TEMPSENSOR_IOCTL_H
#define TEMPSENSOR_IOCTL_H

#include <linux/ioctl.h>

#define TEMP_IOC_MAGIC 't'

/* Reset the simulated sensor back to a baseline temperature (25.0 C) */
#define TEMP_IOC_RESET       _IO(TEMP_IOC_MAGIC, 1)

/* Set how aggressively the simulated temperature drifts each read.
 * Argument is an int, in tenths of a degree (e.g. 5 = up to 0.5C per read). */
#define TEMP_IOC_SET_DRIFT   _IOW(TEMP_IOC_MAGIC, 2, int)

/* Set the simulated battery level (0-100 percent).
 * Argument is an int representing percentage. */
#define TEMP_IOC_SET_BATTERY _IOW(TEMP_IOC_MAGIC, 3, int)

/* Get the current simulated battery level.
 * Argument is a pointer to an int that receives the percentage. */
#define TEMP_IOC_GET_BATTERY _IOR(TEMP_IOC_MAGIC, 4, int)

#endif
