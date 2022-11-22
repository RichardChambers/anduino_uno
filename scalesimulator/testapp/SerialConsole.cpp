// SerialConsole.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

/*
 * Weight-Tronix  SCP-01
 * SCP-01 (NCI STANDARD, and 3825)
 * SERIAL COMMUNICATIONS PROTOCOL:
 * 
 *  https://www.averyweigh-tronix.com/globalassets/products/postal-scales/postal-software-downloads-and-drivers/nci-protocol-serial-rs-232/wtcomm-activex-control/document-3--nci-standard.pdf
 * 
 * Scale Request
 * The device connected to a scale sends a request message requesting a scale reading.
 *
 * Key to symbols used in the message descriptions following:
 *     <ETX> End of Text character (0x03).
 *     <LF>  Line Feed character (0x0A).
 *     <CR>  Carriage Return character (0x0D).
 *     <SP>  Space (0x20).
 *     x     Weight characters from display including minus sign and out-of range characters.
 *     c     Message/menu (ie non-weight) characters from display.
 *     hh... Two or more status bytes.
 *     uu    Units of measure (lb, kg, oz, g, etc. using ANSI standard abbreviations).
 *
 * Name: Request weight
 *     Command:  W<CR>
 *     Response: Returns decimal weight (x1), units and status.
 *               <LF>xxxx.xxuu<CR><LF>hh...<CR><ETX>
 *
 *               Returns lb-oz weight, units plus scale status.
 *               <LF>xxlb<SP>xx.xoz<CR><LF>hh...<CR><ETX>
 *
 *               Returns contents of display (other than wt) with units and scale status.
 *               <LF>cccccccuu<CR><LF>hh...<CR><ETX
 *
 * Name: Change units of measure
 *     Command:  U<CR>
 *     Response: Changes units of measure, returns new units and scale status.
 *               <LF>uu<CR><LF>hh...<CR><ETX>
 *
 * Name: Request status
 *     Command:  S<CR>
 *     Response: Returns scale status.
 *               <LF>hh...<CR><ETX>
 *
 *               Bit 0 is the least significant bit in the byte while bit 7 is the most significant bit.
 *               Bit   Status Byte 1        Status Byte 2             Status Byte 3 (opt)
 *                0    1 = Scale in motion   1 = Under capacity         00 = Low range
 *                     0 = Stable            0 = Not under capacity     01 = (undefined)
 *                                                                      10 = (undefined)
 *                1    1 = Scale at zero     1 = Over capacity          11 = High range
 *                     0 = Not at zero       0 = Not over capacity
 *
 *                2    1 = RAM error         1 = ROM error               1 = Net weight
 *                     0 = RAM okay          0 = ROM okay                0 = Gross weight
 *
 *                3    1 = EEPROM error      1 = Faulty calibration      1 = Initial zero error
 *                     0 = EEPROM okay       0 = Calibration okay        0 = Initial zero OK
 *
 *                4    Always 1              Always 1                    Always 1
 *
 *                5    Always 1              Always 1                    Always 1
 *
 *                6    Always 0              1 = Byte follows            1 = Byte follows
 *                                           0 = Last byte               0 = Last byte
 *
 *                7    Parity                Parity                      Parity
 *
 *
 * Name: unrecognized command
 *     Command:  all others
 *     Response: Unrecognized command
 *               <LF>?<CR><ETX>
 * 
*/
#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <iostream>

#define PIF_OK  1

#define PIF_ERROR_SYSTEM                (HANDLE)(-1)

#define PIF_ERROR_FILE_EXIST            (HANDLE)(-5)
#define PIF_ERROR_FILE_EOF              (HANDLE)(-6)
#define PIF_ERROR_FILE_DISK_FULL        (HANDLE)(-7)
#define PIF_ERROR_FILE_NOT_FOUND        (HANDLE)(-8)

