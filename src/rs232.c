/*
  Hatari - rs232.c

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.

  RS-232 Communications

  This is similar to the Printing functions, we open a direct file
  (e.g. /dev/ttyS0) and send bytes over it.
  Using such method mimicks the ST exactly, and even allows us to connect
  to an actual ST! To wait for incoming data, we create a thread which copies
  the bytes into an input buffer. This method fits in with the internet code
  which also reads data into a buffer.
*/
char RS232_rcsid[] = "Hatari $Id: rs232.c,v 1.4 2004-01-12 12:21:44 thothy Exp $";

#include <SDL.h>
#include <SDL_thread.h>

#include "main.h"
#include "configuration.h"
#include "debug.h"
#include "rs232.h"


#define RS232_DEBUG 1

#if RS232_DEBUG
#define Dprintf(a) printf a
#else
#define Dprintf(a)
#endif


BOOL bConnectedRS232 = FALSE;       /* Connection to RS232? */
static FILE *hCom = NULL;           /* Handle to file */
SDL_Thread *RS232Thread = NULL;     /* Thread handle for reading incoming data */
//DCB dcb;                           // Control block
unsigned char InputBuffer_RS232[MAX_RS232INPUT_BUFFER];
int InputBuffer_Head=0, InputBuffer_Tail=0;


/*-----------------------------------------------------------------------*/
/*
  Initialize RS-232, start thread to wait for incoming data
  (we will open a connection when first bytes are sent).
*/
void RS232_Init(void)
{
#if 0   /* RS232 is untested yet, so it's still disabled at the moment */
	/* Create thread to wait for incoming bytes over RS-232 */
	RS232Thread = SDL_CreateThread(RS232_ThreadFunc, NULL);
	Dprintf(("RS232 thread has been created.\n"));
#endif
}


/*-----------------------------------------------------------------------*/
/*
  Close RS-232 connection and stop checking for incoming data.
*/
void RS232_UnInit(void)
{
	/* Close, kill thread and free resource */
	if (RS232Thread)
	{
		/* Instead of killing the thread directly, we should probably better
		   inform it via IPC so that it can terminate gracefully... */
		SDL_KillThread(RS232Thread);
		RS232Thread = NULL;
	}
	RS232_CloseCOMPort();
}


/*-----------------------------------------------------------------------*/
/*
  Open file on COM port
*/
BOOL RS232_OpenCOMPort(void)
{
	/* Create our COM file for input/output */
	bConnectedRS232 = FALSE;
	hCom = fopen(ConfigureParams.RS232.szDeviceFileName, "w+b"); 
	if (hCom != NULL)
	{
/*
		// Get any early notifications, for thread
		SetCommMask(hCom,EV_RXCHAR);
		// Create input/output buffers
		SetupComm(hCom,4096,4096);
		// Purge buffers
		PurgeComm(hCom,PURGE_TXABORT | PURGE_RXABORT | PURGE_TXCLEAR | PURGE_RXCLEAR);
*/
		/* Set defaults */
		RS232_SetConfig(9600,0,UCR_1STOPBIT|UCR_PARITY|UCR_ODDPARITY);

		/* Set all OK */
		bConnectedRS232 = TRUE;

		Dprintf(("Opened RS232 file: %s\n", ConfigureParams.RS232.szDeviceFileName));
	}

	return(bConnectedRS232);
}


/*-----------------------------------------------------------------------*/
/*
  Close file on COM port
*/
void RS232_CloseCOMPort(void)
{
	/* Do have file open? */
	if (bConnectedRS232)
	{
		/* Close */
		fclose(hCom);
		hCom=NULL;

		bConnectedRS232 = FALSE;

		Dprintf(("Closed RS232 file.\n"));
	}
}


