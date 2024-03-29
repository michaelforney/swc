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
#include <sys/stat.h>
#include <stdlib.h>
#include "devmajor.h"

bool
device_is_input(dev_t rdev)
{
	if (major(rdev) == getdevmajor("wskbd", S_IFCHR))
		return true;
	if (major(rdev) == getdevmajor("wsmouse", S_IFCHR))
		return true;
	if (major(rdev) == getdevmajor("wsmux", S_IFCHR))
		return true;
	return false;
}

bool
device_is_tty(dev_t rdev)
{
	return major(rdev) == getdevmajor("wsdisplay", S_IFCHR);
}

bool
device_is_drm(dev_t rdev)
{
	return major(rdev) == getdevmajor("drm", S_IFCHR);
}

