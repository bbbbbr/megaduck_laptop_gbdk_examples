#include <gbdk/platform.h>
#include <gb/isr.h>

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include <megaduck_laptop_io.h>

volatile SFR __at(0xFF60) FF60_REG;

// TODO: namespace to megaduck
volatile bool    serial_byte_recieved;
volatile uint8_t megaduck_serial_rx_data;

         uint8_t megaduck_serial_rx_buf[MEGADUCK_RX_MAX_PAYLOAD_LEN];
         uint8_t megaduck_serial_rx_buf_len;

         uint8_t megaduck_serial_tx_buf[MEGADUCK_TX_MAX_PAYLOAD_LEN];
         uint8_t megaduck_serial_tx_buf_len;

         uint8_t serial_cmd_0x09_reply_data; // In original hardware it's requested, but used for nothing?


static void delay_1_msec(void);



void sio_isr(void) CRITICAL INTERRUPT {

    // Save received data and update status flag
    // Turn Serial ISR back off
    megaduck_serial_rx_data = SB_REG;
    serial_byte_recieved = true;
    set_interrupts(IE_REG & ~SIO_IFLAG);
}

ISR_VECTOR(VECTOR_SERIAL, sio_isr)



static void delay_1_msec(void) {
    volatile uint8_t c = 75u;
    while (c--);
}


// Waits for a serial transfer to complete with a timeout
//
// - Timeout length is roughly in msec (100 is about ~ 103 msec or 6.14 frames)
// - Serial ISR populates status var if anything was received
void serial_io_wait_for_transfer_with_timeout(uint8_t timeout_len_ms) {
    while (timeout_len_ms--) {
        delay_1_msec();
        if (serial_byte_recieved)
            return;
    }
}


// Sends a byte out over serial IO
void serial_io_send_byte(uint8_t tx_byte) {

    FF60_REG = FF60_REG_BEFORE_XFER;  // Seems optional in testing so far
    SB_REG = tx_byte;
    SC_REG = SIOF_XFER_START | SIOF_CLOCK_INT;

    // TODO: the delay here seems inefficient, but need to find out actual timing on the wire first
    delay_1_msec();

    // Restore to SIO input and clear pending interrupt
    IF_REG = 0x00;
    SC_REG = SIOF_XFER_START | SIOF_CLOCK_EXT;
}


// Prepares to receive data through the serial IO
//
// - Sets serial IO to external clock and enables ready state
// - Turns on Serial interrupt, clears pending interrupts and turns them on
void serial_io_enable_receive_byte(void) {
    FF60_REG = FF60_REG_BEFORE_XFER;
    SC_REG = (SIOF_XFER_START | SIOF_CLOCK_EXT);
    IE_REG |= SIO_IFLAG;
    IF_REG = 0x00;
    enable_interrupts();
}


// Waits for and returns a byte from Serial IO with NO timeout
uint8_t serial_io_read_byte_no_timeout(void) {
    CRITICAL {
        serial_byte_recieved = false;
    }

    serial_io_enable_receive_byte();
    while (!serial_byte_recieved);
    return megaduck_serial_rx_data;
}


// Waits for a byte from Serial IO with a timeout
// Returns:
// - Timeout length is roughly in msec (100 is about ~ 103 msec or 6.14 frames)
// - If timed out: false
// - If successful: true (rx byte will be in megaduck_serial_rx_data global)
bool serial_io_read_byte_with_msecs_timeout(uint8_t timeout_len_ms) {
    uint8_t msec_counter;
    CRITICAL {
        serial_byte_recieved = false;
    }

    serial_io_enable_receive_byte();

    while (timeout_len_ms--) {
        // Each full run of the inner loop is ~ 1msec
        msec_counter = 75u;
        while (msec_counter--) {
            if (serial_byte_recieved)
                return true;
        }
    }

    return serial_byte_recieved;
}


// Sends a byte and waits for a reply with timeout
// Returns:
// - Timeout length is roughly in msec (100 is about ~ 103 msec or 6.14 frames)
// - If timed out: false
// - If ack/reply didn't match expected: false
// - Otherwise true
bool serial_io_send_byte_and_check_ack_msecs_timeout(uint8_t tx_byte, uint8_t timeout_len_ms, uint8_t expected_reply) {
    serial_io_send_byte(tx_byte);

    // Fail if first rx byte timed out
    if (!serial_io_read_byte_with_msecs_timeout(timeout_len_ms)) return false;

    // Then check reply byte vs expected reply
    return (megaduck_serial_rx_data == expected_reply);
}


