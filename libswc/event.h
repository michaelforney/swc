/* swc: libswc/event.h
 *
 * Copyright (c) 2013-2015 Michael Forney
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

#ifndef SWC_EVENT_H
#define SWC_EVENT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <wayland-server.h>

/**
 * An event is the data passed to the listeners of the event_signals of various
 * objects.
 */
struct event {
	/**
	 * The type of event that was sent.
	 *
	 * The meaning of this field depends on the type of object containing the
	 * event_signal that passed this event.
	 */
	uint32_t type;

	/**
	 * Data specific to the event type.
	 *
	 * Unless explicitly stated in the description of the event type, this value
	 * is undefined.
	 */
	void *data;
};

static inline void
send_event(struct wl_signal *signal, uint32_t type, void *event_data)
{
	struct event event = { .type = type, .data = event_data };
	wl_signal_emit(signal, &event);
}

#ifdef __cplusplus
}
#endif

#endif /* SWC_EVENT_H */