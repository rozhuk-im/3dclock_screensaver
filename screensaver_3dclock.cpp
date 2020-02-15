/*
 *  Copyright (c) 2013 - 2020 Naezzhy Petr(Наезжий Пётр) <petn@mail.ru>
 *  All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* 
 * File:   screensaver_3dclock.cpp
 * Author: Naezzhy Petr(Наезжий Пётр) <petn@mail.ru>
 *
 * Created on 23 января 2020 г., 13:16
 */



#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>
#include <linux/random.h>
#include <errno.h>
#include <linux/limits.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <math.h> 

#include <wchar.h> 
#include <pthread.h>

#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/glu.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#include "utils/time_func.h"
#include "graphic/glxwindow.h"


struct	rgb	
{
	uint8_t		r;
	uint8_t		g;
	uint8_t		b;
};

struct	sSymbol
{
	uint16_t	uWidth;
	uint16_t	uHeight;
	uint16_t	uTop;
	int16_t		iLeft;
	GLuint		texture;
};



uint32_t	const	ZERO_OFFSET = '0';
uint32_t	const	NUM_OF_DIGIT = 10;
uint32_t	const	FONT_HEIGHT = 256;

uint32_t	const	BITMAP_WIDTH = 512;
uint32_t	const	BITMAP_HEIGHT = 512;

uint32_t	const	FLAME_WIDTH = 1024;
uint32_t	const	FLAME_HEIGHT = 512;
uint32_t	const	FLAME_RATE_MILLIS = 10;

uint32_t	const	IMPOSSIBLE_VAL = 9999;
float		const	CUBE_ROTATION_SPEED = 0.006;

char		const	FONT_NAME[]="./fonts/Roboto-Bold.ttf";


sSymbol				_symbol[NUM_OF_DIGIT];
rgb					_scrBuff [FLAME_WIDTH][FLAME_HEIGHT];
uint8_t				_appExit = 0;




/* Filling buffer with pseudo random byte values from /dev/random */
inline int32_t 
get_random(void* pBuff, const size_t buffSize)
{
	int32_t	hFile = -1;
	int32_t	iBytesRead;
	if ((hFile = open("/dev/urandom", O_RDONLY )) < 0)
	{
		return hFile;
	}
	
	iBytesRead = read(hFile, pBuff, buffSize);

	close(hFile);
	return iBytesRead;
	
}

int32_t
randval(uint32_t uMaxVal)
{
	uint32_t uRandVal;
	if(sizeof(uRandVal) != get_random(&uRandVal, sizeof(uRandVal) ))
		return -1;

	return (uint64_t)uRandVal*(uMaxVal+1)/0xffffffff;
}

void* 
creating_flame_thread(void*)
{
	uint32_t		uIndex;
	uint32_t		i,j;
	uint8_t			tmp, color;
	rgb				palit[256] = {0};
	uint8_t			palBuff [FLAME_WIDTH][FLAME_HEIGHT];
	uint64_t		uPrevMillis = get_millisec();;
	uint64_t		uCurrMillis;

	/* Filling flame palit */
	for(uIndex=0; uIndex<64; uIndex++)
	{
		palit[uIndex].r=uIndex*4;
		palit[uIndex+64].r=255;
		palit[uIndex+64].g=uIndex*4;
		palit[uIndex+128].r=255;
		palit[uIndex+128].g=255;
		palit[uIndex+128].b=uIndex*4;
		palit[uIndex+192].r=255;
		palit[uIndex+192].g=255;
		palit[uIndex+192].b=255;
	}
	
	/* Flame updating */
	while(_appExit == 0)
	{
		uCurrMillis = get_millisec();
		if(FLAME_RATE_MILLIS >= (get_millisec() - uPrevMillis))
			continue;
			
		uPrevMillis = uCurrMillis;
		
		/* Flame seeds */
		for(i=0; i<FLAME_WIDTH; i+=8)
		{
			color = randval(255);
			for(j=0; j<8; j++)
			{
				palBuff[i+j][0]=color;
			}
		}
		
		/* One pass flame gen method */
//		for(i=1; i<FLAME_WIDTH-1; i++)
//		{
//			for(j=1; j<FLAME_HEIGHT-1; j++)
//			{
//				tmp =( 
//						palBuff[i-1][j-1] 
//						+ palBuff[i][j-1]
//						+ palBuff[i+1][j-1] 
//						+ palBuff[i][j-1] 
//					)/3.96;
//				
//				if(tmp > 1) 
//					tmp--; 
//				else 
//					tmp=0;
//				
//				palBuff[i][j]=tmp;
//			}
//		}

		/* Two pass flame gen method */
		for(i=1; i<FLAME_WIDTH-1; i++)
		{
			for(j=1; j<FLAME_HEIGHT-1; j++)
			{
				tmp =( 
						palBuff[i-1][j-1] 
						+ palBuff[i][j-1]
						+ palBuff[i+1][j-1] 
					)/2.97;
				
				if(tmp > 1) 
					tmp--; 
				else 
					tmp=0;
				
				palBuff[i][j]=tmp;
			}
		}
		
		for(i=FLAME_WIDTH-1; i > 1; i--)
		{
			for(j=1; j<FLAME_HEIGHT-1; j++)
			{
				tmp =( 
						palBuff[i-1][j-1] 
						+ palBuff[i][j-1]
						+ palBuff[i+1][j-1] 
					)/2.97;
				
				if(tmp > 1) 
					tmp--; 
				else 
					tmp=0;
				
				palBuff[i][j]=tmp;
			}
		}

		for(i=0; i<FLAME_WIDTH; i++)
		{
			for(j=0; j<FLAME_HEIGHT; j++)
			{
				_scrBuff[i][j]=palit[palBuff[i][j]];
			}
		}
		
		sleep_millisec(1);
	}

	ERR:

	pthread_exit(NULL);
}