// Sends a command and a multi-byte buffer over Serial IO
//
// - Send buffer globals: megaduck_serial_tx_buf, size in: megaduck_serial_tx_buf_len
// - Length of serial transfer: Determined by megaduck_serial_tx_buf_len
// - Returns: true if succeeded
//
bool serial_io_send_command_and_buffer(uint8_t io_cmd) {

    // Send buffer length + 2 (for length header and checksum bytes)
    uint8_t packet_length = megaduck_serial_tx_buf_len + 2;
    uint8_t checksum_calc = packet_length; // Use total tx length (byte) as initial checksum

    // Save interrupt enables and then set only Serial to ON
    uint8_t int_enables_saved = IE_REG;
    IE_REG = SIO_IFLAG;

    // Send command to initiate buffer transfer, then check for reply
    if (!serial_io_send_byte_and_check_ack_msecs_timeout(io_cmd, TIMEOUT_200_MSEC, SYS_REPLY_SEND_BUFFER_OK)) {
        IE_REG = int_enables_saved;
        return false;
    }

    // Send buffer length + 2 (for length header and checksum bytes)
    delay_1_msec;  // Delay for unknown reasons (present in system rom)
        if (!serial_io_send_byte_and_check_ack_msecs_timeout(packet_length, TIMEOUT_200_MSEC, SYS_REPLY_SEND_BUFFER_OK)) {
        IE_REG = int_enables_saved;
        return false;
    }

    // Send the buffer contents
    uint8_t buffer_bytes_to_send = megaduck_serial_tx_buf_len;
    for (uint8_t idx = 0; idx < megaduck_serial_tx_buf_len; idx++) {

        // Update checksum with next byte
        checksum_calc += megaduck_serial_tx_buf[idx];

        // Send a byte from the buffer
        if (!serial_io_send_byte_and_check_ack_msecs_timeout(megaduck_serial_tx_buf[idx], TIMEOUT_200_MSEC, SYS_REPLY_SEND_BUFFER_OK)) {
            IE_REG = int_enables_saved;
            return false;
        }
    }

    // Done sending buffer bytes, last byte to send is checksum 
    // Tx Checksum Byte should == (((sum of all bytes except checksum) XOR 0xFF) + 1) [two's complement]
    checksum_calc = ~checksum_calc + 1u;  // 2's complement
    // Note different expected reply value versus previous reply checks
    if (!serial_io_send_byte_and_check_ack_msecs_timeout(checksum_calc, TIMEOUT_200_MSEC, SYS_REPLY_BUFFER_SEND_AND_CHECKSUM_OK)) {
        IE_REG = int_enables_saved;
        return false;
    }

    // Success
    IE_REG = int_enables_saved;
    return true;
}