#define PIF_ERROR_COM_POWER_FAILURE     (HANDLE)(-2)
#define PIF_ERROR_COM_TIMEOUT           (HANDLE)(-3)
#define PIF_ERROR_COM_NOT_PROVIDED      (HANDLE)(-4)
#define PIF_ERROR_COM_BUSY              (HANDLE)(-5)
#define PIF_ERROR_COM_EOF               (HANDLE)(-6)
#define PIF_ERROR_COM_ABORTED           (HANDLE)(-9)
#define PIF_ERROR_COM_ACCESS_DENIED     (HANDLE)(-10)      // same as PIF_ERROR_NET_ACCESS_DENIED
#define PIF_ERROR_COM_ERRORS            (HANDLE)(-10)      // same as PIF_ERROR_NET_ERRORS
#define PIF_ERROR_COM_OFFLINE           (HANDLE)(-13)
#define PIF_ERROR_COM_LOOT_TEST         (HANDLE)(-55)
#define PIF_ERROR_COM_NO_DSR_AND_CTS    (HANDLE)(-57)
#define PIF_ERROR_COM_NO_CTS            (HANDLE)(-59)
#define PIF_ERROR_COM_NO_DSR            (HANDLE)(-60)
#define PIF_ERROR_COM_OVERRUN           (HANDLE)(-62)
#define PIF_ERROR_COM_FRAMING           (HANDLE)(-63)
#define PIF_ERROR_COM_PARITY            (HANDLE)(-64)
#define PIF_ERROR_COM_MONITOR           (HANDLE)(-66)
#define PIF_ERROR_COM_BUFFER_OVERFLOW   (HANDLE)(-150)
#define PIF_ERROR_COM_NO_INTERRUPT      (HANDLE)(-151)
#define PIF_ERROR_COM_TIMEOUT_M_L       (HANDLE)(-162)
#define PIF_ERROR_COM_COMM_M_L          (HANDLE)(-163)

#define COM_BYTE_ODD_PARITY     0x08
#define COM_BYTE_EVEN_PARITY    0x18
#define COM_BYTE_2_STOP_BITS    0x04
#define COM_BYTE_7_BITS_DATA    0x02
#define COM_BYTE_8_BITS_DATA    0x03

// for device config option for handshake with serial connections JHHJ 9-13-05
#define COM_BYTE_HANDSHAKE_NONE		0x01
#define COM_BYTE_HANDSHAKE_RTSCTS	0x02
#define COM_BYTE_HANDSHAKE_CTS		0x04
#define COM_BYTE_HANDSHAKE_RTS		0x08
#define COM_BYTE_HANDSHAKE_XONOFF	0x10
#define COM_BYTE_HANDSHAKE_DTRDSR	0x20

typedef struct {
    SHORT   fPip;
    USHORT  usPipAddr;
    USHORT  usComBaud;
    UCHAR   uchComByteFormat;
    UCHAR   uchComTextFormat;
    UCHAR   auchComNonEndChar[4];
    UCHAR   auchComEndChar[3];
    UCHAR   uchComDLEChar;
    UCHAR   auchComHandShakePro;
} PROTOCOL;