/* Generates digits textures from tt fonts */
int32_t	
create_digits_tex_array( void )
{
	FT_Library		lib;
	FT_Face			font;
	FT_GlyphSlot	gliph;
	uint8_t			*bitmap = NULL;
	
	memset(_symbol, 0, sizeof(_symbol) );

	if(0 != FT_Init_FreeType(&lib))
	{
		fprintf(stderr, "FT_Init_FreeType: Error init freetype library\n\r");
		return 0;
	}
	
	if(0 != FT_New_Face(lib, FONT_NAME, 0, &font))
	{
		fprintf(stderr, "FT_New_Face: Font loading error\n");
		return 0;
	}
	
	if(0 != FT_Set_Char_Size(font, FONT_HEIGHT << 6, FONT_HEIGHT << 6, 96, 96))
	{
		fprintf(stderr, "FT_Set_Pixel_Sizes: Error set size of pixel\n");
		return 0;		
	}
	
	
	gliph = font->glyph;
	
	for(uint32_t i = 0; i<NUM_OF_DIGIT; i++)
	{
		if(0 != FT_Load_Char(font, i+ZERO_OFFSET, FT_LOAD_RENDER))
		{
			fprintf(stderr, "FT_Load_Char: Error load char\n");
		}
			
		_symbol[i].uWidth = gliph->bitmap.width;
		_symbol[i].uHeight = gliph->bitmap.rows;
		_symbol[i].uTop = gliph->bitmap_top;
		_symbol[i].iLeft = gliph->bitmap_left;

		/* Two bytes for each pixel */
		uint32_t	uiBitmapSize = 2*_symbol[i].uWidth*_symbol[i].uHeight;	
		bitmap = NULL;
		bitmap = (uint8_t*)malloc(uiBitmapSize);
		if(bitmap == NULL)
		{
			fprintf(stderr, "malloc: Error memory allocating fot bimap\n");
			return 0;
		}
		
		memset(bitmap, 255, uiBitmapSize);
		
		/* Filling bitmap grey&alpha bytes */
		for(uint32_t y=0; y<_symbol[i].uHeight; y++)
		{
			for(uint32_t x=0; x<_symbol[i].uWidth; x++)
			{
				bitmap[2*(x+y*gliph->bitmap.width)+1]
					= 0.97*gliph->bitmap.buffer[x+y*gliph->bitmap.width];
			}

		}
		
		/* Creating symbol texture */
		glGenTextures(1, &_symbol[i].texture);
		if(0 == _symbol[i].texture)
		{
			fprintf(stderr, "glGenTextures: Error generate texture indentifier (GL not init?)\n");
			return 0;
		}
		
			
		glEnable(GL_TEXTURE_RECTANGLE);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		glBindTexture(GL_TEXTURE_RECTANGLE, _symbol[i].texture);
		glTexParameterf(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameterf(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		glTexImage2D( GL_TEXTURE_RECTANGLE, 0, GL_RGBA, 
						_symbol[i].uWidth, _symbol[i].uHeight, 
						0, GL_LUMINANCE_ALPHA,
						GL_UNSIGNED_BYTE, bitmap);
		
		
		free(bitmap);
	}
	
	FT_Done_Face(font);
	FT_Done_FreeType(lib);
	
	return 1;
}

void
destroy_digits_tex_array(void)
{
	for(uint32_t i=0; i<NUM_OF_DIGIT; i++)
	{
		glDeleteTextures(1, &_symbol[i].texture);
	}
	
	memset(_symbol, 0, sizeof(_symbol));
}


/* Draw digit textured quads */
void
draw_gliph_quads(const char *szTxt)
{
	uint32_t uTxtLen = strlen(szTxt);
	if(uTxtLen != 2)
		return;
	
	int32_t	x = ( BITMAP_WIDTH - 2*_symbol[0].uWidth-_symbol[0].iLeft	) *0.45;
	int32_t	y = (BITMAP_HEIGHT-FONT_HEIGHT)/2;
	
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glDisable(GL_LIGHTING);
	glEnable(GL_TEXTURE_RECTANGLE);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
	glEnable(GL_COLOR_MATERIAL);
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	
	glNormal3f( 0.0f,  0.0f, 1.0f);
	
	for(uint32_t i=0; i<uTxtLen; i++)
	{
		glBindTexture(GL_TEXTURE_RECTANGLE, _symbol[szTxt[i]-ZERO_OFFSET].texture );
		
		x += _symbol[szTxt[i]-ZERO_OFFSET].iLeft + 1;
		y = y - (_symbol[szTxt[i]-ZERO_OFFSET].uHeight - _symbol[szTxt[i]-ZERO_OFFSET].uTop);
		
		glBegin(GL_QUADS);
			glTexCoord2f( _symbol[szTxt[i]-ZERO_OFFSET].uWidth-1,	0);
			glVertex3f( x+_symbol[szTxt[i]-ZERO_OFFSET].uWidth, y+_symbol[szTxt[i]-ZERO_OFFSET].uHeight, 0.5f);
			
			glTexCoord2f(0,	0);
			glVertex3f(	x, y+_symbol[szTxt[i]-ZERO_OFFSET].uHeight, 0.5f);
		
			glTexCoord2f(0,	_symbol[szTxt[i]-ZERO_OFFSET].uHeight-1);
			glVertex3f(	x, y, 0.5f);
			
			glTexCoord2f(_symbol[szTxt[i]-ZERO_OFFSET].uWidth-1,	_symbol[szTxt[i]-ZERO_OFFSET].uHeight-1);
			glVertex3f(	x+_symbol[szTxt[i]-ZERO_OFFSET].uWidth, y, 0.5f);
		glEnd();
		
		x += _symbol[szTxt[i]-ZERO_OFFSET].uWidth;
		
	}
};


void
draw_time_edge_texture(const char *szTime, GLuint uTex)
{
	uint32_t	const	uBorderWidth = 20;
	uint32_t	const	uLineWidth = 3;
	uint32_t	const	uCathet = 90;
	float		const	fLineConst = 0.8;
	
	glViewport(0, 0, BITMAP_WIDTH, BITMAP_HEIGHT);
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glOrtho(0, BITMAP_WIDTH, 0, BITMAP_HEIGHT, 0, 20);
	gluLookAt(0, 0, 1, 0, 0, 0, 0, 1, 0);
	
	glDisable(GL_TEXTURE_RECTANGLE);
	
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
	
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);
	
	/*************** Drawing base cube edge texture background ****************/
	/* Background quad */
	glColor4f(0.0, 0.1, 0.1, 0.9f);
	glBegin(GL_QUADS);
		glNormal3f( 0.0f, 0.0f, 1.0f);
		glVertex3f( 0, 0, 1); 
		glVertex3f( BITMAP_WIDTH-1, 0, 1);
		glVertex3f( BITMAP_WIDTH-1, BITMAP_HEIGHT-1, 1);
		glVertex3f( 0, BITMAP_HEIGHT-1, 1);
	glEnd();
	
	/* Borders */
	glColor4f(0.1, 0.1, 1.0, 0.9f);
	/* Bottom */
	glBegin(GL_QUADS);
		glNormal3f( 0.0f, 0.0f, 1.0f);
		glVertex3f( 0, 0, 1); 
		glVertex3f( BITMAP_WIDTH-1, 0, 1);
		glVertex3f( BITMAP_WIDTH-1, uBorderWidth, 1);
		glVertex3f( 0, uBorderWidth, 1);
	glEnd();
	/* left */
	glBegin(GL_QUADS);
		glNormal3f( 0.0f, 0.0f, 1.0f);
		glVertex3f( 0, 0, 1); 
		glVertex3f( uBorderWidth, 0, 1);
		glVertex3f( uBorderWidth, BITMAP_HEIGHT-1, 1);
		glVertex3f( 0, BITMAP_HEIGHT-1, 1);
	glEnd();		
	/* Right */
	glBegin(GL_QUADS);
		glNormal3f( 0.0f, 0.0f, 1.0f);
		glVertex3f( BITMAP_WIDTH-1-uBorderWidth, 0, 1); 
		glVertex3f( BITMAP_WIDTH-1, 0, 1);
		glVertex3f( BITMAP_WIDTH-1, BITMAP_HEIGHT-1, 1);
		glVertex3f( BITMAP_WIDTH-1-uBorderWidth, BITMAP_HEIGHT-1, 1);
	glEnd();	
	/* Top */
	glBegin(GL_QUADS);
		glNormal3f( 0.0f, 0.0f, 1.0f);
		glVertex3f( 0, BITMAP_HEIGHT-1, 1); 
		glVertex3f( BITMAP_WIDTH-1, BITMAP_HEIGHT-1, 1);
		glVertex3f( BITMAP_WIDTH-1, BITMAP_HEIGHT-1-uBorderWidth, 1);
		glVertex3f( 0, BITMAP_HEIGHT-1-uBorderWidth, 1);
	glEnd();
	
	/* Triangles */
	/* Bottom-left */
	glBegin(GL_TRIANGLES);
		glNormal3f( 0.0f, 0.0f, 1.0f);
		glVertex3f( 0, 0, 1);
		glVertex3f( uCathet, 0, 1);
		glVertex3f( 0, uCathet, 1);
	glEnd();
	/* Bottom-right */
	glBegin(GL_TRIANGLES);
		glNormal3f( 0.0f, 0.0f, 1.0f);
		glVertex3f( BITMAP_WIDTH-1, 0, 1);
		glVertex3f( BITMAP_WIDTH-1-uCathet, 0, 1);
		glVertex3f( BITMAP_WIDTH-1, uCathet, 1);
	glEnd();
	/* Top-left */
	glBegin(GL_TRIANGLES);
		glNormal3f( 0.0f, 0.0f, 1.0f);
		glVertex3f( 0, BITMAP_HEIGHT-1, 1);
		glVertex3f( uCathet, BITMAP_HEIGHT-1, 1);
		glVertex3f( 0, BITMAP_HEIGHT-1-uCathet, 1);
	glEnd();
	/* Top-right */
	glBegin(GL_TRIANGLES);
		glNormal3f( 0.0f, 0.0f, 1.0f);
		glVertex3f( BITMAP_WIDTH-1, BITMAP_HEIGHT-1, 1);
		glVertex3f( BITMAP_WIDTH-1-uCathet, BITMAP_HEIGHT-1, 1);
		glVertex3f( BITMAP_WIDTH-1, BITMAP_HEIGHT-1-uCathet, 1);
	glEnd();
	
	/* Lines */
	glLineWidth(uLineWidth);
	glColor4f(1.0, 0.90, 0.1, 0.7f);
	glBegin(GL_LINE_LOOP);
		glNormal3f( 0.0f, 0.0f, 1.0f);
		glVertex3f( uCathet*fLineConst, uBorderWidth, 1);
		glVertex3f( BITMAP_WIDTH-1-uCathet*fLineConst, uBorderWidth, 1);
		glVertex3f( BITMAP_WIDTH-1-uBorderWidth, uCathet*fLineConst, 1);
		glVertex3f( BITMAP_WIDTH-1-uBorderWidth, BITMAP_HEIGHT-1-uCathet*fLineConst, 1);
		glVertex3f( BITMAP_WIDTH-1-uCathet*fLineConst, BITMAP_HEIGHT-1-uBorderWidth, 1);
		glVertex3f( uCathet*fLineConst, BITMAP_HEIGHT-1-uBorderWidth, 1);
		glVertex3f( uBorderWidth, BITMAP_HEIGHT-1-uCathet*fLineConst, 1);
		glVertex3f( uBorderWidth, uCathet*fLineConst, 1);
	glEnd();
	
	glColor4f(1.0f, 1.0f, 1.0f, 0.9f);
	
	draw_gliph_quads(szTime);
	
	glEnable(GL_TEXTURE_RECTANGLE);
	glBindTexture(GL_TEXTURE_RECTANGLE, uTex);
	glCopyTexImage2D(GL_TEXTURE_RECTANGLE, 0, GL_RGBA, 0, 0, BITMAP_WIDTH, BITMAP_HEIGHT, 0);
	
}


/* Redraw window callback */
void
redraw_window(cGLXWindow::sWindowState *ws, cGLXWindow::sXMouseCursPos *mousePos)
{

	time_t		static	rawtime;
	tm			static	tminfo;
	uint32_t	static	uHour = IMPOSSIBLE_VAL;
	uint32_t	static	uMin = IMPOSSIBLE_VAL;
	uint32_t	static	uSec = IMPOSSIBLE_VAL;
	char				cTimeBuff[4] = {0};
	float		static	fX;
	int32_t		static	iStartMousePosX = IMPOSSIBLE_VAL;
	int32_t		static	iStartMousePosY = IMPOSSIBLE_VAL;
	
	GLfloat		static	light0Diffuse[] = { 1.0, 1.0, 1.0 };
	GLfloat		static	light0Ambient[] = { 0.3, 0.3, 0.3 };
	GLfloat		static	light0Direction[] = { 0.0, 0.0, 1.0, 0.0 };
		
	float		static	fRangeZ = -5.5F;
	float		static	fAngleX[3];
	float		static	fAngleY[3];
	float		static	dY[] = {0.2F,0.2f,0.2f};
	uint32_t	static	uIndex;
	GLuint		static	uSecTex, uMinTex, uHourTex, uFlameTex;
	
	uint64_t		static		uPrevMillis;
	uint64_t		static		uCurrMillis;
	float			static		fDelta = 0;
	GLUquadricObj	static		*quadrObj = NULL;

	/************************ GL initializing *********************************/
	if (ws->initFlag)
	{
		uPrevMillis = get_millisec();

		glEnable(GL_POLYGON_SMOOTH);
		glShadeModel(GL_SMOOTH);
		glEnable(GL_NORMALIZE);
		glDepthFunc(GL_LEQUAL);
		glEnable(GL_MULTISAMPLE);

		glEnable(GL_COLOR_MATERIAL);
		glEnable(GL_TEXTURE_RECTANGLE);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

		/* Init sphere objects */
		quadrObj = gluNewQuadric();
		gluQuadricDrawStyle(quadrObj,GLU_FILL);
		gluQuadricNormals(quadrObj, GLU_SMOOTH);
		
		/* Getting start random rotation angles for cubes */
		for(uIndex=0; uIndex<3; uIndex++)
		{
			fAngleY[uIndex]=(float)(randval(60));
			fAngleX[uIndex]=(float)(randval(360));
		}

		/* Generating textures */
		create_digits_tex_array();

		glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

		glTexParameterf(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameterf(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		glGenTextures(1, &uFlameTex);
		glGenTextures(1, &uSecTex);
		glGenTextures(1, &uMinTex);
		glGenTextures(1, &uHourTex);

		glFlush();
	}
	
	/************************* Resizing viewport ******************************/
	if (ws->resizeFlag)
	{
		fX = (float)ws->uWidth/(float)ws->uHeight;
	}
	
	/************************** Closing window ********************************/
	if (ws->destroyFlag)
	{
		destroy_digits_tex_array();
	}
	
	/* Checking mouse cursor position and stop program if it changes */
	if(mousePos->iRootX > 0 && mousePos->iRootY > 0
		&& iStartMousePosX == IMPOSSIBLE_VAL && iStartMousePosY == IMPOSSIBLE_VAL)
	{
		iStartMousePosX = mousePos->iRootX;
		iStartMousePosY = mousePos->iRootY;
	}
	else if((iStartMousePosX != IMPOSSIBLE_VAL || iStartMousePosY != IMPOSSIBLE_VAL)
			&& ((iStartMousePosX != mousePos->iRootX) || (iStartMousePosY != mousePos->iRootY)) )
	{
		_appExit = 1;
	}

	/************************* Render to texture ******************************/
	glDisable(GL_LIGHTING);
	glEnable(GL_TEXTURE_RECTANGLE);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
	glEnable(GL_COLOR_MATERIAL);
	time(&rawtime);
	localtime_r(&rawtime, &tminfo);
	
	/* Create framing digits on edges textures */
	if(uHour != tminfo.tm_hour)
	{
		snprintf(cTimeBuff, sizeof(cTimeBuff), "%0.2d", tminfo.tm_hour);
		draw_time_edge_texture(cTimeBuff, uHourTex);
		uHour = tminfo.tm_hour; 
	}
	if(uMin != tminfo.tm_min)
	{
		snprintf(cTimeBuff, sizeof(cTimeBuff), "%0.2d", tminfo.tm_min);
		draw_time_edge_texture(cTimeBuff, uMinTex);
		uMin = tminfo.tm_min; 
	}
	if(uSec != tminfo.tm_sec)
	{
		snprintf(cTimeBuff, sizeof(cTimeBuff), "%0.2d", tminfo.tm_sec);
		draw_time_edge_texture(cTimeBuff, uSecTex);
		uSec = tminfo.tm_sec; 
	}
	

	/*********************** Render to screen *********************************/
	/* Rotations calculation */
	uCurrMillis = get_millisec();
	fDelta = CUBE_ROTATION_SPEED * float(uCurrMillis - uPrevMillis);
	uPrevMillis = uCurrMillis;
	
	for(uIndex=0; uIndex<3; uIndex++)
	{
		fAngleX[uIndex] = (float)fAngleX[uIndex] + fDelta;
		
		if(fAngleX[uIndex] > 360)
			fAngleX[uIndex]=0;

		fAngleY[uIndex] = fAngleY[uIndex] + dY[uIndex];
		
		if((fAngleY[uIndex] > 60) || (fAngleY[uIndex] < -60)) 
			dY[uIndex] = (-1)*dY[uIndex];
	}
	
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(50,(double)ws->uWidth / ws->uHeight,0.5,500);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	gluLookAt(0, 0, 6, 0, 0, 0, 0, 1, 0);
	glDepthFunc(GL_LEQUAL);
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
	
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glViewport(0, 0, ws->uWidth, ws->uHeight);

	glLoadIdentity();

	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	
	glEnable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);

    glBlendFunc(GL_SRC_ALPHA,GL_ONE);
	glEnable(GL_TEXTURE_RECTANGLE);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	/* Drawing flame quad */
	glBindTexture(GL_TEXTURE_RECTANGLE, uFlameTex);
	glTexImage2D(GL_TEXTURE_RECTANGLE, 0, 3, FLAME_WIDTH, FLAME_HEIGHT, 
						 0, GL_RGB, GL_UNSIGNED_BYTE, _scrBuff);
	
	glDisable(GL_LIGHTING);
	
	glColor4f(1.0,1.0,1.0,0.9f);
    glPushMatrix();
		glTranslatef(0.0f, 0.0f, -10.0f);
		glBegin(GL_QUADS);
			glNormal3f( 0.0f, 0.0f, 1.0f);
			glTexCoord2f(0.0f, 0.0f);						glVertex3f( -5.0f*fX,  -5.2F,  0.0f);
			glTexCoord2f(FLAME_WIDTH/2-1, 0.0f);			glVertex3f( -5.0f*fX,   4.0F,  0.0f);
			glTexCoord2f(FLAME_WIDTH/2-1, FLAME_HEIGHT-1);	glVertex3f(  5.0f*fX,   4.0f,  0.0f);
			glTexCoord2f(0.0f, FLAME_HEIGHT-1);				glVertex3f(  5.0f*fX,  -5.2f,  0.0f);
			glEnd();
	glPopMatrix();
	
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, light0Diffuse);
	glLightfv(GL_LIGHT0, GL_AMBIENT, light0Ambient);
	glLightfv(GL_LIGHT0, GL_POSITION, light0Direction);

	glEnable(GL_COLOR_MATERIAL);
	glLightModelf(GL_LIGHT_MODEL_TWO_SIDE, GL_FALSE);
			
	/* Drawing time cubes */
	glColor4f(1.0f,1.0f,1.0f,1.0f);
	glPushMatrix();
		glTranslatef(-2.0F, 0.0F, fRangeZ);
	    glRotatef(fAngleY[0], 1.0F, 0.0F, 0.0F);
	    glRotatef(fAngleX[0], 0.0F, 1.0F, 0.0F);
		glBindTexture(GL_TEXTURE_RECTANGLE, uHourTex);
		glBegin(GL_QUADS);
			glNormal3f( 0.0F, 0.0F, 0.2F);
			glTexCoord2f(BITMAP_WIDTH,BITMAP_HEIGHT);	glVertex3f( 0.5F, 0.5F, 0.5F);
			glTexCoord2f(0,BITMAP_HEIGHT);				glVertex3f(-0.5F, 0.5F, 0.5F);
			glTexCoord2f(0,0);							glVertex3f(-0.5F,-0.5F, 0.5F);
			glTexCoord2f(BITMAP_WIDTH,0);				glVertex3f( 0.5F,-0.5F, 0.5F);

			glNormal3f( 0.0F, 0.0F,-0.2F);
			glTexCoord2f(BITMAP_WIDTH,0);				glVertex3f(-0.5F,-0.5F,-0.5F);
			glTexCoord2f(BITMAP_WIDTH,BITMAP_HEIGHT);	glVertex3f(-0.5F, 0.5F,-0.5F);
			glTexCoord2f(0,BITMAP_HEIGHT);				glVertex3f( 0.5F, 0.5F,-0.5F);
			glTexCoord2f(0,0);							glVertex3f( 0.5F,-0.5F,-0.5F);

			glNormal3f( 0.0F, 0.2F, 0.0F);
			glTexCoord2f(0,0);							glVertex3f( 0.5F, 0.5F, 0.5F);
			glTexCoord2f(BITMAP_WIDTH,0);				glVertex3f( 0.5F, 0.5F,-0.5F);
			glTexCoord2f(BITMAP_WIDTH,BITMAP_HEIGHT);	glVertex3f(-0.5F, 0.5F,-0.5F);
			glTexCoord2f(0,BITMAP_HEIGHT);				glVertex3f(-0.5F, 0.5F, 0.5F);

			glNormal3f( 0.0F,-0.2F, 0.0F);
			glTexCoord2f(BITMAP_WIDTH,BITMAP_HEIGHT);	glVertex3f(-0.5F,-0.5F,-0.5F);
			glTexCoord2f(0,BITMAP_HEIGHT);				glVertex3f( 0.5F,-0.5F,-0.5F);
			glTexCoord2f(0,0);							glVertex3f( 0.5F,-0.5F, 0.5F);
			glTexCoord2f(BITMAP_WIDTH,0);				glVertex3f(-0.5F,-0.5F, 0.5F);

			glNormal3f( 0.2F, 0.0F, 0.0F);
			glTexCoord2f(0,BITMAP_HEIGHT);				glVertex3f( 0.5F, 0.5F, 0.5F);
			glTexCoord2f(0,0);							glVertex3f( 0.5F,-0.5F, 0.5F);
			glTexCoord2f(BITMAP_WIDTH,0);				glVertex3f( 0.5F,-0.5F,-0.5F);
			glTexCoord2f(BITMAP_WIDTH,BITMAP_HEIGHT);	glVertex3f( 0.5F, 0.5F,-0.5F);

			glNormal3f(-0.2F, 0.0F, 0.0F);
			glTexCoord2f(0,0);							glVertex3f(-0.5F,-0.5F,-0.5F);
			glTexCoord2f(BITMAP_WIDTH,0);				glVertex3f(-0.5F,-0.5F, 0.5F);
			glTexCoord2f(BITMAP_WIDTH,BITMAP_HEIGHT);	glVertex3f(-0.5F, 0.5F, 0.5F);
			glTexCoord2f(0,BITMAP_HEIGHT);				glVertex3f(-0.5F, 0.5F,-0.5F);
		glEnd();
	glPopMatrix();

	glPushMatrix();
		glTranslatef(0.0F, 0.0F, fRangeZ);
	    glRotatef(fAngleY[1], 1.0F, 0.0F, 0.0F);
	    glRotatef(fAngleX[1], 0.0F, 1.0F, 0.0F);
		glBindTexture(GL_TEXTURE_RECTANGLE, uMinTex);
		glBegin(GL_QUADS);
			glNormal3f( 0.0F, 0.0F, 0.2F);
			glTexCoord2f(BITMAP_WIDTH,BITMAP_HEIGHT);	glVertex3f( 0.5F, 0.5F, 0.5F);
			glTexCoord2f(0,BITMAP_HEIGHT);				glVertex3f(-0.5F, 0.5F, 0.5F);
			glTexCoord2f(0,0);							glVertex3f(-0.5F,-0.5F, 0.5F);
			glTexCoord2f(BITMAP_WIDTH,0);				glVertex3f( 0.5F,-0.5F, 0.5F);

			glNormal3f( 0.0F, 0.0F,-0.2F);
			glTexCoord2f(BITMAP_WIDTH,0);				glVertex3f(-0.5F,-0.5F,-0.5F);
			glTexCoord2f(BITMAP_WIDTH,BITMAP_HEIGHT);	glVertex3f(-0.5F, 0.5F,-0.5F);
			glTexCoord2f(0,BITMAP_HEIGHT);				glVertex3f( 0.5F, 0.5F,-0.5F);
			glTexCoord2f(0,0);							glVertex3f( 0.5F,-0.5F,-0.5F);

			glNormal3f( 0.0F, 0.2F, 0.0F);
			glTexCoord2f(0,0);							glVertex3f( 0.5F, 0.5F, 0.5F);
			glTexCoord2f(BITMAP_WIDTH,0);				glVertex3f( 0.5F, 0.5F,-0.5F);
			glTexCoord2f(BITMAP_WIDTH,BITMAP_HEIGHT);	glVertex3f(-0.5F, 0.5F,-0.5F);
			glTexCoord2f(0,BITMAP_HEIGHT);				glVertex3f(-0.5F, 0.5F, 0.5F);

			glNormal3f( 0.0F,-0.2F, 0.0F);
			glTexCoord2f(BITMAP_WIDTH,BITMAP_HEIGHT);	glVertex3f(-0.5F,-0.5F,-0.5F);
			glTexCoord2f(0,BITMAP_HEIGHT);				glVertex3f( 0.5F,-0.5F,-0.5F);
			glTexCoord2f(0,0);							glVertex3f( 0.5F,-0.5F, 0.5F);
			glTexCoord2f(BITMAP_WIDTH,0);				glVertex3f(-0.5F,-0.5F, 0.5F);

			glNormal3f( 0.2F, 0.0F, 0.0F);
			glTexCoord2f(0,BITMAP_HEIGHT);				glVertex3f( 0.5F, 0.5F, 0.5F);
			glTexCoord2f(0,0);							glVertex3f( 0.5F,-0.5F, 0.5F);
			glTexCoord2f(BITMAP_WIDTH,0);				glVertex3f( 0.5F,-0.5F,-0.5F);
			glTexCoord2f(BITMAP_WIDTH,BITMAP_HEIGHT);	glVertex3f( 0.5F, 0.5F,-0.5F);

			glNormal3f(-0.2F, 0.0F, 0.0F);
			glTexCoord2f(0,0);							glVertex3f(-0.5F,-0.5F,-0.5F);
			glTexCoord2f(BITMAP_WIDTH,0);				glVertex3f(-0.5F,-0.5F, 0.5F);
			glTexCoord2f(BITMAP_WIDTH,BITMAP_HEIGHT);	glVertex3f(-0.5F, 0.5F, 0.5F);
			glTexCoord2f(0,BITMAP_HEIGHT);				glVertex3f(-0.5F, 0.5F,-0.5F);
		glEnd();
    glPopMatrix();

    glPushMatrix();
		glTranslatef(2.0F, 0.0F, fRangeZ);
		glRotatef(fAngleY[2], 1.0F, 0.0F, 0.0F);
		glRotatef(fAngleX[2], 0.0F, 1.0F, 0.0F);
		glBindTexture(GL_TEXTURE_RECTANGLE, uSecTex);
		glBegin(GL_QUADS);
			glNormal3f( 0.0F, 0.0F, 0.2F);
			glTexCoord2f(BITMAP_WIDTH,BITMAP_HEIGHT);	glVertex3f( 0.5F, 0.5F, 0.5F);
			glTexCoord2f(0,BITMAP_HEIGHT);				glVertex3f(-0.5F, 0.5F, 0.5F);
			glTexCoord2f(0,0);							glVertex3f(-0.5F,-0.5F, 0.5F);
			glTexCoord2f(BITMAP_WIDTH,0);				glVertex3f( 0.5F,-0.5F, 0.5F);

			glNormal3f( 0.0F, 0.0F,-0.2F);
			glTexCoord2f(BITMAP_WIDTH,0);				glVertex3f(-0.5F,-0.5F,-0.5F);
			glTexCoord2f(BITMAP_WIDTH,BITMAP_HEIGHT);	glVertex3f(-0.5F, 0.5F,-0.5F);
			glTexCoord2f(0,BITMAP_HEIGHT);				glVertex3f( 0.5F, 0.5F,-0.5F);
			glTexCoord2f(0,0);							glVertex3f( 0.5F,-0.5F,-0.5F);

			glNormal3f( 0.0F, 0.2F, 0.0F);
			glTexCoord2f(0,0);							glVertex3f( 0.5F, 0.5F, 0.5F);
			glTexCoord2f(BITMAP_WIDTH,0);				glVertex3f( 0.5F, 0.5F,-0.5F);
			glTexCoord2f(BITMAP_WIDTH,BITMAP_HEIGHT);	glVertex3f(-0.5F, 0.5F,-0.5F);
			glTexCoord2f(0,BITMAP_HEIGHT);				glVertex3f(-0.5F, 0.5F, 0.5F);

			glNormal3f( 0.0F,-0.2F, 0.0F);
			glTexCoord2f(BITMAP_WIDTH,BITMAP_HEIGHT);	glVertex3f(-0.5F,-0.5F,-0.5F);
			glTexCoord2f(0,BITMAP_HEIGHT);				glVertex3f( 0.5F,-0.5F,-0.5F);
			glTexCoord2f(0,0);							glVertex3f( 0.5F,-0.5F, 0.5F);
			glTexCoord2f(BITMAP_WIDTH,0);				glVertex3f(-0.5F,-0.5F, 0.5F);

			glNormal3f( 0.2F, 0.0F, 0.0F);
			glTexCoord2f(0,BITMAP_HEIGHT);				glVertex3f( 0.5F, 0.5F, 0.5F);
			glTexCoord2f(0,0);							glVertex3f( 0.5F,-0.5F, 0.5F);
			glTexCoord2f(BITMAP_WIDTH,0);				glVertex3f( 0.5F,-0.5F,-0.5F);
			glTexCoord2f(BITMAP_WIDTH,BITMAP_HEIGHT);	glVertex3f( 0.5F, 0.5F,-0.5F);

			glNormal3f(-0.2F, 0.0F, 0.0F);
			glTexCoord2f(0,0);							glVertex3f(-0.5F,-0.5F,-0.5F);
			glTexCoord2f(BITMAP_WIDTH,0);				glVertex3f(-0.5F,-0.5F, 0.5F);
			glTexCoord2f(BITMAP_WIDTH,BITMAP_HEIGHT);	glVertex3f(-0.5F, 0.5F, 0.5F);
			glTexCoord2f(0,BITMAP_HEIGHT);				glVertex3f(-0.5F, 0.5F,-0.5F);
		glEnd();
    glPopMatrix();
	
	/* Drawing flame reflections on cube edges */
	/* Getting path of full flame texture */
	glTexParameterf(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameterf(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_T, GL_CLAMP);
	
	glBindTexture(GL_TEXTURE_RECTANGLE, uFlameTex);
	
	glColor4f(0.3f, 0.0f, 0.0f, 0.5f);
	glPushMatrix();
		glTranslatef(-2.0F, 0.0F, fRangeZ);
	    glRotatef(fAngleY[0], 1.0F, 0.0F, 0.0F);
	    glRotatef(fAngleX[0], 0.0F, 1.0F, 0.0F);
		glBegin(GL_QUADS);
			glNormal3f( 0.0, 0.0, 1.0);
			glTexCoord2f(BITMAP_WIDTH/8, BITMAP_HEIGHT/16);	glVertex3f( 0.5F, 0.5F, 0.5F);
			glTexCoord2f(BITMAP_WIDTH/8,BITMAP_HEIGHT/8);	glVertex3f(-0.5F, 0.5F, 0.5F);
			glTexCoord2f(BITMAP_WIDTH/16,BITMAP_HEIGHT/8);	glVertex3f(-0.5F,-0.5F, 0.5F);
			glTexCoord2f(BITMAP_WIDTH/16,BITMAP_HEIGHT/16);	glVertex3f( 0.5F,-0.5F, 0.5F);

			glNormal3f( 0.0, 0.0,-1.0);
			glTexCoord2f(BITMAP_WIDTH/16,BITMAP_HEIGHT/16);	glVertex3f(-0.5F,-0.5F,-0.5F);
			glTexCoord2f(BITMAP_WIDTH/8,BITMAP_HEIGHT/16);	glVertex3f(-0.5F, 0.5F,-0.5F);
			glTexCoord2f(BITMAP_WIDTH/8,BITMAP_HEIGHT/8);	glVertex3f( 0.5F, 0.5F,-0.5F);
			glTexCoord2f(BITMAP_WIDTH/16,BITMAP_HEIGHT/8);	glVertex3f( 0.5F,-0.5F,-0.5F);

			glNormal3f( 0.0, 1.0, 0.0F);
			glTexCoord2f(BITMAP_WIDTH/16,BITMAP_HEIGHT/16);	glVertex3f( 0.5F, 0.5F, 0.5F);
			glTexCoord2f(BITMAP_WIDTH/8,BITMAP_HEIGHT/16);	glVertex3f( 0.5F, 0.5F,-0.5F);
			glTexCoord2f(BITMAP_WIDTH/8,BITMAP_HEIGHT/8);	glVertex3f(-0.5F, 0.5F,-0.5F);
			glTexCoord2f(BITMAP_WIDTH/16,BITMAP_HEIGHT/8);	glVertex3f(-0.5F, 0.5F, 0.5F);

			glNormal3f( 0.0,-1.0, 0.0);
			glTexCoord2f(BITMAP_WIDTH/8, BITMAP_HEIGHT/8);	glVertex3f(-0.5F,-0.5F,-0.5F);
			glTexCoord2f(BITMAP_WIDTH/16, BITMAP_HEIGHT/8);	glVertex3f( 0.5F,-0.5F,-0.5F);
			glTexCoord2f(BITMAP_WIDTH/16,BITMAP_HEIGHT/16);	glVertex3f( 0.5F,-0.5F, 0.5F);
			glTexCoord2f(BITMAP_WIDTH/8,BITMAP_HEIGHT/16);	glVertex3f(-0.5F,-0.5F, 0.5F);

			glNormal3f( 1.0, 0.0, 0.0);
			glTexCoord2f(BITMAP_WIDTH/8, BITMAP_HEIGHT/8);	glVertex3f( 0.5F, 0.5F, 0.5F);
			glTexCoord2f(BITMAP_WIDTH/16,BITMAP_HEIGHT/8);	glVertex3f( 0.5F,-0.5F, 0.5F);
			glTexCoord2f(BITMAP_WIDTH/16,BITMAP_HEIGHT/16);	glVertex3f( 0.5F,-0.5F,-0.5F);
			glTexCoord2f(BITMAP_WIDTH/8, BITMAP_HEIGHT/16);	glVertex3f( 0.5F, 0.5F,-0.5F);

			glNormal3f(-1.0, 0.0, 0.0);
			glTexCoord2f(BITMAP_WIDTH/16,BITMAP_HEIGHT/8);	glVertex3f(-0.5F,-0.5F,-0.5F);
			glTexCoord2f(BITMAP_WIDTH/16,BITMAP_HEIGHT/16);	glVertex3f(-0.5F,-0.5F, 0.5F);
			glTexCoord2f(BITMAP_WIDTH/8,BITMAP_HEIGHT/16);	glVertex3f(-0.5F, 0.5F, 0.5F);
			glTexCoord2f(BITMAP_WIDTH/8,BITMAP_HEIGHT/8);	glVertex3f(-0.5F, 0.5F,-0.5F);
		glEnd();
	glPopMatrix();

	glPushMatrix();
		glTranslatef(0.0F, 0.0F, fRangeZ);
	    glRotatef(fAngleY[1], 1.0F, 0.0F, 0.0F);
	    glRotatef(fAngleX[1], 0.0F, 1.0F, 0.0F);
		glBegin(GL_QUADS);
			glNormal3f( 0.0, 0.0, 1.0);
			glTexCoord2f(BITMAP_WIDTH/8, BITMAP_HEIGHT/16);	glVertex3f( 0.5F, 0.5F, 0.5F);
			glTexCoord2f(BITMAP_WIDTH/8,BITMAP_HEIGHT/8);	glVertex3f(-0.5F, 0.5F, 0.5F);
			glTexCoord2f(BITMAP_WIDTH/16,BITMAP_HEIGHT/8);	glVertex3f(-0.5F,-0.5F, 0.5F);
			glTexCoord2f(BITMAP_WIDTH/16,BITMAP_HEIGHT/16);	glVertex3f( 0.5F,-0.5F, 0.5F);

			glNormal3f( 0.0, 0.0,-1.0);
			glTexCoord2f(BITMAP_WIDTH/16,BITMAP_HEIGHT/16);	glVertex3f(-0.5F,-0.5F,-0.5F);
			glTexCoord2f(BITMAP_WIDTH/8,BITMAP_HEIGHT/16);	glVertex3f(-0.5F, 0.5F,-0.5F);
			glTexCoord2f(BITMAP_WIDTH/8,BITMAP_HEIGHT/8);	glVertex3f( 0.5F, 0.5F,-0.5F);
			glTexCoord2f(BITMAP_WIDTH/16,BITMAP_HEIGHT/8);	glVertex3f( 0.5F,-0.5F,-0.5F);

			glNormal3f( 0.0, 1.0, 0.0F);
			glTexCoord2f(BITMAP_WIDTH/16,BITMAP_HEIGHT/16);	glVertex3f( 0.5F, 0.5F, 0.5F);
			glTexCoord2f(BITMAP_WIDTH/8,BITMAP_HEIGHT/16);	glVertex3f( 0.5F, 0.5F,-0.5F);
			glTexCoord2f(BITMAP_WIDTH/8,BITMAP_HEIGHT/8);	glVertex3f(-0.5F, 0.5F,-0.5F);
			glTexCoord2f(BITMAP_WIDTH/16,BITMAP_HEIGHT/8);	glVertex3f(-0.5F, 0.5F, 0.5F);

			glNormal3f( 0.0,-1.0, 0.0);
			glTexCoord2f(BITMAP_WIDTH/8, BITMAP_HEIGHT/8);	glVertex3f(-0.5F,-0.5F,-0.5F);
			glTexCoord2f(BITMAP_WIDTH/16, BITMAP_HEIGHT/8);	glVertex3f( 0.5F,-0.5F,-0.5F);
			glTexCoord2f(BITMAP_WIDTH/16,BITMAP_HEIGHT/16);	glVertex3f( 0.5F,-0.5F, 0.5F);
			glTexCoord2f(BITMAP_WIDTH/8,BITMAP_HEIGHT/16);	glVertex3f(-0.5F,-0.5F, 0.5F);

			glNormal3f( 1.0, 0.0, 0.0);
			glTexCoord2f(BITMAP_WIDTH/8, BITMAP_HEIGHT/8);	glVertex3f( 0.5F, 0.5F, 0.5F);
			glTexCoord2f(BITMAP_WIDTH/16,BITMAP_HEIGHT/8);	glVertex3f( 0.5F,-0.5F, 0.5F);
			glTexCoord2f(BITMAP_WIDTH/16,BITMAP_HEIGHT/16);	glVertex3f( 0.5F,-0.5F,-0.5F);
			glTexCoord2f(BITMAP_WIDTH/8, BITMAP_HEIGHT/16);	glVertex3f( 0.5F, 0.5F,-0.5F);

			glNormal3f(-1.0, 0.0, 0.0);
			glTexCoord2f(BITMAP_WIDTH/16,BITMAP_HEIGHT/8);	glVertex3f(-0.5F,-0.5F,-0.5F);
			glTexCoord2f(BITMAP_WIDTH/16,BITMAP_HEIGHT/16);	glVertex3f(-0.5F,-0.5F, 0.5F);
			glTexCoord2f(BITMAP_WIDTH/8,BITMAP_HEIGHT/16);	glVertex3f(-0.5F, 0.5F, 0.5F);
			glTexCoord2f(BITMAP_WIDTH/8,BITMAP_HEIGHT/8);	glVertex3f(-0.5F, 0.5F,-0.5F);
		glEnd();
    glPopMatrix();

    glPushMatrix();
		glTranslatef(2.0F, 0.0F, fRangeZ);
		glRotatef(fAngleY[2], 1.0F, 0.0F, 0.0F);
		glRotatef(fAngleX[2], 0.0F, 1.0F, 0.0F);
		glBegin(GL_QUADS);
			glNormal3f( 0.0, 0.0, 1.0);
			glTexCoord2f(BITMAP_WIDTH/8, BITMAP_HEIGHT/16);	glVertex3f( 0.5F, 0.5F, 0.5F);
			glTexCoord2f(BITMAP_WIDTH/8,BITMAP_HEIGHT/8);	glVertex3f(-0.5F, 0.5F, 0.5F);
			glTexCoord2f(BITMAP_WIDTH/16,BITMAP_HEIGHT/8);	glVertex3f(-0.5F,-0.5F, 0.5F);
			glTexCoord2f(BITMAP_WIDTH/16,BITMAP_HEIGHT/16);	glVertex3f( 0.5F,-0.5F, 0.5F);

			glNormal3f( 0.0, 0.0,-1.0);
			glTexCoord2f(BITMAP_WIDTH/16,BITMAP_HEIGHT/16);	glVertex3f(-0.5F,-0.5F,-0.5F);
			glTexCoord2f(BITMAP_WIDTH/8,BITMAP_HEIGHT/16);	glVertex3f(-0.5F, 0.5F,-0.5F);
			glTexCoord2f(BITMAP_WIDTH/8,BITMAP_HEIGHT/8);	glVertex3f( 0.5F, 0.5F,-0.5F);
			glTexCoord2f(BITMAP_WIDTH/16,BITMAP_HEIGHT/8);	glVertex3f( 0.5F,-0.5F,-0.5F);

			glNormal3f( 0.0, 1.0, 0.0F);
			glTexCoord2f(BITMAP_WIDTH/16,BITMAP_HEIGHT/16);	glVertex3f( 0.5F, 0.5F, 0.5F);
			glTexCoord2f(BITMAP_WIDTH/8,BITMAP_HEIGHT/16);	glVertex3f( 0.5F, 0.5F,-0.5F);
			glTexCoord2f(BITMAP_WIDTH/8,BITMAP_HEIGHT/8);	glVertex3f(-0.5F, 0.5F,-0.5F);
			glTexCoord2f(BITMAP_WIDTH/16,BITMAP_HEIGHT/8);	glVertex3f(-0.5F, 0.5F, 0.5F);

			glNormal3f( 0.0,-1.0, 0.0);
			glTexCoord2f(BITMAP_WIDTH/8, BITMAP_HEIGHT/8);	glVertex3f(-0.5F,-0.5F,-0.5F);
			glTexCoord2f(BITMAP_WIDTH/16, BITMAP_HEIGHT/8);	glVertex3f( 0.5F,-0.5F,-0.5F);
			glTexCoord2f(BITMAP_WIDTH/16,BITMAP_HEIGHT/16);	glVertex3f( 0.5F,-0.5F, 0.5F);
			glTexCoord2f(BITMAP_WIDTH/8,BITMAP_HEIGHT/16);	glVertex3f(-0.5F,-0.5F, 0.5F);

			glNormal3f( 1.0, 0.0, 0.0);
			glTexCoord2f(BITMAP_WIDTH/8, BITMAP_HEIGHT/8);	glVertex3f( 0.5F, 0.5F, 0.5F);
			glTexCoord2f(BITMAP_WIDTH/16,BITMAP_HEIGHT/8);	glVertex3f( 0.5F,-0.5F, 0.5F);
			glTexCoord2f(BITMAP_WIDTH/16,BITMAP_HEIGHT/16);	glVertex3f( 0.5F,-0.5F,-0.5F);
			glTexCoord2f(BITMAP_WIDTH/8, BITMAP_HEIGHT/16);	glVertex3f( 0.5F, 0.5F,-0.5F);

			glNormal3f(-1.0, 0.0, 0.0);
			glTexCoord2f(BITMAP_WIDTH/16,BITMAP_HEIGHT/8);	glVertex3f(-0.5F,-0.5F,-0.5F);
			glTexCoord2f(BITMAP_WIDTH/16,BITMAP_HEIGHT/16);	glVertex3f(-0.5F,-0.5F, 0.5F);
			glTexCoord2f(BITMAP_WIDTH/8,BITMAP_HEIGHT/16);	glVertex3f(-0.5F, 0.5F, 0.5F);
			glTexCoord2f(BITMAP_WIDTH/8,BITMAP_HEIGHT/8);	glVertex3f(-0.5F, 0.5F,-0.5F);
		glEnd();
    glPopMatrix();
	
	/* Drawing spheres between cubes */
    glDisable(GL_TEXTURE_RECTANGLE);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);

    glColor3f(0.4f, 0.2f, 0.2f);
    glPushMatrix();
		glNormal3f( 0.0F, 0.0F, 1.0F);
		glTranslatef(1.0F, 0.2F, fRangeZ);
		gluSphere(quadrObj,0.1,16,16);
	glPopMatrix();

    glPushMatrix();
		glNormal3f( 0.0F, 0.0F, 1.0F);
		glTranslatef(1.0F, -0.2F, fRangeZ);
		gluSphere(quadrObj,0.1,16,16);
    glPopMatrix();

	glPushMatrix();
		glNormal3f( 0.0F, 0.0F, 1.0F);
		glTranslatef(-1.0F, 0.2F, fRangeZ);
		gluSphere(quadrObj,0.1,16,16);
    glPopMatrix();

    glPushMatrix();
       glNormal3f( 0.0F, 0.0F, 1.0F);
		 glTranslatef(-1.0F, -0.2F, fRangeZ);
		 gluSphere(quadrObj,0.1,16,16);
    glPopMatrix();
	
	glFlush();

}

void events_update(XEvent *event)
{
	switch (event->type)
	{
		case ButtonPress:
			_appExit = 1;
		return;
		case KeyPress:
			_appExit = 1;
		return;
	}
}

/*
 * 
 */
int main(int argc, char** argv)
{
	cGLXWindow				window;
	pthread_attr_t			ptAttr;
	pthread_t				ptFlame = 0;

	window.create_window(800, 600, "3dclock", redraw_window);
	window.init_events_callback(events_update);

	window.hide_cursor();
	window.set_window_fullscreen_popup();
	
	pthread_attr_init(&ptAttr);
	pthread_attr_setscope(&ptAttr, PTHREAD_SCOPE_PROCESS);
	
	pthread_create(&ptFlame, &ptAttr, creating_flame_thread, NULL);

	while (0 < window.update_window() && _appExit == 0)
	{
		sleep_millisec(1);
	}
	
	window.destroy_window();
	_appExit = 1;

	if(ptFlame)
		pthread_join(ptFlame, NULL);
	
	pthread_attr_destroy(&ptAttr);
	
	window.show_cursor();

	return 0;
}

