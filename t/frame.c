/*
 * Copyright (c) 2017 Fastly, Kazuho Oku
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */
#include "quicly/frame.h"
#include "test.h"

static void test_ack_decode_underflow(void)
{
    quicly_ack_frame_t decoded;

    { /* ack pn=0 */
        uint8_t pat[] = {0xe, 0, 0, 0, 0};
        const uint8_t *src = pat + 1;
        ok(quicly_decode_ack_frame(pat[0], &src, pat + sizeof(pat), &decoded) == 0);
        ok(src == pat + sizeof(pat));
        ok(decoded.largest_acknowledged == 0);
        ok(decoded.num_gaps == 0);
        ok(decoded.ack_block_lengths[0] == 1);
        ok(decoded.smallest_acknowledged == 0);
    }
    { /* underflow in first block length */
        uint8_t pat[] = {0xe, 0, 0, 0, 1};
        const uint8_t *src = pat + 1;
        ok(quicly_decode_ack_frame(pat[0], &src, pat + sizeof(pat), &decoded) != 0);
    }

    { /* frame with gap going down to pn=0 */
        uint8_t pat[] = {0xe, 2, 0, 1, 0, 0, 0};
        const uint8_t *src = pat + 1;
        ok(quicly_decode_ack_frame(pat[0], &src, pat + sizeof(pat), &decoded) == 0);
        ok(src == pat + sizeof(pat));
        ok(decoded.largest_acknowledged == 2);
        ok(decoded.num_gaps == 1);
        ok(decoded.ack_block_lengths[0] == 1);
        ok(decoded.ack_block_lengths[1] == 1);
        ok(decoded.smallest_acknowledged == 0);
    }

    { /* additional block length going negative */
        uint8_t pat[] = {0xe, 2, 0, 1, 0, 0, 1};
        const uint8_t *src = pat + 1;
        ok(quicly_decode_ack_frame(pat[0], &src, pat + sizeof(pat), &decoded) != 0);
    }
    { /* gap going negative */
        uint8_t pat[] = {0xe, 2, 0, 1, 0, 3, 0};
        const uint8_t *src = pat + 1;
        ok(quicly_decode_ack_frame(pat[0], &src, pat + sizeof(pat), &decoded) != 0);
    }
}

static void test_ack_decode(void)
{
    {
        uint8_t pat[] = {0xe, 0x34, 0x00, 0x00, 0x11};
        quicly_ack_frame_t decoded;

        const uint8_t *src = pat + 1;
        ok(quicly_decode_ack_frame(pat[0], &src, pat + sizeof(pat), &decoded) == 0);
        ok(src == pat + sizeof(pat));
        ok(decoded.largest_acknowledged == 0x34);
        ok(decoded.num_gaps == 0);
        ok(decoded.ack_block_lengths[0] == 0x12);
        ok(decoded.smallest_acknowledged == 0x34 - 0x12 + 1);
    }

    {
        uint8_t pat[] = {0xe, 0x34, 0x00, 0x02, 0x00, 0x01, 0x02, 0x03, 0x04};
        quicly_ack_frame_t decoded;

        const uint8_t *src = pat + 1;
        ok(quicly_decode_ack_frame(pat[0], &src, pat + sizeof(pat), &decoded) == 0);
        ok(src == pat + sizeof(pat));
        ok(decoded.largest_acknowledged == 0x34);
        ok(decoded.num_gaps == 2);
        ok(decoded.ack_block_lengths[0] == 1);
        ok(decoded.gaps[0] == 2);
        ok(decoded.ack_block_lengths[1] == 3);
        ok(decoded.gaps[1] == 4);
        ok(decoded.ack_block_lengths[2] == 5);
        ok(decoded.smallest_acknowledged == 0x34 - 1 - 2 - 3 - 4 - 5 + 1);
    }

    subtest("underflow", test_ack_decode_underflow);
}

static void test_ack_encode(void)
{
    quicly_ranges_t ranges;
    size_t range_index;
    uint8_t buf[256], *end;
    const uint8_t *src;
    quicly_ack_frame_t decoded;

    quicly_ranges_init(&ranges);
    quicly_ranges_update(&ranges, 0x12, 0x13);

    range_index = 0;
    end = quicly_encode_ack_frame(buf, buf + sizeof(buf), &ranges, &range_index);
    ok(end - buf == 5);

    quicly_ranges_dispose(&ranges);

    src = buf + 1;
    ok(quicly_decode_ack_frame(src[-1], &src, end, &decoded) == 0);
    ok(src == end);
    ok(decoded.num_gaps == 0);
    ok(decoded.largest_acknowledged == 0x12);
    ok(decoded.ack_block_lengths[0] == 1);

    /* TODO add more */
}

static void test_mozquic(void)
{
    quicly_stream_frame_t frame;
    static const char *mess = "\xc5\0\0\0\0\0\0\xb6\x16\x03";
    const uint8_t *p = (void *)mess, type_flags = *p++;
    quicly_decode_stream_frame(type_flags, &p, p + 9, &frame);
}

void test_frame(void)
{
    subtest("ack-decode", test_ack_decode);
    subtest("ack-encode", test_ack_encode);
    subtest("mozquic", test_mozquic);
}