HANDLE   PifOpenCom(USHORT usPortId, CONST PROTOCOL* pProtocol)
{
    TCHAR   wszPortName[16] = { 0 };
    HANDLE  hHandle;
    DWORD dwError;
    DCB dcb = { 0 };
    COMMTIMEOUTS comTimer = { 0 };
    DWORD   dwCommMasks;
    DWORD   dwBaudRate;                 // baud rate
    DWORD   nCharBits, nMultiple;
    BYTE    bByteSize;                  // number of bits/byte, 4-8
    BYTE    bParity;                    // 0-4 = no, odd, even, mark, space
    BYTE    bStopBits;                  // 0,1,2 = 1, 1.5, 2
    BOOL    fResult;

    // see Microsoft document HOWTO: Specify Serial Ports Larger than COM9.
    // https://support.microsoft.com/en-us/kb/115831
    // CreateFile() can be used to get a handle to a serial port. The "Win32 Programmer's Reference" entry for "CreateFile()"
    // mentions that the share mode must be 0, the create parameter must be OPEN_EXISTING, and the template must be NULL. 
    //
    // CreateFile() is successful when you use "COM1" through "COM9" for the name of the file;
    // however, the value INVALID_HANDLE_VALUE is returned if you use "COM10" or greater. 
    //
    // If the name of the port is \\.\COM10, the correct way to specify the serial port in a call to
    // CreateFile() is "\\\\.\\COM10".
    //
    // NOTES: This syntax also works for ports COM1 through COM9. Certain boards will let you choose
    //        the port names yourself. This syntax works for those names as well.
    wsprintf(wszPortName, TEXT("\\\\.\\COM%d"), usPortId);

    /* Open the serial port. */
    /* avoid to failuer of CreateFile */
//    for (i = 0; i < 10; i++) {
    do {
        hHandle = CreateFile(wszPortName, /* Pointer to the name of the port, PifOpenCom() */
            GENERIC_READ | GENERIC_WRITE,  /* Access (read-write) mode */
            0,            /* Share mode */
            NULL,         /* Pointer to the security attribute */
            OPEN_EXISTING,/* How to open the serial port */
            0,            /* Port attributes */
            NULL);        /* Handle to port with attribute */
                          /* to copy */

/* If it fails to open the port, return FALSE. */
        if (hHandle == INVALID_HANDLE_VALUE) {    /* Could not open the port. */
            dwError = GetLastError();
            if (dwError == ERROR_FILE_NOT_FOUND || dwError == ERROR_INVALID_NAME || dwError == ERROR_ACCESS_DENIED) {
                // the COM port does not exist. probably a Virtual Serial Communications Port
                // from a USB device which was either unplugged or turned off.
                // or the COM port or Virtual Serial Communications port is in use by some other application.
                return PIF_ERROR_COM_ACCESS_DENIED;
            }
        }
        else {
            break;
        }
        //   }
    } while (0);
    if (hHandle == INVALID_HANDLE_VALUE) {    /* Could not open the port. */
        return PIF_ERROR_COM_ERRORS;
    }

    /* clear the error and purge the receive buffer */
    dwError = (DWORD)(~0);                  // set all error code bits on
    ClearCommError(hHandle, &dwError, NULL);
    PurgeComm(hHandle, PURGE_TXABORT | PURGE_RXABORT | PURGE_TXCLEAR | PURGE_RXCLEAR);


    /* make up comm. parameters */

    dwBaudRate = pProtocol->usComBaud;
    if (dwBaudRate == 0) {
        CloseHandle(hHandle);
        return PIF_ERROR_COM_ERRORS;
    }

    if ((pProtocol->uchComByteFormat & (COM_BYTE_7_BITS_DATA | COM_BYTE_8_BITS_DATA)) == COM_BYTE_7_BITS_DATA) {
        bByteSize = 7;
    }
    else {
        bByteSize = 8;
    }
    if ((pProtocol->uchComByteFormat & COM_BYTE_2_STOP_BITS) == COM_BYTE_2_STOP_BITS) {
        bStopBits = TWOSTOPBITS;
    }
    else {
        bStopBits = ONESTOPBIT;
    }
    if ((pProtocol->uchComByteFormat & (COM_BYTE_ODD_PARITY | COM_BYTE_EVEN_PARITY)) == COM_BYTE_EVEN_PARITY) {
        bParity = EVENPARITY;
    }
    else
        if ((pProtocol->uchComByteFormat & (COM_BYTE_ODD_PARITY | COM_BYTE_EVEN_PARITY)) == COM_BYTE_ODD_PARITY) {
            bParity = ODDPARITY;
        }
        else {
            bParity = NOPARITY;
        }

    /* Get the default port setting information. */
    GetCommState(hHandle, &dcb);


    /* set up no flow control as default */

    dcb.BaudRate = dwBaudRate;              // Current baud 
    dcb.fBinary = TRUE;               // Binary mode; no EOF check 
    dcb.fParity = (bParity != NOPARITY);               // Enable parity checking 
    dcb.ByteSize = bByteSize;                 // Number of bits/byte, 4-8
    dcb.Parity = bParity;            // 0-4=no,odd,even,mark,space 
    dcb.StopBits = bStopBits;        // 0,1,2 = 1, 1.5, 2 
    dcb.fDsrSensitivity = FALSE;      // DSR sensitivity 

    //if ((usFlag & KITCHEN_PORT_FLAG)) {
//  if (pProtocol->fPip == PIF_COM_PROTOCOL_XON) { //
    if (pProtocol->auchComHandShakePro & COM_BYTE_HANDSHAKE_XONOFF)
    {

        dcb.fOutxCtsFlow = FALSE;         // No CTS output flow control 
        dcb.fOutxDsrFlow = TRUE;          // No DSR output flow control for 7161
        dcb.fTXContinueOnXoff = TRUE;                       /* XOFF continues Tx */
        dcb.fOutX = TRUE;                       /* XON/XOFF out flow control */
        dcb.fInX = TRUE;                       /* XON/XOFF in flow control */
        dcb.XonChar = 0x11;	//The default value for this property is the ASCII/ANSI value 17
        dcb.XoffChar = 0x13;	//The default value for this property is the ASCII/ANSI value 19

    }
    else
        /* set up RTS/CTS flow control by option in device configulation*/
        if (pProtocol->auchComHandShakePro & COM_BYTE_HANDSHAKE_RTSCTS) {

            dcb.fOutxCtsFlow = TRUE;         // CTS output flow control 
            dcb.fOutxDsrFlow = FALSE;         // No DSR output flow control 
            dcb.fTXContinueOnXoff = FALSE;     // XOFF continues Tx heee
            dcb.fOutX = FALSE;                // No XON/XOFF out flow control 
            dcb.fInX = FALSE;                 // No XON/XOFF in flow control 
            dcb.fRtsControl = RTS_CONTROL_HANDSHAKE;
            dcb.fDsrSensitivity = FALSE;      // DSR sensitivity 
        }
        else
            //Set up RTS flow control by option in device configulation
            if (pProtocol->auchComHandShakePro & COM_BYTE_HANDSHAKE_RTS) {

                dcb.fOutxCtsFlow = FALSE;         // no CTS output flow control 
                dcb.fOutxDsrFlow = FALSE;         // No DSR output flow control 
                dcb.fTXContinueOnXoff = FALSE;     // XOFF continues Tx 
                dcb.fOutX = FALSE;                // No XON/XOFF out flow control 
                dcb.fInX = FALSE;                 // No XON/XOFF in flow control 
                dcb.fRtsControl = RTS_CONTROL_HANDSHAKE;
                dcb.fDsrSensitivity = FALSE;      // DSR sensitivity
            }
            else
                //Set up CTS flow control by option in device configulation
                if (pProtocol->auchComHandShakePro & COM_BYTE_HANDSHAKE_CTS) {

                    dcb.fOutxCtsFlow = TRUE;         // CTS output flow control 
                    dcb.fOutxDsrFlow = FALSE;         // No DSR output flow control 
                    dcb.fTXContinueOnXoff = FALSE;     // XOFF continues Tx 
                    dcb.fOutX = FALSE;                // No XON/XOFF out flow control 
                    dcb.fInX = FALSE;                 // No XON/XOFF in flow control 
                    dcb.fRtsControl = FALSE;
                    dcb.fDsrSensitivity = FALSE;      // DSR sensitivity
                }
                else
                    //Set up DSR/DTR flow control by option in device configulation
                    if (pProtocol->auchComHandShakePro & COM_BYTE_HANDSHAKE_DTRDSR) {


                        dcb.fOutxCtsFlow = FALSE;         // CTS output flow control 
                        dcb.fOutxDsrFlow = TRUE;         // DSR output flow control 
                        dcb.fDtrControl = DTR_CONTROL_HANDSHAKE; //
                        dcb.fTXContinueOnXoff = FALSE;     // XOFF continues Tx 
                        dcb.fOutX = FALSE;                // No XON/XOFF out flow control 
                        dcb.fInX = FALSE;                 // No XON/XOFF in flow control 
                        dcb.fRtsControl = FALSE;
                        dcb.fDsrSensitivity = FALSE;      // DSR sensitivity
                    }


    /* Configure the port according to the specifications of the DCB */
    /* structure. */
    fResult = SetCommState(hHandle, &dcb);
    if (!fResult) {
        /* Could not create the read thread. */
        dwError = GetLastError();
        CloseHandle(hHandle);
        return PIF_ERROR_COM_ERRORS;
    }

    /* set up default time out value */

    /* compute no. of bits / data */
    nCharBits = 1 + bByteSize;
    nCharBits += (bParity == NOPARITY) ? 0 : 1;
    nCharBits += (bStopBits == ONESTOPBIT) ? 1 : 2;
    nMultiple = (2 * CBR_9600 / dwBaudRate);

    fResult = GetCommTimeouts(hHandle, &comTimer);
    //Windows CE default values are listed next to the variables
    //These were verified on Windows CE
    //These default values were different on Windows XP
    //so set the values to the CE defaults
    comTimer.ReadIntervalTimeout = 250;         /* 250 default of CE Emulation driver */
    comTimer.ReadTotalTimeoutMultiplier = 10;          /* 10 default of CE Emulation driver */
    comTimer.ReadTotalTimeoutConstant = 2000;        /* read within 2000 msec (100 default of CE Emulation driver) */
    comTimer.WriteTotalTimeoutMultiplier = 0;           /* 0 default of CE Emulation driver */
    comTimer.WriteTotalTimeoutConstant = 1000;        /* allow 1000 msec to write (0 default of CE Emulation driver) */

    fResult = SetCommTimeouts(hHandle, &comTimer);
    if (!fResult)
    {
        dwError = GetLastError();
        CloseHandle(hHandle);
        return PIF_ERROR_COM_ERRORS;
    }

    /* Direct the port to perform extended functions SETDTR and SETRTS */
    /* SETDTR: Sends the DTR (data-terminal-ready) signal. */
    /* SETRTS: Sends the RTS (request-to-send) signal. */
    EscapeCommFunction(hHandle, SETDTR);
    EscapeCommFunction(hHandle, SETRTS);

    /* make up comm. event mask */
    dwCommMasks = EV_CTS | EV_DSR | EV_ERR | EV_RLSD | EV_RXCHAR;

    /* set up comm. event mask */
    fResult = SetCommMask(hHandle, dwCommMasks);
    if (!fResult)
    {
        dwError = GetLastError();
        CloseHandle(hHandle);
        return PIF_ERROR_COM_ERRORS;
    }

    /* clear the error and purge the receive buffer */

//  dwErrors = (DWORD)(~0);                 // set all error code bits on
//  fResult  = ClearCommError(hComm, &dwErrors, NULL);
//  fResult  = PurgeComm(hComm, PURGE_RXABORT | PURGE_RXCLEAR);


    return hHandle;
}

