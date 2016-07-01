/*===================================================================*/
/*                                                                   */
/*  InfoNES_System_Linux.cpp : Linux specific File                   */
/*                                                                   */
/*  2001/05/18  InfoNES Project ( Sound is based on DarcNES )        */
/*                                                                   */
/*===================================================================*/

/*-------------------------------------------------------------------*/
/*  Include files                                                    */
/*-------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/soundcard.h>

#include "../InfoNES.h"
#include "../InfoNES_System.h"
#include "../InfoNES_pAPU.h"

//bool define
#define TRUE 1
#define FALSE 0

/* lcd ������� ͷ�ļ� */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <termios.h>

#include <fcntl.h>

#define JOYPAD_DEV "/dev/joypad"
static int joypad_fd;

static int fb_fd;
static unsigned char *fb_mem;
static int px_width;
static int line_width;
static int screen_width;
static struct fb_var_screeninfo var;

static int init_joypad()
{
	joypad_fd = open(JOYPAD_DEV, O_RDONLY);
	if(-1 == joypad_fd)
	{
		printf("joypad dev not found \r\n");
		return -1;
	}
	return 0;
}

static int lcd_fb_display_px(WORD color, int x, int y)
{
	unsigned char  *pen8;
	unsigned short *pen16;
	
	pen8 = (unsigned char *)(fb_mem + y*line_width + x*px_width);
	pen16 = (unsigned short *)pen8;
	*pen16 = color;
	
	return 0;
}

