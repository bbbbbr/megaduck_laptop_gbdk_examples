#include <gbdk/platform.h>
#include <gb/isr.h>

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "megaduck_laptop_io.h"

volatile SFR __at(0xFF60) FF60_REG;

volatile bool    serial_byte_recieved;
volatile uint8_t serial_rx_data;
         uint8_t serial_rx_buf[MEGADUCK_RX_MAX_PAYLOAD_LEN];
         uint8_t serial_rx_buf_len;

         uint8_t serial_system_status;
         uint8_t serial_cmd_0x09_reply_data; // In original hardware it's requested, but used for nothing?


static void delay_1_msec(void);



void sio_isr(void) CRITICAL INTERRUPT {

    // Save received data and update status flag
    // Turn Serial ISR back off
    serial_rx_data = SB_REG;
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
// - Timeout is about ~ 103 msec or 6.14
// - Serial ISR populates status var if anything was received
void serial_io_wait_for_transfer_w_timeout_100msec(void) {
    uint8_t counter = 0x64u;
    while (counter--) {
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
    return serial_rx_data;
}


// Waits for a byte from Serial IO with a timeout (100 msec)
// Returns:
// - If timed out: false
// - If successful: true (rx byte will be in serial_rx_data global)
bool serial_io_wait_receive_timeout_100msec(void) {
    CRITICAL {
        serial_byte_recieved = false;
    }

    serial_io_enable_receive_byte();
    serial_io_wait_for_transfer_w_timeout_100msec();
    return serial_byte_recieved;
}


// Waits for a byte from Serial IO with a timeout (200 msec)
// Returns:
// - If timed out: false
// - If successful: true (rx byte will be in serial_rx_data global)
bool serial_io_wait_receive_timeout_200msec(void) {
    CRITICAL {
        serial_byte_recieved = false;
    }
    serial_io_enable_receive_byte();

    uint8_t timeout = 2u;
    while (timeout-- & (!serial_byte_recieved)) {
        serial_io_wait_for_transfer_w_timeout_100msec();
    }

    return serial_byte_recieved;
}


// Sends a command and then receives a multi-byte buffer over Serial IO
//
// - Receive buffer globals: serial_rx_buf, size in: serial_rx_buf_len
// - Length of serial transfer: Determined by sender
// - Returns: true if succeeded
//
bool serial_io_send_command_and_receive_buffer(uint8_t io_cmd) {

    uint8_t packet_length  = 0u;
    uint8_t checksum_calc  = 0x00u;

    // Reset global rx buffer length
    serial_rx_buf_len      = 0u;

    // Save interrupt enables and then set only Serial to ON
    uint8_t int_enables_saved = IE_REG;
    IE_REG = SIO_IFLAG;

    // delay_1_msec()  // Another mystery, ignore it for now
    serial_io_send_byte(io_cmd);
    
    // Fail if first rx byte timed out
    if (serial_io_wait_receive_timeout_100msec()) {

        // First rx byte will be length of all incoming bytes
        if (serial_rx_data <= MEGADUCK_RX_MAX_PAYLOAD_LEN) {

            // Save rx byte as length and use to initialize checksum
            // Reduce length by 1 (since it includes length byte already received)
            checksum_calc = serial_rx_data;
            packet_length     = serial_rx_data - 1u;

            while (packet_length--) {
                // Wait for next rx byte
                if (serial_io_wait_receive_timeout_100msec()) {
                    // Save rx byte to buffer and add to checksum
                    checksum_calc                     += serial_rx_data;
                    serial_rx_buf[serial_rx_buf_len++] = serial_rx_data;
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
void serial_external_controller_init(void) {
    uint8_t counter;

    IE_REG = SIO_IFLAG;
    serial_system_status = SYS_REPLY__BOOT_UNSET;

    // Send a count up sequence through the serial IO (0,1,2,3...255)
    // Exit on 8 bit unsigned wraparound to 0x00
    counter = 0u;
    do {
        serial_io_send_byte(counter++);
    } while (counter != 0u);

    // Then wait for a response with no timeout
    if (serial_io_read_byte_no_timeout() != SYS_REPLY_BOOT_OK) {
        serial_system_status |= SYS_REPLY__BOOT_FAIL;
    }

    // Send a command that seems to request a 255..0 countdown sequence from the external controller
    serial_io_send_byte(SYS_CMD_INIT_SEQ_REQUEST);

    // Expects a reply sequence through the serial IO of (255,254,253...0)
    counter = 255u;

    // Exit on 8 bit unsigned wraparound to 0xFFu
    do {
        if (counter != serial_io_read_byte_no_timeout()) {
            // This is for serial_system_status_set_fail() { ... }
            serial_system_status |= SYS_REPLY__BOOT_FAIL;
        }
        counter--;
    } while (counter != 255u);

    // Check for failures during the reply sequence
    // and send reply byte based on that
    if (serial_system_status & SYS_REPLY__BOOT_FAIL)
        serial_io_send_byte(SYS_CMD_ABORT_OR_FAIL);
    else
        serial_io_send_byte(SYS_CMD_DONE_OR_OK);
}



void serial_startup(void) {
    uint8_t int_enables_saved;

    disable_interrupts();
    int_enables_saved = IE_REG;
    SC_REG = 0x00u;
    SB_REG = 0x00u;

    // Initialize Serially attached peripheral
    serial_external_controller_init();
    while (serial_system_status & SYS_REPLY__BOOT_FAIL);

    // Save response from some command
    // (so far not seen being used in 32K Bank 0)
    serial_io_send_byte(SYS_CMD_INIT_UNKNOWN_0x09);
    serial_io_read_byte_no_timeout();
    serial_cmd_0x09_reply_data = serial_rx_data;

    // Ignore the RTC init check for now

    // general_init__97A_
    // vram_init__752_

    IE_REG = int_enables_saved;
    enable_interrupts();
}

