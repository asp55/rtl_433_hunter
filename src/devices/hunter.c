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

The message consists of:
1) A preamble of 12 short pulses, ~400us high followed by ~400us low
2) a 5188us gap
3) a 66 bit message 
   - 42 bits unique id for the remote, 12 bits for the command, and 12 bits for the inverse of the command (Each bit has a 1200us total pulse width, high: ~400us short / ~800us long)


Data layout:
    PPPPPPPP PPPP1III IIIIIIII IIIIIIII IIIIIIII IIIIIIII IIIII00C CCCCCCCC C11KKKKK KKKKK0

- P: 12-bit preamble
- I: 40-bit remote id
- C: 10-bit command
- K: 10-bit inverse command
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

    int result = 0;

    /*
     * invert the whole bit buffer.
     */
    bitbuffer_invert(bitbuffer);


    uint8_t const preamble_pattern[] = {0x00, 0x0f}; // 12 bit preamble

    for (int row = 0; row < bitbuffer->num_rows; ++row) {
        unsigned start_pos = bitbuffer_search(bitbuffer, row, 0, preamble_pattern, HUNTER_PREAMBLE_BITLEN);
        start_pos += 12; // skip preamble

        if (start_pos > bitbuffer->bits_per_row[row]) {
            //preamble not found
            //return DECODE_ABORT_LENGTH; 
            decoder_log(decoder, 1, __func__, "no preamble");
            continue;
        }

        if((bitbuffer->bits_per_row[row] - start_pos) < (HUNTER_BITLEN - HUNTER_PREAMBLE_BITLEN)) {
            //Message too short
            //return DECODE_ABORT_LENGTH; 
            decoder_log(decoder, 1, __func__, "short message");
            continue;

        }

        //Flip the bits

        /*
        * Get the command and inverse command from the message to check for message integrity
        */
        uint8_t c[2];
        bitbuffer_extract_bytes(bitbuffer, row, start_pos+43, c, 10);
        int command = c[0] << 8 | c[1];
        command = command >> 6; // Remove the right padding

        uint8_t ic[2];
        bitbuffer_extract_bytes(bitbuffer, row, start_pos+43+10+2, ic, 10);
        int icommand = ic[0] << 8 | ic[1];
        icommand = icommand >> 6;  // Remove the right padding

        // Command & inverse command should mask eachother out and result in 0
        if ((command & icommand) != 0 || (command | icommand) != 1023) {
            // Enable with -vv (verbose decoders)
            decoder_log(decoder, 1, __func__, "bad message");
            //return DECODE_FAIL_SANITY;
            continue;
        }

        /*
        * We've found at least 1 good message. Update result.
        */
        result = 1;

        /*
        * Now that we know the message is good, grab the id
        */
        uint8_t id[5];
        bitbuffer_extract_bytes(bitbuffer, row, start_pos+1, id, 40);

        char remote_id[11];
        sprintf(remote_id, "%02X%02X%02X%02X%02X", id[0], id[1], id[2], id[3], id[4]);

        char target_string[] = "Unknown";
        char action_string[17] = "Unknown";

        if(
          command == 4 ||
          command == 32 ||
          command == 35 ||
          command == 64 ||
          command == 98
        ) {
          sprintf(target_string, "Fan");

          switch(command) {
            case 4:
              sprintf(action_string, "Speed 33%%");
              break;

            case 32:
              sprintf(action_string, "Speed 66%%");
              break;

            case 64:
              sprintf(action_string, "Speed 100%%");
              break;

            case 35:
              sprintf(action_string, "Toggle");
              break;

            case 98:
              sprintf(action_string, "Off");
              break;
          }
        }
        else if(
          command == 10 ||
          command == 11 ||
          command == 12 ||
          command == 13 ||
          command == 14 ||
          command == 15 ||
          command == 72 ||
          command == 73 ||
          command == 138 ||
          command == 266 ||
          command == 768
        ) {
          sprintf(target_string, "Light");
          switch(command) {
            case 10:
              sprintf(action_string, "Brightness 12.5%%");
              break;

            case 11:
              sprintf(action_string, "Brightness 25%%");
              break;

            case 12:
              sprintf(action_string, "Brightness 37.5%%");
              break;

            case 13:
              sprintf(action_string, "Brightness 50%%");
              break;

            case 14:
              sprintf(action_string, "Brightness 62.5%%");
              break;

            case 15:
              sprintf(action_string, "Brightness 75%%");
              break;

            case 72:
              sprintf(action_string, "Brightness 87.5%%");
              break;

            case 73:
              sprintf(action_string, "Brightness 100%%");
              break;

            case 138:
              sprintf(action_string, "On");
              break;

            case 266:
              sprintf(action_string, "Off");
              break;

            case 768:
              sprintf(action_string, "Toggle");
              break;	
          }
        }


        /* clang-format off */
        data = data_make(
                "model", "", DATA_STRING, "Hunter",
                "id",    "", DATA_STRING, remote_id,
                "command", "", DATA_INT,  command,
                "target", "", DATA_STRING,  target_string,
                "action", "", DATA_STRING,  action_string,
                NULL);
        /* clang-format on */
        decoder_output_data(decoder, data);


    }


    


    return result;
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
        "target",
        "action",
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
        .reset_limit = 1480000,
        .tolerance   = 160,
        .decode_fn   = &hunter_decode,
        .fields      = output_fields,
};
