/* swc: launch/devmajor-netbsd.c
 *
 * Copyright (c) 2020 Nia Alarie
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include <sys/types.h>
#include <sys/sysctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "devmajor.h"

static int
sysctl_devmajor(const char *devname)
{
	static int name[] = { CTL_KERN, KERN_DRIVERS };
	struct kinfo_drivers *drivers = NULL;
	size_t i, len, newlen;
	int major;

	if (sysctl(name, 2, NULL, &len, NULL, 0)) {
		perror("sysctl");
		goto fail;
	}
	if ((drivers = calloc(sizeof(struct kinfo_drivers), len)) == NULL) {
		perror("calloc");
		goto fail;
	}
	newlen = len;
	if (sysctl(name, 2, drivers, &newlen, NULL, 0)) {
		perror("sysctl");
		goto fail;
	}
	for (i = 0; i < len; ++i) {
		if (strcmp(devname, drivers[i].d_name) == 0) {
			major = drivers[i].d_cmajor;
			free(drivers);
			return major;
		}
	}
fail:
	free(drivers);
	return -1;
}

bool
device_is_input(dev_t rdev)
{
	if (major(rdev) == sysctl_devmajor("wskbd"))
		return true;
	if (major(rdev) == sysctl_devmajor("wsmouse"))
		return true;
	if (major(rdev) == sysctl_devmajor("wsmux"))
		return true;
	return false;
}

bool
device_is_tty(dev_t rdev)
{
	return major(rdev) == sysctl_devmajor("wsdisplay");
}

bool
device_is_drm(dev_t rdev)
{
	return major(rdev) == sysctl_devmajor("drm");
}

