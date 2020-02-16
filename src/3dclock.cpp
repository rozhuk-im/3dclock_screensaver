/*
 *  Copyright (c) 2002 - 2020 Naezzhy Petr(Наезжий Пётр) <petn@mail.ru>
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


#include <sys/types.h>
#include <stdint.h>
#include <errno.h>

#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/glu.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#include "glxwindow.h"


struct rgb {
	uint8_t r;
	uint8_t g;
	uint8_t b;
};

struct sSymbol {
	uint32_t width;
	uint32_t height;
	uint32_t top;
	int32_t left;
	GLuint texture;
};


#define NUM_OF_DIGIT		10
#define FONT_HEIGHT		256

#define BITMAP_WIDTH		512
#define BITMAP_HEIGHT		512

#define FLAME_WIDTH		1024
#define FLAME_HEIGHT		1024

#define IMPOSSIBLE_VAL		9999
#define CUBE_ROTATION_SPEED	0.006f

#define FONT_NAME		"./fonts/Roboto-Bold.ttf"


static sSymbol _symbol[NUM_OF_DIGIT];
static rgb _scr_buf[FLAME_WIDTH][FLAME_HEIGHT];
static volatile int _appExit = 0;


static inline uint64_t	
get_millisec(void) {
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);

	return ((uint64_t)((ts.tv_sec * 1000) + (ts.tv_nsec / 1000000)));
}

static inline void		
sleep_millisec(uint32_t uiMillisec) {
	struct timespec	ts;

	ts.tv_sec = (uiMillisec / 1000);
	ts.tv_nsec = ((uiMillisec * 1000000) - (ts.tv_sec * 1000000000));
	nanosleep(&ts, NULL);
}

static inline int32_t
randval(uint32_t max_val) {
	return (arc4random() % (max_val + 1));
}

static void
flame_update(void) {
	size_t i, j;
	uint8_t tmp, color, pal_buf[FLAME_WIDTH][FLAME_HEIGHT];
	static rgb palit[256];
	static int palit_init_done = 0;

	/* Filling flame palit. */
	if (0 == palit_init_done) {
		palit_init_done ++;
		memset(&palit, 0x00, sizeof(palit));
		for (i = 0; i < 64; i ++) {
			palit[i +   0].r = (uint8_t)(i * 4);
			palit[i +  64].r = 255;
			palit[i +  64].g = (uint8_t)(i * 4);
			palit[i + 128].r = 255;
			palit[i + 128].g = 255;
			palit[i + 128].b = (uint8_t)(i * 4);
			palit[i + 192].r = 255;
			palit[i + 192].g = 255;
			palit[i + 192].b = 255;
		}
	}

	/* Flame seeds. */
	for (i = 0; i < FLAME_WIDTH; i += 8) {
		color = (uint8_t)arc4random();
		for (j = 0; j < 8; j ++) {
			pal_buf[i + j][0] = color;
		}
	}

#if 1
	/* One pass flame gen method. */
	for (i = 1; i < (FLAME_WIDTH - 1); i ++) {
		for (j = 1; j < (FLAME_HEIGHT - 1); j ++) {
			tmp =(uint8_t)((
			    pal_buf[i - 1][j - 1] +
			    pal_buf[i][j - 1] +
			    pal_buf[i + 1][j - 1] +
			    pal_buf[i][j -1]) / 3.96f);
			if (tmp > 1) {
				tmp --;
			} else {
				tmp = 0;
			}
			pal_buf[i][j]=tmp;
		}
	}
#else
	/* Two pass flame gen method. */
	for (i = 1; i < (FLAME_WIDTH - 1); i ++) {
		for (j = 1; j < (FLAME_HEIGHT - 1); j ++) {
			tmp = (pal_buf[i - 1][j - 1] +
			    pal_buf[i][j - 1] +
			    pal_buf[i + 1][j - 1]) / 2.97f;
			if (tmp > 1) {
				tmp --;
			} else {
				tmp = 0;
			}
			pal_buf[i][j] = tmp;
		}
	}
	for (i = (FLAME_WIDTH - 2); i > 1; i --) {
		for(j = 1; j < (FLAME_HEIGHT - 1); j ++) {
			tmp = (pal_buf[i - 1][j - 1] +
			    pal_buf[i][j - 1] +
			    pal_buf[i + 1][j - 1]) / 2.97f;
			if (tmp > 1) {
				tmp --;
			} else {
				tmp = 0;
			}
			pal_buf[i][j] = tmp;
		}
	}
