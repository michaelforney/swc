/* swc: swc/internal.h
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

#ifndef SWC_INTERNAL_H
#define SWC_INTERNAL_H

#include "util.h"

#define INTERNAL_DECL(type, name)                                           \
    struct                                                                  \
    {                                                                       \
        struct swc_ ## type name;                                           \
        struct swc_ ## type ## _internal _ ## name ## _internal;            \
    }

#define INTERNAL_ASSOCIATIONS(ptr, base)                                    \
    INTERNAL_ASSOCIATION(window, ptr,                                       \
    base)

#if defined(__has_feature)
#   define HAVE_GENERIC __has_extension(c_generic_selections)
/* GCC doesn't have _Generic support, even with -std=c11 */
#elif __STDC_VERSION >= 201112L && !defined(__GNUC__)
#   define HAVE_GENERIC 1
#else
#   define HAVE_GENERIC 0
#endif

#if HAVE_GENERIC
#   define INTERNAL_ASSOCIATION(type, ptr, next)                            \
        struct swc_ ## type *:                                              \
            &((INTERNAL_DECL(type, dummy) *) ptr)->_dummy_internal          \
        next
#   define INTERNAL(ptr)                                                    \
        _Generic(ptr, INTERNAL_ASSOCIATIONS(ptr,))
#else
/* If we don't have _Generic, emulate it with __builtin_choose_expr. */
#   define INTERNAL_ASSOCIATION(type, ptr, next)                            \
        __builtin_choose_expr(                                              \
        __builtin_types_compatible_p(typeof(ptr), struct swc_ ## type *),   \
        &((INTERNAL_DECL(type, dummy) *) ptr)->_dummy_internal, next)
#   define INTERNAL(ptr) \
        INTERNAL_ASSOCIATIONS(ptr, (void) 0)
#endif

#define CONTAINER_OF_INTERNAL(ptr, type, member)                            \
    &CONTAINER_OF(ptr, INTERNAL_DECL(type, dummy),                          \
                  _dummy_internal.member)->dummy

#endif