/*fhfh
**********************************************************************
**                                                                  **
**  Synopsis:   SHORT PIFENTRY PifReadCom(USHORT usPort           **
**                                          VOID FAR *pBuffer       **
**                                          USHORT usBytes)          **
**              usFile:         com handle                         **
**              pBuffer:        reading buffer                      **
**              usBytes:        sizeof pBuffer                      **
**                                                                  **
**  return:     number of bytes to be read                          **
**                                                                  **
**  Description:reading from serial i/o port                        **
**                                                                  **
**********************************************************************
fhfh*/
SHORT  PifReadCom(HANDLE  hHandle, void * pBuffer, USHORT usBytes)
{
    DWORD   dwBytesRead;
    BOOL    fResult;
    DWORD   dwError;

    //    fResult = ClearCommError(hHandle, &dwErrors, &stat);
    fResult = ReadFile(hHandle, pBuffer, (DWORD)usBytes, &dwBytesRead, NULL);

    if (fResult) {
        if (!dwBytesRead) return (SHORT)PIF_ERROR_COM_TIMEOUT;
        return (SHORT)dwBytesRead;
    }
    else {
        SHORT  sErrorCode = 0;     // error code from PifSubGetErrorCode(). must call after GetLastError().
        dwError = GetLastError();
        return (sErrorCode);
    }
}

