#pragma once
#include "Defs.h"

// #define NO_DBG_LOG

#define SERIAL_PORT_0 0x3F8 /* COM1, ttyS0 */
#define SERIAL_PORT_1 0x2F8 /* COM2, ttyS1 */
#define SERIAL_PORT_2 0x3E8 /* COM3, ttyS2 */
#define SERIAL_PORT_3 0x2E8 /* COM4, ttyS3 */

#define PORT_NUM SERIAL_PORT_1

/*
	UART Register Offsets
*/
#define BAUD_LOW_OFFSET         0x00
#define BAUD_HIGH_OFFSET        0x01
#define IER_OFFSET              0x01
#define LCR_SHADOW_OFFSET       0x01
#define FCR_SHADOW_OFFSET       0x02
#define IR_CONTROL_OFFSET       0x02
#define FCR_OFFSET              0x02
#define EIR_OFFSET              0x02
#define BSR_OFFSET              0x03
#define LCR_OFFSET              0x03
#define MCR_OFFSET              0x04
#define LSR_OFFSET              0x05
#define MSR_OFFSET              0x06

/*
	UART Register Bit Defines
*/
#define LSR_TXRDY               0x20
#define LSR_RXDA                0x01
#define DLAB                    0x01

#define BAUDRATE_MAX 115200


typedef unsigned long long UINTN;


namespace DBG
{
	void SerialPortInitialize(UINT16 Port, UINTN Baudrate);
	void DebugPrintDecimal(long long number);
	void DebugPrintHex(UINT64 number, const bool show_zeros);
	void Print(const char* format, ...);
}
