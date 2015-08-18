/**
 * The MIT License (MIT)
 *
 * Copyright (c) 2014-2015 Gil Mendes
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/**
 * @file
 * @brief               Timing functions.
 */

#ifndef __TIME_H
#define __TIME_H

#include <lib/utility.h>

#ifndef TARGET_HAS_DELAY
extern mstime_t target_internal_time(void);
#endif

extern void delay(mstime_t time);

/** Convert seconds to milliseconds.
 * @param secs          Seconds value to convert.
 * @return              Equivalent time in milliseconds. */
static inline mstime_t secs_to_msecs(unsigned secs) {
        return (mstime_t)secs * 1000;
}

/** Convert milliseconds to seconds.
 * @param msecs         Milliseconds value to convert.
 * @return              Equivalent time in seconds (rounded up). */
static inline unsigned msecs_to_secs(mstime_t msecs) {
        return round_up(msecs, 1000) / 1000;
}

#endif /* __TIME_H */