static int lcd_fb_init()
{
	//���ʹ�� mmap �򿪷�ʽ ������ ������ʽ
	fb_fd = open("/dev/fb0", O_RDWR);
	if(-1 == fb_fd)
	{
		printf("cat't open /dev/fb0 \n");
		return -1;
	}
	//��ȡ��Ļ����
	if(-1 == ioctl(fb_fd, FBIOGET_VSCREENINFO, &var))
	{
		close(fb_fd);
		printf("cat't ioctl /dev/fb0 \n");
		return -1;
	}
	
	//�������
	px_width = var.bits_per_pixel / 8;
	line_width = var.xres * px_width;
	screen_width = var.yres * line_width;
	
	fb_mem = (unsigned char *)mmap(NULL, screen_width, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
	if(fb_mem == (void *)-1)
	{
		close(fb_fd);
		printf("cat't mmap /dev/fb0 \n");
		return -1;
	}
	//����
	memset(fb_mem, 0 , screen_width);
	return 0;
}

/*-------------------------------------------------------------------*/
/*  ROM image file information                                       */
/*-------------------------------------------------------------------*/

char	szRomName[256];
char	szSaveName[256];
int	nSRAM_SaveFlag;

/*-------------------------------------------------------------------*/
/*  Constants ( Linux specific )                                     */
/*-------------------------------------------------------------------*/

#define VBOX_SIZE	7
#define SOUND_DEVICE	"/dev/dsp"
#define VERSION		"InfoNES v0.91J"

/*-------------------------------------------------------------------*/
/*  Global Variables ( Linux specific )                              */
/*-------------------------------------------------------------------*/

/* Emulation thread */
pthread_t  emulation_tid;
int bThread;

/* Pad state */
DWORD	dwKeyPad1;
DWORD	dwKeyPad2;
DWORD	dwKeySystem;

/* For Sound Emulation */
BYTE	final_wave[2048];
int	waveptr;
int	wavflag;
int	sound_fd;

/*-------------------------------------------------------------------*/
/*  Function prototypes ( Linux specific )                           */
/*-------------------------------------------------------------------*/

void *emulation_thread( void *args );


void start_application( char *filename );


int LoadSRAM();


int SaveSRAM();


/* Palette data */
WORD NesPalette[64] =
{
	0x39ce, 0x1071, 0x0015, 0x2013, 0x440e, 0x5402, 0x5000, 0x3c20,
	0x20a0, 0x0100, 0x0140, 0x00e2, 0x0ceb, 0x0000, 0x0000, 0x0000,
	0x5ef7, 0x01dd, 0x10fd, 0x401e, 0x5c17, 0x700b, 0x6ca0, 0x6521,
	0x45c0, 0x0240, 0x02a0, 0x0247, 0x0211, 0x0000, 0x0000, 0x0000,
	0x7fff, 0x1eff, 0x2e5f, 0x223f, 0x79ff, 0x7dd6, 0x7dcc, 0x7e67,
	0x7ae7, 0x4342, 0x2769, 0x2ff3, 0x03bb, 0x0000, 0x0000, 0x0000,
	0x7fff, 0x579f, 0x635f, 0x6b3f, 0x7f1f, 0x7f1b, 0x7ef6, 0x7f75,
	0x7f94, 0x73f4, 0x57d7, 0x5bf9, 0x4ffe, 0x0000, 0x0000, 0x0000
};

/*===================================================================*/
/*                                                                   */
/*                main() : Application main                          */
/*                                                                   */
/*===================================================================*/

/* Application main */
int main( int argc, char **argv )
{
	char cmd;

	/*-------------------------------------------------------------------*/
	/*  Pad Control                                                      */
	/*-------------------------------------------------------------------*/

	/* Initialize a pad state */
	dwKeyPad1	= 0;
	dwKeyPad2	= 0;
	dwKeySystem = 0;

	/*-------------------------------------------------------------------*/
	/*  Load Cassette & Create Thread                                    */
	/*-------------------------------------------------------------------*/

	/* Initialize thread state */
	bThread = FALSE;

	/* If a rom name specified, start it */
	if ( argc == 2 )
	{
		start_application( argv[1] );
	}
	
	lcd_fb_init();
	init_joypad();
	
	//��ѭ���д��������¼�
	while(1)
	{		
		dwKeyPad1 = read(joypad_fd, 0, 0);
	}
	return(0);
}


/*===================================================================*/
/*                                                                   */
/*           emulation_thread() : Thread Hooking Routine             */
/*                                                                   */
/*===================================================================*/

void *emulation_thread( void *args )
{
	InfoNES_Main();
}


/*===================================================================*/
/*                                                                   */
/*     start_application() : Start NES Hardware                      */
/*                                                                   */
/*===================================================================*/
void start_application( char *filename )
{
	/* Set a ROM image name */
	strcpy( szRomName, filename );

	/* Load cassette */
	if ( InfoNES_Load( szRomName ) == 0 )
	{
		/* Load SRAM */
		LoadSRAM();

		/* Create Emulation Thread */
		bThread = TRUE;
		pthread_create( &emulation_tid, NULL, emulation_thread, NULL );
	}
}


/*===================================================================*/
/*                                                                   */
/*           LoadSRAM() : Load a SRAM                                */
/*                                                                   */
/*===================================================================*/
int LoadSRAM()
{
/*
 *  Load a SRAM
 *
 *  Return values
 *     0 : Normally
 *    -1 : SRAM data couldn't be read
 */

	FILE		*fp;
	unsigned char	pSrcBuf[SRAM_SIZE];
	unsigned char	chData;
	unsigned char	chTag;
	int		nRunLen;
	int		nDecoded;
	int		nDecLen;
	int		nIdx;

	/* It doesn't need to save it */
	nSRAM_SaveFlag = 0;

	/* It is finished if the ROM doesn't have SRAM */
	if ( !ROM_SRAM )
		return(0);

	/* There is necessity to save it */
	nSRAM_SaveFlag = 1;

	/* The preparation of the SRAM file name */
	strcpy( szSaveName, szRomName );
	strcpy( strrchr( szSaveName, '.' ) + 1, "srm" );

	/*-------------------------------------------------------------------*/
	/*  Read a SRAM data                                                 */
	/*-------------------------------------------------------------------*/

	/* Open SRAM file */
	fp = fopen( szSaveName, "rb" );
	if ( fp == NULL )
		return(-1);

	/* Read SRAM data */
	fread( pSrcBuf, SRAM_SIZE, 1, fp );

	/* Close SRAM file */
	fclose( fp );

	/*-------------------------------------------------------------------*/
	/*  Extract a SRAM data                                              */
	/*-------------------------------------------------------------------*/

	nDecoded	= 0;
	nDecLen		= 0;

	chTag = pSrcBuf[nDecoded++];

	while ( nDecLen < 8192 )
	{
		chData = pSrcBuf[nDecoded++];

		if ( chData == chTag )
		{
			chData	= pSrcBuf[nDecoded++];
			nRunLen = pSrcBuf[nDecoded++];
			for ( nIdx = 0; nIdx < nRunLen + 1; ++nIdx )
			{
				SRAM[nDecLen++] = chData;
			}
		}else  {
			SRAM[nDecLen++] = chData;
		}
	}

	/* Successful */
	return(0);
}


/*===================================================================*/
/*                                                                   */
/*           SaveSRAM() : Save a SRAM                                */
/*                                                                   */
/*===================================================================*/
int SaveSRAM()
{
/*
 *  Save a SRAM
 *
 *  Return values
 *     0 : Normally
 *    -1 : SRAM data couldn't be written
 */

	FILE		*fp;
	int		nUsedTable[256];
	unsigned char	chData;
	unsigned char	chPrevData;
	unsigned char	chTag;
	int		nIdx;
	int		nEncoded;
	int		nEncLen;
	int		nRunLen;
	unsigned char	pDstBuf[SRAM_SIZE];

	if ( !nSRAM_SaveFlag )
		return(0);  /* It doesn't need to save it */

	/*-------------------------------------------------------------------*/
	/*  Compress a SRAM data                                             */
	/*-------------------------------------------------------------------*/

	memset( nUsedTable, 0, sizeof nUsedTable );

	for ( nIdx = 0; nIdx < SRAM_SIZE; ++nIdx )
	{
		++nUsedTable[SRAM[nIdx++]];
	}
	for ( nIdx = 1, chTag = 0; nIdx < 256; ++nIdx )
	{
		if ( nUsedTable[nIdx] < nUsedTable[chTag] )
			chTag = nIdx;
	}

	nEncoded	= 0;
	nEncLen		= 0;
	nRunLen		= 1;

	pDstBuf[nEncLen++] = chTag;

	chPrevData = SRAM[nEncoded++];

	while ( nEncoded < SRAM_SIZE && nEncLen < SRAM_SIZE - 133 )
	{
		chData = SRAM[nEncoded++];

		if ( chPrevData == chData && nRunLen < 256 )
			++nRunLen;
		else{
			if ( nRunLen >= 4 || chPrevData == chTag )
			{
				pDstBuf[nEncLen++]	= chTag;
				pDstBuf[nEncLen++]	= chPrevData;
				pDstBuf[nEncLen++]	= nRunLen - 1;
			}else  {
				for ( nIdx = 0; nIdx < nRunLen; ++nIdx )
					pDstBuf[nEncLen++] = chPrevData;
			}

			chPrevData	= chData;
			nRunLen		= 1;
		}
	}
	if ( nRunLen >= 4 || chPrevData == chTag )
	{
		pDstBuf[nEncLen++]	= chTag;
		pDstBuf[nEncLen++]	= chPrevData;
		pDstBuf[nEncLen++]	= nRunLen - 1;
	}else  {
		for ( nIdx = 0; nIdx < nRunLen; ++nIdx )
			pDstBuf[nEncLen++] = chPrevData;
	}

	/*-------------------------------------------------------------------*/
	/*  Write a SRAM data                                                */
	/*-------------------------------------------------------------------*/

	/* Open SRAM file */
	fp = fopen( szSaveName, "wb" );
	if ( fp == NULL )
		return(-1);

	/* Write SRAM data */
	fwrite( pDstBuf, nEncLen, 1, fp );

	/* Close SRAM file */
	fclose( fp );

	/* Successful */
	return(0);
}


/*===================================================================*/
/*                                                                   */
/*                  InfoNES_Menu() : Menu screen                     */
/*                                                                   */
/*===================================================================*/
int InfoNES_Menu()
{
/*
 *  Menu screen
 *
 *  Return values
 *     0 : Normally
 *    -1 : Exit InfoNES
 */

	/* If terminated */
	if ( bThread == FALSE )
	{
		return(-1);
	}

	/* Nothing to do here */
	return(0);
}


/*===================================================================*/
/*                                                                   */
/*               InfoNES_ReadRom() : Read ROM image file             */
/*                                                                   */
/*===================================================================*/
int InfoNES_ReadRom( const char *pszFileName )
{
/*
 *  Read ROM image file
 *
 *  Parameters
 *    const char *pszFileName          (Read)
 *
 *  Return values
 *     0 : Normally
 *    -1 : Error
 */

	FILE *fp;

	/* Open ROM file */
	fp = fopen( pszFileName, "rb" );
	if ( fp == NULL )
		return(-1);

	/* Read ROM Header */
	fread( &NesHeader, sizeof NesHeader, 1, fp );
	if ( memcmp( NesHeader.byID, "NES\x1a", 4 ) != 0 )
	{
		/* not .nes file */
		fclose( fp );
		return(-1);
	}

	/* Clear SRAM */
	memset( SRAM, 0, SRAM_SIZE );

	/* If trainer presents Read Triner at 0x7000-0x71ff */
	if ( NesHeader.byInfo1 & 4 )
	{
		fread( &SRAM[0x1000], 512, 1, fp );
	}

	/* Allocate Memory for ROM Image */
	ROM = (BYTE *) malloc( NesHeader.byRomSize * 0x4000 );

	/* Read ROM Image */
	fread( ROM, 0x4000, NesHeader.byRomSize, fp );

	if ( NesHeader.byVRomSize > 0 )
	{
		/* Allocate Memory for VROM Image */
		VROM = (BYTE *) malloc( NesHeader.byVRomSize * 0x2000 );

		/* Read VROM Image */
		fread( VROM, 0x2000, NesHeader.byVRomSize, fp );
	}

	/* File close */
	fclose( fp );

	/* Successful */
	return(0);
}


/*===================================================================*/
/*                                                                   */
/*           InfoNES_ReleaseRom() : Release a memory for ROM         */
/*                                                                   */
/*===================================================================*/
void InfoNES_ReleaseRom()
{
/*
 *  Release a memory for ROM
 *
 */

	if ( ROM )
	{
		free( ROM );
		ROM = NULL;
	}

	if ( VROM )
	{
		free( VROM );
		VROM = NULL;
	}
}


/*===================================================================*/
/*                                                                   */
/*             InfoNES_MemoryCopy() : memcpy                         */
/*                                                                   */
/*===================================================================*/
void *InfoNES_MemoryCopy( void *dest, const void *src, int count )
{
/*
 *  memcpy
 *
 *  Parameters
 *    void *dest                       (Write)
 *      Points to the starting address of the copied block's destination
 *
 *    const void *src                  (Read)
 *      Points to the starting address of the block of memory to copy
 *
 *    int count                        (Read)
 *      Specifies the size, in bytes, of the block of memory to copy
 *
 *  Return values
 *    Pointer of destination
 */

	memcpy( dest, src, count );
	return(dest);
}


/*===================================================================*/
/*                                                                   */
/*             InfoNES_MemorySet() : memset                          */
/*                                                                   */
/*===================================================================*/
void *InfoNES_MemorySet( void *dest, int c, int count )
{
/*
 *  memset
 *
 *  Parameters
 *    void *dest                       (Write)
 *      Points to the starting address of the block of memory to fill
 *
 *    int c                            (Read)
 *      Specifies the byte value with which to fill the memory block
 *
 *    int count                        (Read)
 *      Specifies the size, in bytes, of the block of memory to fill
 *
 *  Return values
 *    Pointer of destination
 */

	memset( dest, c, count );
	return(dest);
}


/*===================================================================*/
/*                                                                   */
/*      InfoNES_LoadFrame() :                                        */
/*           Transfer the contents of work frame on the screen       */
/*                                                                   */
/*===================================================================*/
void InfoNES_LoadFrame()
{
	int x,y;
	WORD wColor;
	for (y = 0; y < NES_DISP_HEIGHT; y++ )
	{
		for (x = 0; x < NES_DISP_WIDTH; x++ )
		{
			wColor = WorkFrame[y * NES_DISP_WIDTH  + x ];
			lcd_fb_display_px(wColor, x, y);
		}
	}
}


/*===================================================================*/
/*                                                                   */
/*             InfoNES_PadState() : Get a joypad state               */
/*                                                                   */
/*===================================================================*/
void InfoNES_PadState( DWORD *pdwPad1, DWORD *pdwPad2, DWORD *pdwSystem )
{
/*
 *  Get a joypad state
 *
 *  Parameters
 *    DWORD *pdwPad1                   (Write)
 *      Joypad 1 State
 *
 *    DWORD *pdwPad2                   (Write)
 *      Joypad 2 State
 *
 *    DWORD *pdwSystem                 (Write)
 *      Input for InfoNES
 *
 */

	/* Transfer joypad state */
	*pdwPad1	= dwKeyPad1;
	*pdwPad2	= dwKeyPad2;
	*pdwSystem	= dwKeySystem;
	
	dwKeyPad1 = 0;
}


/*===================================================================*/
/*                                                                   */
/*        InfoNES_SoundInit() : Sound Emulation Initialize           */
/*                                                                   */
/*===================================================================*/
void InfoNES_SoundInit( void )
{
	sound_fd = 0;
}


/*===================================================================*/
/*                                                                   */
/*        InfoNES_SoundOpen() : Sound Open                           */
/*                                                                   */
/*===================================================================*/
int InfoNES_SoundOpen( int samples_per_sync, int sample_rate )
{
	return 1;
}


/*===================================================================*/
/*                                                                   */
/*        InfoNES_SoundClose() : Sound Close                         */
/*                                                                   */
/*===================================================================*/
void InfoNES_SoundClose( void )
{
	if ( sound_fd )
	{
		close( sound_fd );
	}
}


/*===================================================================*/
/*                                                                   */
/*            InfoNES_SoundOutput() : Sound Output 5 Waves           */
/*                                                                   */
/*===================================================================*/
void InfoNES_SoundOutput( int samples, BYTE *wave1, BYTE *wave2, BYTE *wave3, BYTE *wave4, BYTE *wave5 )
{
	
}


/*===================================================================*/
/*                                                                   */
/*            InfoNES_Wait() : Wait Emulation if required            */
/*                                                                   */
/*===================================================================*/
void InfoNES_Wait()
{
}


/*===================================================================*/
/*                                                                   */
/*            InfoNES_MessageBox() : Print System Message            */
/*                                                                   */
/*===================================================================*/
void InfoNES_MessageBox( char *pszMsg, ... )
{
	printf( "MessageBox: %s \n", pszMsg );
}


/*
 * End of InfoNES_System_Linux.cpp
 */