#endif

	for (i = 0; i < FLAME_WIDTH; i ++) {
		for (j = 0; j < FLAME_HEIGHT; j ++) {
			_scr_buf[i][j] = palit[pal_buf[i][j]];
		}
	}
}


/* Generates digits textures from tt fonts */
static int32_t	
create_digits_tex_array(void) {
	FT_Library lib;
	FT_Face font;
	FT_GlyphSlot gliph;
	uint8_t *bitmap;
	size_t i, x, y;
	
	memset(_symbol, 0x00, sizeof(_symbol));

	if (0 != FT_Init_FreeType(&lib)) {
		fprintf(stderr, "FT_Init_FreeType: Error init freetype library\n\r");
		return 0;
	}
	
	if (0 != FT_New_Face(lib, FONT_NAME, 0, &font)) {
		fprintf(stderr, "FT_New_Face: Font loading error\n");
		return 0;
	}
	
	if (0 != FT_Set_Char_Size(font, FONT_HEIGHT << 6, FONT_HEIGHT << 6, 96, 96)) {
		fprintf(stderr, "FT_Set_Pixel_Sizes: Error set size of pixel\n");
		return 0;
	}

	gliph = font->glyph;

	for (i = 0; i < NUM_OF_DIGIT; i ++) {
		if (0 != FT_Load_Char(font, (i + '0'), FT_LOAD_RENDER)) {
			fprintf(stderr, "FT_Load_Char: Error load char\n");
		}

		_symbol[i].width = gliph->bitmap.width;
		_symbol[i].height = gliph->bitmap.rows;
		_symbol[i].top = gliph->bitmap_top;
		_symbol[i].left = gliph->bitmap_left;

		/* Two bytes for each pixel. */
		size_t uiBitmapSize = 2 * _symbol[i].width * _symbol[i].height;	
		bitmap = (uint8_t*)malloc(uiBitmapSize);
		if (bitmap == NULL) {
			fprintf(stderr, "malloc: Error memory allocating fot bimap\n");
			return 0;
		}
		memset(bitmap, 0xff, uiBitmapSize);

		/* Filling bitmap grey&alpha bytes. */
		for (y = 0; y < _symbol[i].height; y ++) {
			for (x = 0; x<_symbol[i].width; x++) {
				bitmap[2 * (x + y * gliph->bitmap.width) + 1] = 0.97f * gliph->bitmap.buffer[x + y * gliph->bitmap.width];
			}

		}
	
		/* Creating symbol texture. */
		glGenTextures(1, &_symbol[i].texture);
		if (0 == _symbol[i].texture) {
			fprintf(stderr, "glGenTextures: Error generate texture indentifier (GL not init?)\n");
			return 0;
		}

		glEnable(GL_TEXTURE_RECTANGLE);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		glBindTexture(GL_TEXTURE_RECTANGLE, _symbol[i].texture);
		glTexParameterf(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameterf(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		glTexImage2D(GL_TEXTURE_RECTANGLE, 0, GL_RGBA, 
		    _symbol[i].width, _symbol[i].height, 0,
		    GL_LUMINANCE_ALPHA,	GL_UNSIGNED_BYTE, bitmap);

		free(bitmap);
	}

	FT_Done_Face(font);
	FT_Done_FreeType(lib);

	return 1;
}

static void
destroy_digits_tex_array(void)
{
	for (uint32_t i = 0; i < NUM_OF_DIGIT; i ++)
	{
		glDeleteTextures(1, &_symbol[i].texture);
	}

	memset(_symbol, 0x00, sizeof(_symbol));
}


/* Draw didx textured quads */
static void
draw_gliph_quads(const int time_val) {
	size_t i, didx;
	uint8_t time_digits[2];

	if (0 > time_val || 99 < time_val)
		return;
	time_digits[0] = (uint8_t)(time_val / 10);
	time_digits[1] = (uint8_t)(time_val % 10);

	int32_t	x = (BITMAP_WIDTH - 2 * _symbol[0].width - _symbol[0].left) * 0.45;
	int32_t	y = (BITMAP_HEIGHT-FONT_HEIGHT) / 2;

	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glDisable(GL_LIGHTING);
	glEnable(GL_TEXTURE_RECTANGLE);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
	glEnable(GL_COLOR_MATERIAL);
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glNormal3f(0.0f, 0.0f, 1.0f);

	for (i = 0; i < 2; i ++) {
		didx = time_digits[i];
		glBindTexture(GL_TEXTURE_RECTANGLE, _symbol[didx].texture);

		x += (_symbol[didx].left + 1);
		y = (y - (_symbol[didx].height - _symbol[didx].top));

		glBegin(GL_QUADS);
		{
			glTexCoord2f((_symbol[didx].width - 1), 0.0f);
			glVertex3f((x + _symbol[didx].width), (y + _symbol[didx].height), 0.5f);

			glTexCoord2f(0.0f, 0.0f);
			glVertex3f(x, (y + _symbol[didx].height), 0.5f);

			glTexCoord2f(0.0f, (_symbol[didx].height - 1));
			glVertex3f(x, y, 0.5f);

			glTexCoord2f((_symbol[didx].width - 1), (_symbol[didx].height - 1));
			glVertex3f((x + _symbol[didx].width), y, 0.5f);
		}
		glEnd();

		x += _symbol[didx].width;
	}
}


static void
draw_time_edge_texture(const int time_val, const GLuint tex_id) {
	const float border_width = 20.0f;
	const float line_width = 3.0f;
	const float cathet = 90.0f;
	const float line_const = 0.8f;

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

	/******* Drawing base cube edge texture background ************/
	/* Background quad */
	glColor4f(0.0f, 0.1f, 0.1f, 0.9f);
	glBegin(GL_QUADS);
	{
		glNormal3f(0.0f, 0.0f, 1.0f);
		glVertex3f(0.0f, 0.0f, 1.0f);
		glVertex3f((BITMAP_WIDTH - 1), 0.0f, 1.0f);
		glVertex3f((BITMAP_WIDTH - 1) , (BITMAP_HEIGHT - 1), 1.0f);
		glVertex3f(0.0f, (BITMAP_HEIGHT - 1), 1.0f);
	}
	glEnd();

	/* Borders */
	glColor4f(0.1f, 0.1f, 1.0f, 0.9f);

	/* Bottom */
	glBegin(GL_QUADS);
	{
		glNormal3f(0.0f, 0.0f, 1.0f);
		glVertex3f(0.0f, 0.0f, 1.0f);
		glVertex3f((BITMAP_WIDTH - 1), 0.0f, 1.0f);
		glVertex3f((BITMAP_WIDTH - 1), border_width, 1.0f);
		glVertex3f(0.0f, border_width, 1.0f);
	}
	glEnd();

	/* left */
	glBegin(GL_QUADS);
	{
		glNormal3f(0.0f, 0.0f, 1.0f);
		glVertex3f(0.0f, 0.0f, 1.0f);
		glVertex3f(border_width, 0.0f, 1.0f);
		glVertex3f(border_width, (BITMAP_HEIGHT - 1), 1.0f);
		glVertex3f(0.0f, (BITMAP_HEIGHT - 1), 1.0f);
	}
	glEnd();

	/* Right */
	glBegin(GL_QUADS);
	{
		glNormal3f(0.0f, 0.0f, 1.0f);
		glVertex3f((BITMAP_WIDTH - 1 - border_width), 0.0f, 1.0f);
		glVertex3f((BITMAP_WIDTH - 1), 0.0f, 1.0f);
		glVertex3f((BITMAP_WIDTH - 1), (BITMAP_HEIGHT - 1), 1.0f);
		glVertex3f((BITMAP_WIDTH - 1 - border_width), (BITMAP_HEIGHT - 1), 1.0f);
	}
	glEnd();

	/* Top */
	glBegin(GL_QUADS);
	{
		glNormal3f(0.0f, 0.0f, 1.0f);
		glVertex3f(0.0f, (BITMAP_HEIGHT - 1), 1.0f);
		glVertex3f((BITMAP_WIDTH - 1), (BITMAP_HEIGHT - 1), 1.0f);
		glVertex3f((BITMAP_WIDTH - 1), (BITMAP_HEIGHT - 1- border_width), 1.0f);
		glVertex3f(0.0f, (BITMAP_HEIGHT - 1 - border_width), 1.0f);
	}
	glEnd();

	/* Triangles */
	/* Bottom-left */
	glBegin(GL_TRIANGLES);
	{
		glNormal3f(0.0f, 0.0f, 1.0f);
		glVertex3f(0.0f, 0.0f, 1.0f);
		glVertex3f(cathet, 0.0f, 1.0f);
		glVertex3f(0.0f, cathet, 1.0f);
	}
	glEnd();

	/* Bottom-right */
	glBegin(GL_TRIANGLES);
	{
		glNormal3f(0.0f, 0.0f, 1.0f);
		glVertex3f((BITMAP_WIDTH - 1), 0.0f, 1.0f);
		glVertex3f((BITMAP_WIDTH - 1 - cathet), 0.0f, 1.0f);
		glVertex3f((BITMAP_WIDTH - 1), cathet, 1.0f);
	}
	glEnd();

	/* Top-left */
	glBegin(GL_TRIANGLES);
	{
		glNormal3f(0.0f, 0.0f, 1.0f);
		glVertex3f(0.0f, (BITMAP_HEIGHT - 1), 1.0f);
		glVertex3f(cathet, (BITMAP_HEIGHT - 1), 1.0f);
		glVertex3f(0.0f, (BITMAP_HEIGHT - 1 - cathet), 1.0f);
	}
	glEnd();

	/* Top-right */
	glBegin(GL_TRIANGLES);
	{
		glNormal3f(0.0f, 0.0f, 1.0f);
		glVertex3f((BITMAP_WIDTH - 1), (BITMAP_HEIGHT - 1), 1.0f);
		glVertex3f((BITMAP_WIDTH - 1 - cathet), (BITMAP_HEIGHT - 1), 1.0f);
		glVertex3f((BITMAP_WIDTH - 1), (BITMAP_HEIGHT - 1 - cathet), 1.0f);
	}
	glEnd();

	/* Lines */
	glLineWidth(line_width);
	glColor4f(1.0, 0.90f, 0.1f, 0.7f);
	glBegin(GL_LINE_LOOP);
	{
		glNormal3f(0.0f, 0.0f, 1.0f);
		glVertex3f((cathet * line_const), border_width, 1.0f);
		glVertex3f((BITMAP_WIDTH - 1 - cathet * line_const), border_width, 1.0f);
		glVertex3f((BITMAP_WIDTH - 1 - border_width), (cathet * line_const), 1.0f);
		glVertex3f((BITMAP_WIDTH - 1 - border_width), (BITMAP_HEIGHT - 1 - cathet * line_const), 1.0f);
		glVertex3f((BITMAP_WIDTH - 1 - cathet * line_const), (BITMAP_HEIGHT - 1 - border_width), 1.0f);
		glVertex3f((cathet * line_const), (BITMAP_HEIGHT - 1 - border_width), 1.0f);
		glVertex3f(border_width, (BITMAP_HEIGHT - 1 - cathet * line_const), 1.0f);
		glVertex3f(border_width, (cathet * line_const), 1.0f);
	}
	glEnd();

	glColor4f(1.0f, 1.0f, 1.0f, 0.9f);

	draw_gliph_quads(time_val);

	glEnable(GL_TEXTURE_RECTANGLE);
	glBindTexture(GL_TEXTURE_RECTANGLE, tex_id);
	glCopyTexImage2D(GL_TEXTURE_RECTANGLE, 0, GL_RGBA, 0, 0, BITMAP_WIDTH, BITMAP_HEIGHT, 0);
}


/* Redraw window callback */
static void
redraw_window(cGLXWindow::sWindowState *ws, cGLXWindow::sXMouseCursPos *mousePos) {
	size_t i;
	time_t rawtime;
	tm tminfo;
	float fDelta;
	const float light0Diffuse[] = { 1.0f, 1.0f, 1.0f };
	const float light0Ambient[] = { 0.3f, 0.3f, 0.3f };
	const float light0Direction[] = { 0.0f, 0.0f, 1.0f, 0.0f };
	const float fRangeZ = -5.5f;
	const float fX = ((float)ws->width / (float)ws->height);
	const uint64_t cur_time_ms = get_millisec();

	static float dY[] = { 0.2f, 0.2f, 0.2f };
	static uint32_t	uHour = IMPOSSIBLE_VAL;
	static uint32_t	uMin = IMPOSSIBLE_VAL;
	static uint32_t	uSec = IMPOSSIBLE_VAL;
	static int32_t iStartMousePosX = IMPOSSIBLE_VAL;
	static int32_t iStartMousePosY = IMPOSSIBLE_VAL;
	static GLuint tex_second, tex_minute, tex_hour, tex_flame;
	static uint64_t prev_time_ms = get_millisec();
	static GLUquadricObj *quadrObj = NULL;

	float static fAngleX[3];
	float static fAngleY[3];

	/************************ GL initializing *********************/
	if (ws->initFlag) {
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
		gluQuadricDrawStyle(quadrObj, GLU_FILL);
		gluQuadricNormals(quadrObj, GLU_SMOOTH);

		/* Getting start random rotation angles for cubes */
		for (i = 0; i < 3; i ++) {
			fAngleY[i] = (float)randval(60);
			fAngleX[i] = (float)randval(360);
		}

		/* Generating textures */
		create_digits_tex_array();

		glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

		glTexParameterf(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameterf(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		glGenTextures(1, &tex_flame);
		glGenTextures(1, &tex_second);
		glGenTextures(1, &tex_minute);
		glGenTextures(1, &tex_hour);

		glFlush();
	}

	/************************** Closing window ********************/
	if (ws->destroyFlag) {
		destroy_digits_tex_array();
	}

	/* Checking mouse cursor position and stop program if it changes. */
	if (mousePos->iRootX > 0 &&
	    mousePos->iRootY > 0 &&
	    iStartMousePosX == IMPOSSIBLE_VAL &&
	    iStartMousePosY == IMPOSSIBLE_VAL) {
		iStartMousePosX = mousePos->iRootX;
		iStartMousePosY = mousePos->iRootY;
	} else if ((iStartMousePosX != IMPOSSIBLE_VAL || iStartMousePosY != IMPOSSIBLE_VAL) &&
	    ((iStartMousePosX != mousePos->iRootX) || (iStartMousePosY != mousePos->iRootY))) {
		_appExit = 1;
	}

	/************************* Render to texture ******************/
	glDisable(GL_LIGHTING);
	glEnable(GL_TEXTURE_RECTANGLE);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
	glEnable(GL_COLOR_MATERIAL);
	time(&rawtime);
	localtime_r(&rawtime, &tminfo);
	
	/* Create framing digits on edges textures */
	if (uSec != tminfo.tm_sec) {
		uSec = tminfo.tm_sec; 
		draw_time_edge_texture(tminfo.tm_sec, tex_second);
		if (uMin != tminfo.tm_min) {
			uMin = tminfo.tm_min; 
			draw_time_edge_texture(tminfo.tm_min, tex_minute);
			if (uHour != tminfo.tm_hour) {
				uHour = tminfo.tm_hour; 
				draw_time_edge_texture(tminfo.tm_hour, tex_hour);
			}
		}
	}

	/* Flame updating. */
	flame_update();

	/*********************** Render to screen *********************/
	/* Rotations calculation */
	fDelta = CUBE_ROTATION_SPEED * float(cur_time_ms - prev_time_ms);
	prev_time_ms = cur_time_ms;
	
	for (i = 0; i < 3; i ++) {
		fAngleX[i] = (float)fAngleX[i] + fDelta;
		if (fAngleX[i] > 360.0f) {
			fAngleX[i] = 0.0f;
		}
		fAngleY[i] = fAngleY[i] + dY[i];
		
		if (fAngleY[i] > 60.0f || fAngleY[i] < -60.0f) {
			dY[i] = -dY[i];
		}
	}

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(50, (double)fX, 0.5, 500);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	gluLookAt(0, 0, 6, 0, 0, 0, 0, 1, 0);
	glDepthFunc(GL_LEQUAL);
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
	
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glViewport(0, 0, ws->width, ws->height);

	glLoadIdentity();

	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	
	glEnable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);

    glBlendFunc(GL_SRC_ALPHA,GL_ONE);
	glEnable(GL_TEXTURE_RECTANGLE);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	/* Drawing flame quad */
	glBindTexture(GL_TEXTURE_RECTANGLE, tex_flame);
	glTexImage2D(GL_TEXTURE_RECTANGLE, 0, 3, FLAME_WIDTH, FLAME_HEIGHT, 
	    0, GL_RGB, GL_UNSIGNED_BYTE, _scr_buf);	
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
		glBindTexture(GL_TEXTURE_RECTANGLE, tex_hour);
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
		glBindTexture(GL_TEXTURE_RECTANGLE, tex_minute);
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
		glBindTexture(GL_TEXTURE_RECTANGLE, tex_second);
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
	
	glBindTexture(GL_TEXTURE_RECTANGLE, tex_flame);
	
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

static void
events_update(XEvent *event) {

	switch (event->type) {
	case ButtonPress:
	case KeyPress:
		_appExit = 1;
		break;
	}
}

int
main(int argc, char **argv) {
	cGLXWindow window;

	window.create_window(0, 0, "3dclock", redraw_window);
	window.init_events_callback(events_update);
	//window.hide_cursor();
	window.set_window_fullscreen_popup();

	while (0 < window.update_window() && _appExit == 0) {
		sleep_millisec(1);
	}
	
	window.destroy_window();
	window.show_cursor();

	return 0;
}