/*-----------------------------------------------------------------------*/
/*
  Set hardware configuration of RS-232

  Ctrl:Communications parameters, (default 0)No handshake
    Bit 0: XOn/XOff
    Bit 1: RTS/CTS

  Ucr: USART Control Register
    Bit 1:0-Odd Parity, 1-Even Parity
    Bit 2:0-No Parity, 1-Parity
    Bits 3,4: Start/Stop bits
      0 0 0-Start, 0-Stop    Synchronous
      0 1 0-Start, 1-Stop    Asynchronous
      1 0 1-Start, 1.5-Stop  Asynchronous
      1 1 1-Start, 2-Stop    Asynchronous
    Bits 5,6: 'WordLength'
      0 0 ,8 Bits
      0 1 ,7 Bits
      1 0 ,6 Bits
      1 1 ,5 Bits
    Bit 7: Frequency from TC and RC
*/
void RS232_SetConfig(int Baud,short int Ctrl,short int Ucr)
{
	Dprintf(("RS232_SetConfig(%i,%i,%i)\n",Baud,(int)Ctrl,(int)Ucr));
/* FIXME */
/*  
  // Get current config
  memset(&dcb,0x0,sizeof(DCB));
  GetCommState(hCom, &dcb);
  // Set defaults
  BuildCommDCB("baud=9600 parity=N data=8 stop=1",&dcb);
  
  // Need XOn/XOff?
  if (Ctrl&CTRL_XON_XOFF) {
    dcb.fOutX = TRUE;
    dcb.fInX = TRUE;
  }
  // And RTS/CTS?
  if (Ctrl&CTRL_RTS_CTS)
    dcb.fRtsControl = RTS_CONTROL_HANDSHAKE;

  // Type of parity(if enabled)
  if (Ucr&UCR_EVENPARITY)
    dcb.Parity = EVENPARITY;
  else
    dcb.Parity = ODDPARITY;
  // Need parity?
  if (Ucr&UCR_PARITY)
    dcb.fParity = TRUE;
  // Number of stop bits
  switch(Ucr&UCR_STARTSTOP) {
    case UCR_0STOPBIT:        // PC doesn't appear to have no stop bits? Eh?
    case UCR_1STOPBIT:
      dcb.StopBits = ONESTOPBIT;
      break;
    case UCR_15STOPBIT:
      dcb.StopBits = ONE5STOPBITS;
      break;
    case UCR_2STOPBIT:
      dcb.StopBits = TWOSTOPBITS;
      break;
  }

  // And set
  SetCommState(hCom, &dcb);
*/
}


/*----------------------------------------------------------------------- */
/*
  Pass bytes from emulator to RS-232
*/
BOOL RS232_TransferBytesTo(unsigned char *pBytes, int nBytes)
{
	/* Do need to open a connection to RS232? */
	if (!bConnectedRS232)
	{
		/* Do have RS-232 enabled? */
		if (ConfigureParams.RS232.bEnableRS232)
			bConnectedRS232 = RS232_OpenCOMPort();
	}

	/* Have we connected to the RS232? */
	if (bConnectedRS232)
	{
		/* Send bytes directly to the COM file */
		if (fwrite(pBytes, 1, nBytes, hCom))
		{
			fflush(hCom);
		}

		/* Show icon on status bar */
		/*StatusBar_SetIcon(STATUS_ICON_RS232,ICONSTATE_UPDATE);*/

		return(TRUE);   /* OK */
	}
	else
		return(FALSE);  /* Failed */
}


/*-----------------------------------------------------------------------*/
/*
  Read characters from our internal input buffer(bytes from other machine)
*/
BOOL RS232_ReadBytes(unsigned char *pBytes, int nBytes)
{
	int i;

	/* Connected? */
	if (bConnectedRS232)
	{
		/* Read bytes out of input buffer */
		for (i=0; i<nBytes; i++)
		{
			*pBytes++ = InputBuffer_RS232[InputBuffer_Head];
			InputBuffer_Head = (InputBuffer_Head+1) % MAX_RS232INPUT_BUFFER;
		}
		return(TRUE);
	}

	return(FALSE);
}


/*-----------------------------------------------------------------------*/
/*
  Return TRUE if bytes waiting!
*/
BOOL RS232_GetStatus(void)
{
	/* Connected? */
	if (bConnectedRS232)
	{
		/* Do we have bytes in the input buffer? */
		if (InputBuffer_Head != InputBuffer_Tail)
			return(TRUE);
	}

	/* No, none */
	return(FALSE);
}


/*-----------------------------------------------------------------------*/
/*
  Add incoming bytes from other machine into our input buffer
*/
void RS232_AddBytesToInputBuffer(unsigned char *pBytes, int nBytes)
{
	int i;

	/* Copy bytes into input buffer */
	for (i=0; i<nBytes; i++)
	{
		InputBuffer_RS232[InputBuffer_Tail] = *pBytes++;
		InputBuffer_Tail = (InputBuffer_Tail+1) % MAX_RS232INPUT_BUFFER;
	}
}


/*-----------------------------------------------------------------------*/
/*
  Thread to read incoming RS-232 data, and pass to emulator input buffer
*/
int RS232_ThreadFunc(void *pData)
{
	int iInChar;
	char cInChar;

	/* Check for any RS-232 incoming data */
	while (TRUE)
	{
		if (hCom)
		{
			/* Read the bytes in, if we have any */
			iInChar = fgetc(hCom);
			if (iInChar != EOF)
			{
				/* Copy into our internal queue */
				cInChar = iInChar;
				RS232_AddBytesToInputBuffer(&cInChar, 1);
				Dprintf(("RS232: Read character $%x\n", cInChar));
			}
			else
			{
				Dprintf(("RS232: Reached end of file!"));
			}

			/* Sleep for a while */
			SDL_Delay(2);
		}
		else
		{
			/* No RS-232 connection, sleep for 20ms */
			SDL_Delay(20);
		}
	}

	return(TRUE);
}
