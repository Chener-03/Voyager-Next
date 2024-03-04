#include "Debug.h"
#include <stdarg.h>
#include <intrin.h>

UINT8 m_Data = 8;
UINT8 m_Stop = 1;
UINT8 m_Parity = 0;
UINT8 m_BreakSet = 0;


namespace DBG
{
    constexpr char alphabet[] = "0123456789ABCDEF";

    void SerialPortInitialize(UINT16 Port, UINTN Baudrate)
    {
		#ifndef NO_DBG_LOG
        // Map 5..8 to 0..3
        UINT8 Data = (UINT8)(m_Data - (UINT8)5);

        // Calculate divisor for baud generator
        UINTN Divisor = BAUDRATE_MAX / Baudrate;

        // Set communications format
        UINT8 OutputData = (UINT8)((DLAB << 7) | (m_BreakSet << 6) | (m_Parity << 3) | (m_Stop << 2) | Data);
        __outbyte((UINTN)(Port + LCR_OFFSET), OutputData);

        // Configure baud rate
        __outbyte((UINTN)(Port + BAUD_HIGH_OFFSET), (UINT8)(Divisor >> 8));
        __outbyte((UINTN)(Port + BAUD_LOW_OFFSET), (UINT8)(Divisor & 0xff));

        // Switch back to bank 0
        OutputData = (UINT8)((~DLAB << 7) | (m_BreakSet << 6) | (m_Parity << 3) | (m_Stop << 2) | Data);
        __outbyte((UINTN)(Port + LCR_OFFSET), OutputData);
		#endif

    }

    void DebugPrintDecimal(long long number)
    {
		#ifndef NO_DBG_LOG
        if (number < 0)
        {
            __outbyte(PORT_NUM, '-');
            number = -number;
        }

        for (auto d = 1000000000000000000; d != 0; d /= 10)
            if ((number / d) != 0)
            {
                __outbyte(PORT_NUM, alphabet[(number / d) % 10]);
            }
            else
            {
                __outbyte(PORT_NUM, '0');
            }

		#endif
    }

    void DebugPrintHex(UINT64 number, const bool show_zeros)
    {
		#ifndef NO_DBG_LOG
        for (auto d = 0x1000000000000000; d != 0; d /= 0x10)
            if (show_zeros || (number / d) != 0)
                __outbyte(PORT_NUM, alphabet[(number / d) % 0x10]);
		#endif

    }


    void Print(const char* format, ...)
    {
		#ifndef NO_DBG_LOG
        va_list args;
        va_start(args, format);
        while (format[0])
        {
            if (format[0] == '%')
            {
                switch (format[1])
                {
                case 'd':
                    DebugPrintDecimal(va_arg(args, int));
                    format += 2;
                    continue;
                case 'x':
                    DebugPrintHex(va_arg(args, UINT32), false);
                    format += 2;
                    continue;
                case 'l':
                    if (format[2] == 'l')
                    {
                        switch (format[3])
                        {
                        case 'd':
                            DebugPrintDecimal(va_arg(args, UINT64));
                            format += 4;
                            continue;
                        case 'x':
                            DebugPrintHex(va_arg(args, UINT64), false);
                            format += 4;
                            continue;
                        }
                    }
                    break;
                case 'p':
                    DebugPrintHex(va_arg(args, UINT64), true);
                    format += 2;
                    continue;
                }
            }

            __outbyte(PORT_NUM, format[0]);
            ++format;
        }
        va_end(args);
		#endif
    }

}