// Sends a command and then receives a multi-byte buffer over Serial IO
//
// - Receive buffer globals: megaduck_serial_rx_buf, size in: megaduck_serial_rx_buf_len
// - Length of serial transfer: Determined by sender
// - Returns: true if succeeded
//
bool serial_io_send_command_and_receive_buffer(uint8_t io_cmd) {

    uint8_t packet_length  = 0u;
    uint8_t checksum_calc  = 0x00u;

    // Reset global rx buffer length
    megaduck_serial_rx_buf_len      = 0u;

    // Save interrupt enables and then set only Serial to ON
    uint8_t int_enables_saved = IE_REG;
    IE_REG = SIO_IFLAG;

    // delay_1_msec()  // Another mystery, ignore it for now
    serial_io_send_byte(io_cmd);

    // Fail if first rx byte timed out
    if (serial_io_read_byte_with_msecs_timeout(TIMEOUT_100_MSEC)) {

        // First rx byte will be length of all incoming bytes
        if (megaduck_serial_rx_data <= MEGADUCK_RX_MAX_PAYLOAD_LEN) {

            // Save rx byte as length and use to initialize checksum
            // Reduce length by 1 (since it includes length byte already received)
            checksum_calc = megaduck_serial_rx_data;
            packet_length     = megaduck_serial_rx_data - 1u;

            while (packet_length--) {
                // Wait for next rx byte
                if (serial_io_read_byte_with_msecs_timeout(TIMEOUT_100_MSEC)) {
                    // Save rx byte to buffer and add to checksum
                    checksum_calc                     += megaduck_serial_rx_data;
                    megaduck_serial_rx_buf[megaduck_serial_rx_buf_len++] = megaduck_serial_rx_data;
                } else {
                    // Error: Break out and set checksum so it fails test below (causing return with failure)
                    checksum_calc = 0xFFu;
                    break;
                }
            }

            // Done receiving buffer bytes, last rx byte should be checksum 
            // Rx Checksum Byte should == (((sum of all bytes except checksum) XOR 0xFF) + 1) [two's complement]
            // so ((sum of received bytes including checksum byte) should == -> unsigned 8 bit overflow -> 0x00
            if (checksum_calc == 0x00u) {
                // Return success
                serial_io_send_byte(SYS_CMD_DONE_OR_OK);
                IE_REG = int_enables_saved;
                return true;                
            }
        }
    }

    // Something went wrong, error out
    serial_io_send_byte(SYS_CMD_ABORT_OR_FAIL);
    IE_REG = int_enables_saved;
    return false;
}


// Does a serial IO external controller init
//
// - Needs to be done any time system is powered on or a cartridge is booted
// - Sends count up sequence + some commands, waits for and checks a count down sequence in reverse
bool megaduck_laptop_controller_init(void) {
    uint8_t counter;

    IE_REG = SIO_IFLAG;
    bool serial_system_init_is_ok = true;

    // Send a count up sequence through the serial IO (0,1,2,3...255)
    // Exit on 8 bit unsigned wraparound to 0x00
    counter = 0u;
    do {
        serial_io_send_byte(counter++);
    } while (counter != 0u);

    // Then wait for a response
    // Fail if reply back timed out or was not expected response
    if (serial_io_read_byte_with_msecs_timeout(TIMEOUT_2_MSEC)) {
        if (megaduck_serial_rx_data != SYS_REPLY_BOOT_OK) serial_system_init_is_ok = false;
    } else
        serial_system_init_is_ok = false;

    // Send a command that seems to request a 255..0 countdown sequence from the external controller
    if (serial_system_init_is_ok)  {
        serial_io_send_byte(SYS_CMD_INIT_SEQ_REQUEST);

        // Expects a reply sequence through the serial IO of (255,254,253...0)
        counter = 255u;

        // Exit on 8 bit unsigned wraparound to 0xFFu
        do {
            // Fail if reply back timed out or did not match expected counter
            // TODO: OEM approach doesn't break out once a failure occurs, 
            //       but maybe that's possible + sending the abort command early?
            if (serial_io_read_byte_with_msecs_timeout(TIMEOUT_2_MSEC)) {
                if (counter != megaduck_serial_rx_data) serial_system_init_is_ok = false;
            } else
                serial_system_init_is_ok = false;
            counter--;
        } while (counter != 255u);

        // Check for failures during the reply sequence
        // and send reply byte based on that
        if (serial_system_init_is_ok)
            serial_io_send_byte(SYS_CMD_DONE_OR_OK);
        else
            serial_io_send_byte(SYS_CMD_ABORT_OR_FAIL);
    }

    return serial_system_init_is_ok;
}


bool megaduck_laptop_init(void) {
    uint8_t int_enables_saved;
    bool laptop_init_is_ok = true;
`
    disable_interrupts();
    int_enables_saved = IE_REG;
    SC_REG = 0x00u;
    SB_REG = 0x00u;

    // Initialize Serially attached peripheral
    laptop_init_is_ok = megaduck_laptop_controller_init();
    if (laptop_init_is_ok) {
        // Save response from some command
        // (so far not seen being used in 32K Bank 0)
        serial_io_send_byte(SYS_CMD_INIT_UNKNOWN_0x09);
        serial_io_read_byte_no_timeout();
        serial_cmd_0x09_reply_data = megaduck_serial_rx_data;
    }

    // Ignore the RTC init check for now

    IE_REG = int_enables_saved;
    enable_interrupts();

    return (laptop_init_is_ok);
}