/*fhfh
**********************************************************************
**                                                                  **
**  Synopsis:   SHORT PIFENTRY PifWriteCom(USHORT usPort           **
**                                          VOID FAR *pBuffer       **
**                                          USHORT usBytes)          **
**              usFile:         com handle                         **
**              pBuffer:        reading buffer                      **
**              usBytes:        sizeof pBuffer                      **
**                                                                  **
**  return:     number of bytes to be written                       **
**                                                                  **
**  Description:writing to serial i/o port                          **
**                                                                  **
**********************************************************************
fhfh*/
SHORT  PifWriteCom(HANDLE  hHandle, const void * pBuffer,
    USHORT usBytes)
{
    DWORD dwBytesWritten;
    BOOL    fResult;
    DWORD   dwError;

    fResult = WriteFile(hHandle, pBuffer, (DWORD)usBytes, &dwBytesWritten, NULL);

    if (fResult) {
        if ((usBytes != dwBytesWritten) && (dwBytesWritten == 0)) {
            PurgeComm(hHandle, PURGE_TXCLEAR);
            return (SHORT)PIF_ERROR_COM_TIMEOUT;
        }
        return (SHORT)dwBytesWritten;
    }
    else {
        SHORT  sErrorCode = 0;     // error code from PifSubGetErrorCode(). must call after GetLastError().

        dwError = GetLastError();
        return (sErrorCode);
    }
}


