/** @file
    Hunter Ceiling Fan Remotes (433Mhz).

    Copyright (C) 2024 Andrew S. Parnell

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
 */

/*
    Use this as a starting point for a new decoder.

    Keep the Doxygen (slash-star-star) comment above to document the file and copyright.

    Keep the Doxygen (slash-star-star) comment below to describe the decoder.
    See http://www.doxygen.nl/manual/markdown.html for the formatting options.

    Remove all other multiline (slash-star) comments.
    Use single-line (slash-slash) comments to annontate important lines if needed.

    To use this:
    - Copy this template to a new file
    - Change at least `new_template` in the source
    - Add to include/rtl_433_devices.h
    - Run ./maintainer_update.py (needs a clean git stage or commit)

    Note that for simple devices doorbell/PIR/remotes a flex conf (see conf dir) is preferred.
*/

/**
The device uses PWM encoding,

The device sends a transmission on button press.

Data layout:
    PPPPPPPP PPPPIIII IIIIIIII IIIIIIII IIIIIIII IIIIIIII IIIIIICC CCCCCCCC CCKKKKKK KKKKKK

- P: 12-bit preamble
- I: 42-bit remote id
- C: 12-bit command
- K: 12-bit inverse command
*/

#include "decoder.h"

/*
 * Message is 78 bits long
 * Messages has a preamble of 0xfff
 * Remote sends the message 3 times
 *
 */
#define HUNTER_BITLEN     78
#define HUNTER_PREAMBLE_BITLEN     12
#define HUNTER_MINREPEATS 2

static int hunter_decode(r_device *decoder, bitbuffer_t *bitbuffer)
{
    data_t *data;
    int r;      // a row index

    /*
     * Early debugging aid to see demodulated bits in buffer and
     * to determine if your limit settings are matched and firing
     * this decode callback.
     *
     * 1. Enable with -vvv (debug decoders)
     * 2. Delete this block when your decoder is working
     */
    decoder_log_bitbuffer(decoder, 2, __func__, bitbuffer, "");


    /*
     * The bit buffer will contain multiple rows.
     * Typically a complete message will be contained in a single
     * row if long and reset limits are set correctly.
     * May contain multiple message repeats.
     * Message might not appear in row 0, if protocol uses
     * start/preamble periods of different lengths.
     */

    /*
     * Or, if you expect repeated packets
     * find a suitable row:
     */

    r = bitbuffer_find_repeated_row(bitbuffer, HUNTER_MINREPEATS, HUNTER_BITLEN);
    if (r < 0) {
        return DECODE_ABORT_EARLY;
    }

    /*
     * Or (preferred) search for the message preamble:
     * See bitbuffer_search()
     */

    uint8_t const preamble_pattern[] = {0xff, 0xf0}; // 12 bit preamble

    unsigned start_pos = bitbuffer_search(bitbuffer, r, 0, preamble_pattern, 12);
    start_pos += 12; // skip preamble

    if ((bitbuffer->bits_per_row[r] - start_pos) < HUNTER_BITLEN - HUNTER_PREAMBLE_BITLEN) {
        return DECODE_ABORT_LENGTH; // short buffer or preamble not found
    }

    /*
     * Get the command and inverse command from the message to check for message integrity
     */
    uint8_t c[2];
    bitbuffer_extract_bytes(bitbuffer, r, start_pos+42, c, 12);
    int command = c[0] << 8 | c[1];

    uint8_t ic[2];
    bitbuffer_extract_bytes(bitbuffer, r, start_pos+4+12, ic, 12);
    int icommand = ic[0] << 8 | ic[1];

    // Command & inverse command should mask eachother out and result in 0
    if (command & icommand) {
        // Enable with -vv (verbose decoders)
        decoder_log(decoder, 1, __func__, "bad message");
        return DECODE_FAIL_OTHER;
    }



    /*
     * Now that we know the message is good, grab the id & full message
     */
    uint8_t id[6];
    bitbuffer_extract_bytes(bitbuffer, r, start_pos, id, 42);
    char remote_id[13];
    sprintf(remote_id, "%02X%02X%02X%02X%02X%02X", id[0], id[1], id[2], id[3], id[4], id[5]);


    uint8_t value[9];
    bitbuffer_extract_bytes(bitbuffer, r, start_pos, value, HUNTER_BITLEN - HUNTER_PREAMBLE_BITLEN);
    char value_string[19];
    sprintf(value_string, "%02X%02X%02X%02X%02X%02X%02X%02X%02X", value[0], value[1], value[2], value[3], value[4], value[5], value[6], value[7], value[8]);


    /* clang-format off */
    data = data_make(
            "model", "", DATA_STRING, "Hunter",
            "id",    "", DATA_STRING, remote_id,
            "command", "", DATA_INT,  command,
            "data",  "", DATA_STRING, value_string,
            NULL);
    /* clang-format on */
    decoder_output_data(decoder, data);

    // Return 1 if message successfully decoded
    return 1;
}



/*
 * List of fields that may appear in the output
 *
 * Used to determine what fields will be output in what
 * order for this device when using -F csv.
 *
 */
static char const *const output_fields[] = {
        "model",
        "id",
        "command",
        "data",
        NULL,
};

/*
 * r_device - registers device/callback. see rtl_433_devices.h
 *
 * Timings:
 *
 * short, long, and reset - specify pulse/period timings in [us].
 *     These timings will determine if the received pulses
 *     match, so your callback will fire after demodulation.
 *
 * Modulation:
 *
 * The function used to turn the received signal into bits.
 * See:
 * - pulse_slicer.h for descriptions
 * - r_device.h for the list of defined names
 *
 * To enable your device, append it to the list in include/rtl_433_devices.h
 * and sort it into src/CMakeLists.txt or run ./maintainer_update.py
 *
 */
r_device const hunter = {
        .name        = "Hunter",
        .modulation  = OOK_PULSE_PWM,
        .short_width = 412,
        .long_width  = 812,
        .reset_limit = 12004,
        .tolerance   = 160,
        .decode_fn   = &hunter_decode,
        .fields      = output_fields,
};
