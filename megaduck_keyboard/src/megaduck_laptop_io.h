#include <gbdk/platform.h>
#include <stdint.h>

#ifndef _MEGADUCK_LAPTOP_IO_H
#define _MEGADUCK_LAPTOP_IO_H



#define FF60_REG_BEFORE_XFER       0x00u

#define SYS_REPLY__BOOT_FAIL       0x01u
#define SYS_REPLY__BOOT_UNSET      0x00u

#define SYS_CMD_INIT_SEQ_REQUEST          0x00u   // Value sent to request the init 255..0 countdown sequence
#define SYS_CMD_GET_KEYS                  0x00u   // Requests a multi-byte buffer with keyboard data
#define SYS_CMD_DONE_OR_OK                0x01u
#define SYS_CMD_DONE_OR_OK_AND_SOMETHING  0x81u
#define SYS_CMD_ABORT_OR_FAIL             0x04u
#define SYS_CMD_RUN_CART_IN_SLOT          0x08u
#define SYS_CMD_INIT_UNKNOWN_0x09         0x09u   // May also be PrintScreen related
#define SYS_CMD_RTC_SET_DATE_AND_TIME     0x0Bu   // Sets Hardware RTC Date and Time using multi-byte buffer send/TX
#define SYS_CMD_RTC_GET_DATE_AND_TIME     0x0Cu   // Requests a multi-byte buffer with RTC data from Peripheral


#define SYS_REPLY_READ_FAIL_MAYBE  0x00u
#define SYS_REPLY_BOOT_OK          0x01u
//#define SYS_REPLY_MAYBE_KBD_START  0x04u  // TODO: 0x0E in megaduck disasm, but 0x04 when tested and when logged.
#define SYS_REPLY_KBD_LEN            3u  // 3 payload bytes (excluding 1 length header byte)

#define MEGADUCK_KBD_BYTE_1_EXPECT   0x0Eu
#define MEGADUCK_SIO_BOOT_OK         0x01u

#define MEGADUCK_RX_MAX_PAYLOAD_LEN  14u // 13 data bytes + 1 checksum byte max reply length?

#define TIMEOUT_2_MSEC                  2u  // Used for hardware init counter sequence
#define TIMEOUT_100_MSEC              100u
#define TIMEOUT_200_MSEC              200u


extern uint8_t serial_cmd_0x09_reply_data;

extern volatile uint8_t megaduck_serial_rx_data;
extern volatile uint8_t megaduck_serial_status;
extern          uint8_t megaduck_serial_rx_buf[MEGADUCK_RX_MAX_PAYLOAD_LEN];
extern          uint8_t megaduck_serial_rx_buf_len;

void serial_io_wait_for_transfer_with_timeout(uint8_t);
void serial_io_send_byte(uint8_t);
void serial_io_enable_receive_byte(void);
bool megaduck_laptop_controller_init(void);
bool megaduck_laptop_init(void);

bool serial_io_send_command_and_receive_buffer(uint8_t);
bool serial_io_read_byte_with_msecs_timeout(uint8_t);

uint8_t serial_io_read_byte_no_timeout(void);


#endif // _MEGADUCK_LAPTOP_IO_H