/*fhfh
**********************************************************************
**                                                                  **
**  Synopsis:   VOID PIFENTRY PifCloseCom(USHORT usPort)           **
**              usPort:         com handle                         **
**                                                                  **
**  return:     none                                                **
**                                                                  **
**  Description:closing serial i/o                                  **
**                                                                  **
**********************************************************************
fhfh*/
VOID   PifCloseCom(HANDLE  hHandle)
{
    BOOL    fReturn;
    if (hHandle != INVALID_HANDLE_VALUE) {
        fReturn = CloseHandle(hHandle);
    }

    return;
}

struct ScaleStatus {
    int  iError;
    unsigned char  s1;
    unsigned char  s2;
    unsigned char  s3;
};

ScaleStatus parseResponseStatus(char* pBuff)
{
	// two forms of the status byte sequence in a response.
	//  - SCP-01  ->  <LF>hh...<CR><ETX>
	//  - SCP-02  ->  <LF>Shh...<CR><ETX>

    ScaleStatus st = { 0 };
    int   iNdex = 2;

	if (pBuff[0] == 'S') {
		// SCP-02 protocol detected. skip the S character to get to status bytes.
		pBuff++;
	}

	st.s1 = pBuff[0];
	st.s2 = pBuff[1];
    if (st.s2 & 0x40) {
        // byte follows bit is turned on so lets get the next byte.
        st.s3 = pBuff[2];
		iNdex++;
	}

    // check that first two bytes of status have the always on bit set.
    if ((pBuff[0] & 0x30) != 0x30)
        st.iError = 1;
    if ((pBuff[1] & 0x30) != 0x30)
        st.iError = 2;
    if ((st.s2 & 0x40) && (pBuff[2] & 0x30) != 0x30)
        st.iError = 3;

    // check that message is terminated by a carriage return and an ETX character.
    if (pBuff[iNdex] != '\r' && pBuff[iNdex+1] != 0x03)
        st.iError = 4;

    return st;
}

void parseResponseWeight(char *pBuff)
{
    if (pBuff[0] == '\n') {
        const int iStateEnd = 5;
        int iState = 0, ndxBuff;
        int iMsb = 0, iLsb = 0;
        int iUnits = 0;

        for (ndxBuff = 1; pBuff[ndxBuff] && iState < iStateEnd; ) {
            switch (iState) {
            case 0:    // parsing out Most Significant part of weight
                if (!isdigit(pBuff[ndxBuff])) {
                    iState++;
                }
                else {
                    iMsb *= 10;
                    iMsb += pBuff[ndxBuff] - '0';
                    ndxBuff++;
                }
                break;
            case 1:    // should be a decimal point. if not data error
                if (pBuff[ndxBuff] == '.') {
                    iState++;
                    ndxBuff++;
                }
                else if (pBuff[ndxBuff] == '?') {
					// unrecognized command response is "\n?\r\x03"
                    iState = 200;
                }
                else {
                    iState = 100;
                }
                break;
            case 2:    // parsing out Least Significant part of weight
                if (!isdigit(pBuff[ndxBuff])) {
                    iState++;
                }
                else {
                    iLsb *= 10;
                    iLsb += pBuff[ndxBuff] - '0';
                    ndxBuff++;
                }
                break;
            case 3:    // parsing out units of measurement
                switch (pBuff[ndxBuff]) {
                case 'l':
                case 'L':
                    ndxBuff++;
                    switch (pBuff[ndxBuff]) {
                    case 'b':
                    case 'B':
                        iState++;
                        ndxBuff++;
                        iUnits = 0;
                        break;
                    default:
                        iState = 101;
                        break;
                    }
                    break;
                case 'k':
                case 'K':
                    ndxBuff++;
                    switch (pBuff[ndxBuff]) {
                    case 'g':
                    case 'G':
                        iState++;
                        ndxBuff++;
                        iUnits = 1;
                        break;
                    default:
                        iState = 101;
                        break;
                    }
                    break;
                default:
                    iState = 101;
                    break;
                }
                break;
            case 4:
                if (pBuff[ndxBuff] == '\r') {
                    ndxBuff++;
                    if (pBuff[ndxBuff] == '\n') {
                        iState++;
                        ndxBuff++;
                    }
                    else {
                        iState = 102;
                    }
                } else {
                    iState = 102;
                }
                break;
            case iStateEnd:
                break;
            default:
                break;
            }
        }

        if (iState == iStateEnd) {
            ScaleStatus  st = parseResponseStatus(pBuff + ndxBuff);
            printf("  Response:  weight %d.%d %s  status 0x%1.1x 0x%1.1x\n", iMsb, iLsb, ((iUnits == 0) ? "lb" : "kg"), st.s1, st.s2);
        }
        else if (iState == 200) {
            printf("  Unrecognized command.\n");
        }
        else {
            printf("  Error in response %d at index %d.\n", iState, ndxBuff);
        }
    }
}

