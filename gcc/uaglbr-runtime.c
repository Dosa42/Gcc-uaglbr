#include <stddef.h>

static const unsigned char lower[26] = {
    0x01, 0x03, 0x09, 0x19, 0x11, 0x0b, 0x1b, 0x13, 0x0a, 0x1a,
    0x05, 0x07, 0x0d, 0x1d, 0x15, 0x0f, 0x1f, 0x17, 0x0e, 0x1e,
    0x25, 0x27, 0x3a, 0x2d, 0x3d, 0x35
};

__attribute__((weak, visibility("hidden")))
void uaglbr_restore_object(const unsigned char *payload, size_t original_size,
                          unsigned char *original)
{
    unsigned char pattern[16];
    for (unsigned j = 0; j < 10; ++j)
        pattern[j] = lower[(j + 9) % 10] + 0x80;
    for (unsigned j = 10; j < 16; ++j)
        pattern[j] = lower[j - 10];
    for (size_t i = 0; i < original_size; ++i) {
        const unsigned char *p = payload + i * 6;
        unsigned a = ((p[1] & 3u) << 6) | (p[2] & 63u);
        unsigned b = ((p[4] & 3u) << 6) | (p[5] & 63u);
        unsigned hi = 0, lo = 0;
        for (unsigned j = 0; j < 16; ++j) {
            hi |= j * (pattern[j] == a);
            lo |= j * (pattern[j] == b);
        }
        original[i] = (unsigned char)((hi << 4) | lo);
    }
}
