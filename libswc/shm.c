/* swc: libswc/shm.c
 *
 * Copyright (c) 2013 Michael Forney
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

#include "shm.h"
#include "internal.h"

#include <stddef.h>
#include <wld/wld.h>
#include <wld/pixman.h>

struct swc_shm swc_shm;

bool swc_shm_initialize()
{
    if (!(swc.shm->context = wld_pixman_create_context()))
        goto error0;

    if (!(swc.shm->renderer = wld_create_renderer(swc.shm->context)))
        goto error1;

    return true;

  error1:
    wld_destroy_context(swc.shm->context);
  error0:
    return false;
}

void swc_shm_finalize()
{
    wld_destroy_renderer(swc.shm->renderer);
    wld_destroy_context(swc.shm->context);
}