void printHelp()
{
    printf("Commands\n");
    printf("   w  - ask for weight from scale.\n");
    printf("   s  - ask for status from scale.\n");
    printf("   z  - zero scale.\n");
    printf("   p  - set port number and open port.\n");
    printf("   h  - print this help text.\n");
    printf("   e  - exit.\n");

    fflush(stdout);
}

int main()
{
    printHelp();

    SHORT     sPortId = -1;
    PROTOCOL  Protocol = { 0 };

    Protocol.usComBaud = 9600;
    Protocol.auchComHandShakePro |= COM_BYTE_HANDSHAKE_XONOFF;

    HANDLE   hPort = INVALID_HANDLE_VALUE;

    do {
        char buf[32] = { 0 };
        USHORT  usBytes = 0;
        int     iRead = 0;

        printf("> "); fflush(stdout);

        char xBuff[128];
        char* ptr;
        ptr = fgets(xBuff, 128, stdin);

        if (xBuff[0] == 'e' || xBuff[0] == 'x') break;

        switch (xBuff[0]) {
        case 'w':
        case 'W':
            strcpy(buf, "W\r");
            usBytes = strlen(buf);
            PifWriteCom(hPort, &buf, usBytes);
            iRead = 1;
            break;
        case 's':
        case 'S':
            strcpy(buf, "S\r");
            usBytes = strlen(buf);
            PifWriteCom(hPort, &buf, usBytes);
            iRead = 1;
            break;
        case 'z':
        case 'Z':
            strcpy(buf, "Z\r");
            usBytes = strlen(buf);
            PifWriteCom(hPort, &buf, usBytes);
            iRead = 1;
            break;
        case 'p':
        case 'P':
            PifCloseCom(hPort);
            sPortId = atoi(xBuff + 1);
            hPort = PifOpenCom(sPortId, &Protocol);
            if ((long)hPort < 0) {
                printf("ERROR: open port failed code %d\n", (long)hPort);
                hPort = INVALID_HANDLE_VALUE;
            }
            break;
        case 'h':
        case 'H':
        default:
            printHelp();
            break;
        }

        if (iRead) {
            char bufin[128] = { 0 };

            short sRet = PifReadCom(hPort, &bufin, sizeof(bufin));
            if (bufin[0] != '\n') {
                printf("  Response has incorrect format.\n");
            } else if (bufin[1] == '?') {
                printf("  Response: Unrecognized command.\n");
            }
            else {
                switch (buf[0]) {
                case 'z':
                case 'Z':
                case 's':
                case 'S':
                    ScaleStatus  st = parseResponseStatus(bufin + 1);
                    if (st.iError == 0) {
                        printf("  Response:  status 0x%1.1x 0x%1.1x\n", st.s1, st.s2);
                    } 
                    else {
                        printf("  Error in response: iError = %d.\n", st.iError);
                    }
                    break;
                case 'w':
                case 'W':
                    parseResponseWeight(bufin);
                    break;
                }
            }
        }

    } while (1);

    PifCloseCom(hPort);

}
