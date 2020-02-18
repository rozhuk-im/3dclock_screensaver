/*
 *  Copyright (c) 2002 - 2020 Naezzhy Petr(Наезжий Пётр) <petn@mail.ru>
 *  Copyright (c) 2020 Rozhuk Ivan <rozhuk.im@gmail.com>
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


#include <sys/param.h>
#include <sys/types.h>
#include <stdint.h>
#include <errno.h>

#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/glu.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#include "glxwindow.h"

#ifndef __unused
#	define __unused		__attribute__((__unused__))
#endif


#define FONT_HEIGHT		256

#define BITMAP_WIDTH		512
#define BITMAP_HEIGHT		512

#define FLAME_WIDTH		1024
#define FLAME_HEIGHT		1024

#define IMPOSSIBLE_VAL		9999
#define CUBE_ROTATION_SPEED	0.006f

#define FONT_NAME		"./fonts/Roboto-Bold.ttf"


typedef struct rgb_s {
	uint8_t r;
	uint8_t g;
	uint8_t b;
} rgb_t, *rgb_p;

typedef struct digit_descriptor_s {
	uint32_t width;
	uint32_t height;
	uint32_t top;
	int32_t left;
	GLuint texture;
} digit_desc_t, *digit_desc_p;


typedef struct clock_3d_s {
	volatile int	running;
	rgb_t		flame_buf[FLAME_WIDTH][FLAME_HEIGHT];
	digit_desc_t	digit_desc[10];
	glx_wnd_t	glx_wnd;
} clock_3d_t, *clock_3d_p;



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

static inline uint32_t
randval(uint32_t max_val) {
	return (arc4random() % (max_val + 1));
}

static void
flame_update(clock_3d_p clock_3d) {
	size_t i, j;
	uint8_t tmp, color, pal_buf[FLAME_WIDTH][FLAME_HEIGHT];
	static rgb_t palit[256];
	static int palit_init_done = 0;

	/* Filling flame palit. */
	if (0 == palit_init_done) {
		palit_init_done ++;
		memset(&palit, 0x00, sizeof(palit));
		for (i = 0; i < 64; i ++) {
			palit[i +   0].r = (uint8_t)(i * 4);
			palit[i +  64].r = 0xff;
			palit[i +  64].g = (uint8_t)(i * 4);
			palit[i + 128].r = 0xff;
			palit[i + 128].g = 0xff;
			palit[i + 128].b = (uint8_t)(i * 4);
			palit[i + 192].r = 0xff;
			palit[i + 192].g = 0xff;
			palit[i + 192].b = 0xff;
		}
	}

	/* Flame seeds. */
	for (i = 0; i < FLAME_WIDTH; i += 8) {
		color = (uint8_t)arc4random();
		for (j = 0; j < 8; j ++) {
			pal_buf[i + j][0] = color;
		}
	}

#if 0
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
			pal_buf[i][j] = tmp;
			clock_3d->flame_buf[i][j] = palit[tmp];
		}
	}
#else
	/* Two pass flame gen method. */
	for (i = 1; i < (FLAME_WIDTH - 1); i ++) {
		for (j = 1; j < (FLAME_HEIGHT - 1); j ++) {
			tmp = (uint8_t)((pal_buf[i - 1][j - 1] +
			    pal_buf[i][j - 1] +
			    pal_buf[i + 1][j - 1]) / 2.97f);
			if (tmp > 1) {
				tmp --;
			} else {
				tmp = 0;
			}
			pal_buf[i][j] = tmp;
		}
	}
	for (i = (FLAME_WIDTH - 2); i > 1; i --) {
		for (j = 1; j < (FLAME_HEIGHT - 1); j ++) {
			tmp = (uint8_t)((pal_buf[i - 1][j - 1] +
			    pal_buf[i][j - 1] +
			    pal_buf[i + 1][j - 1]) / 2.97f);
			if (tmp > 1) {
				tmp --;
			} else {
				tmp = 0;
			}
			pal_buf[i][j] = tmp;
			clock_3d->flame_buf[i][j] = palit[tmp];
		}
	}
#endif
	//for (i = 0; i < (FLAME_WIDTH * FLAME_HEIGHT); i ++) {
	//	clock_3d->flame_buf[0][i] = palit[pal_buf[0][i]];
	//}
}


/* Generates digits textures from tt fonts. */
static int	
create_digits_tex_array(clock_3d_p clock_3d) {
	int error = -1;
	FT_Library lib = NULL;
	FT_Face font = NULL;
	FT_GlyphSlot gliph = NULL;
	uint8_t *bitmap = NULL;
	size_t i, x, y, bm_size;

	memset(&clock_3d->digit_desc, 0x00, sizeof(clock_3d->digit_desc));

	if (0 != FT_Init_FreeType(&lib))
		return (-1);

	if (0 != FT_New_Face(lib, FONT_NAME, 0, &font))
		goto err_out;

	if (0 != FT_Set_Char_Size(font, (FONT_HEIGHT << 6),
	    (FONT_HEIGHT << 6), 96, 96))
		goto err_out;

	gliph = font->glyph;
	for (i = 0; i < nitems(clock_3d->digit_desc); i ++) {
		if (0 != FT_Load_Char(font, (i + '0'), FT_LOAD_RENDER))
			goto err_out;

		clock_3d->digit_desc[i].width = gliph->bitmap.width;
		clock_3d->digit_desc[i].height = gliph->bitmap.rows;
		clock_3d->digit_desc[i].top = (uint32_t)gliph->bitmap_top;
		clock_3d->digit_desc[i].left = gliph->bitmap_left;

		/* Two bytes for each pixel. */
		bm_size = (2 * clock_3d->digit_desc[i].width *
		    clock_3d->digit_desc[i].height);	
		bitmap = (uint8_t*)malloc(bm_size);
		if (NULL == bitmap) {
			error = ENOMEM;
			goto err_out;
		}
		memset(bitmap, 0xff, bm_size);

		/* Filling bitmap grey&alpha bytes. */
		for (y = 0; y < clock_3d->digit_desc[i].height; y ++) {
			for (x = 0; x < clock_3d->digit_desc[i].width; x ++) {
				bitmap[2 * (x + y * gliph->bitmap.width) + 1] =
				    (uint8_t)(0.97f * gliph->bitmap.buffer[x + y * gliph->bitmap.width]);
			}
		}

		/* Creating symbol texture. */
		glGenTextures(1, &clock_3d->digit_desc[i].texture);
		if (0 == clock_3d->digit_desc[i].texture)
			goto err_out;
		glEnable(GL_TEXTURE_RECTANGLE);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		glBindTexture(GL_TEXTURE_RECTANGLE, clock_3d->digit_desc[i].texture);
		glTexParameterf(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameterf(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexImage2D(GL_TEXTURE_RECTANGLE, 0, GL_RGBA, 
		    (GLsizei)clock_3d->digit_desc[i].width,
		    (GLsizei)clock_3d->digit_desc[i].height, 0,
		    GL_LUMINANCE_ALPHA,	GL_UNSIGNED_BYTE, bitmap);

		free(bitmap);
		bitmap = NULL;
	}
	error = 0;

err_out:
	if (0 != error) {
		for (i = 0; i < nitems(clock_3d->digit_desc); i ++) {
			glDeleteTextures(1, &clock_3d->digit_desc[i].texture);
		}
	}
	free(bitmap);
	FT_Done_Face(font);
	FT_Done_FreeType(lib);

	return (error);
}

static void
destroy_digits_tex_array(clock_3d_p clock_3d)
{
	for (size_t i = 0; i < nitems(clock_3d->digit_desc); i ++) {
		glDeleteTextures(1, &clock_3d->digit_desc[i].texture);
	}
	memset(&clock_3d->digit_desc, 0x00, sizeof(clock_3d->digit_desc));
}


/* Draw didx textured quads. */
static void
draw_gliph_quads(clock_3d_p clock_3d, const int time_val) {
	size_t i, didx;
	uint32_t x, y;
	uint8_t time_digits[2];

	if (0 > time_val || 99 < time_val)
		return;
	time_digits[0] = (uint8_t)(time_val / 10);
	time_digits[1] = (uint8_t)(time_val % 10);

	x = (uint32_t)((BITMAP_WIDTH - ((2 * clock_3d->digit_desc[0].width) +
	    (uint32_t)clock_3d->digit_desc[0].left)) * 0.45f);
	y = ((BITMAP_HEIGHT - FONT_HEIGHT) / 2);

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
		glBindTexture(GL_TEXTURE_RECTANGLE,
		    clock_3d->digit_desc[didx].texture);

		x += (uint32_t)(clock_3d->digit_desc[didx].left + 1);
		y -= (clock_3d->digit_desc[didx].height - clock_3d->digit_desc[didx].top);

		glBegin(GL_QUADS);
		{
			glTexCoord2f((clock_3d->digit_desc[didx].width - 1),
			    0.0f);
			glVertex3f((x + clock_3d->digit_desc[didx].width),
			    (y + clock_3d->digit_desc[didx].height), 0.5f);

			glTexCoord2f(0.0f, 0.0f);
			glVertex3f(x,
			    (y + clock_3d->digit_desc[didx].height),
			    0.5f);

			glTexCoord2f(0.0f,
			    (clock_3d->digit_desc[didx].height - 1));
			glVertex3f(x, y, 0.5f);

			glTexCoord2f((clock_3d->digit_desc[didx].width - 1),
			    (clock_3d->digit_desc[didx].height - 1));
			glVertex3f((x + clock_3d->digit_desc[didx].width),
			    y, 0.5f);
		}
		glEnd();

		x += clock_3d->digit_desc[didx].width;
	}
}


static void
draw_time_edge_texture(clock_3d_p clock_3d, const int time_val,
    const GLuint tex_id) {
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
	/* Background quad. */
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

	/* Borders. */
	glColor4f(0.1f, 0.1f, 1.0f, 0.9f);

	/* Bottom. */
	glBegin(GL_QUADS);
	{
		glNormal3f(0.0f, 0.0f, 1.0f);
		glVertex3f(0.0f, 0.0f, 1.0f);
		glVertex3f((BITMAP_WIDTH - 1), 0.0f, 1.0f);
		glVertex3f((BITMAP_WIDTH - 1), border_width, 1.0f);
		glVertex3f(0.0f, border_width, 1.0f);
	}
	glEnd();

	/* Left. */
	glBegin(GL_QUADS);
	{
		glNormal3f(0.0f, 0.0f, 1.0f);
		glVertex3f(0.0f, 0.0f, 1.0f);
		glVertex3f(border_width, 0.0f, 1.0f);
		glVertex3f(border_width, (BITMAP_HEIGHT - 1), 1.0f);
		glVertex3f(0.0f, (BITMAP_HEIGHT - 1), 1.0f);
	}
	glEnd();

	/* Right. */
	glBegin(GL_QUADS);
	{
		glNormal3f(0.0f, 0.0f, 1.0f);
		glVertex3f((BITMAP_WIDTH - 1 - border_width), 0.0f, 1.0f);
		glVertex3f((BITMAP_WIDTH - 1), 0.0f, 1.0f);
		glVertex3f((BITMAP_WIDTH - 1), (BITMAP_HEIGHT - 1), 1.0f);
		glVertex3f((BITMAP_WIDTH - 1 - border_width), (BITMAP_HEIGHT - 1), 1.0f);
	}
	glEnd();

	/* Top. */
	glBegin(GL_QUADS);
	{
		glNormal3f(0.0f, 0.0f, 1.0f);
		glVertex3f(0.0f, (BITMAP_HEIGHT - 1), 1.0f);
		glVertex3f((BITMAP_WIDTH - 1), (BITMAP_HEIGHT - 1), 1.0f);
		glVertex3f((BITMAP_WIDTH - 1), (BITMAP_HEIGHT - 1- border_width), 1.0f);
		glVertex3f(0.0f, (BITMAP_HEIGHT - 1 - border_width), 1.0f);
	}
	glEnd();

	/* Triangles. */
	/* Bottom-left. */
	glBegin(GL_TRIANGLES);
	{
		glNormal3f(0.0f, 0.0f, 1.0f);
		glVertex3f(0.0f, 0.0f, 1.0f);
		glVertex3f(cathet, 0.0f, 1.0f);
		glVertex3f(0.0f, cathet, 1.0f);
	}
	glEnd();

	/* Bottom-right. */
	glBegin(GL_TRIANGLES);
	{
		glNormal3f(0.0f, 0.0f, 1.0f);
		glVertex3f((BITMAP_WIDTH - 1), 0.0f, 1.0f);
		glVertex3f((BITMAP_WIDTH - 1 - cathet), 0.0f, 1.0f);
		glVertex3f((BITMAP_WIDTH - 1), cathet, 1.0f);
	}
	glEnd();

	/* Top-left. */
	glBegin(GL_TRIANGLES);
	{
		glNormal3f(0.0f, 0.0f, 1.0f);
		glVertex3f(0.0f, (BITMAP_HEIGHT - 1), 1.0f);
		glVertex3f(cathet, (BITMAP_HEIGHT - 1), 1.0f);
		glVertex3f(0.0f, (BITMAP_HEIGHT - 1 - cathet), 1.0f);
	}
	glEnd();

	/* Top-right. */
	glBegin(GL_TRIANGLES);
	{
		glNormal3f(0.0f, 0.0f, 1.0f);
		glVertex3f((BITMAP_WIDTH - 1), (BITMAP_HEIGHT - 1), 1.0f);
		glVertex3f((BITMAP_WIDTH - 1 - cathet), (BITMAP_HEIGHT - 1), 1.0f);
		glVertex3f((BITMAP_WIDTH - 1), (BITMAP_HEIGHT - 1 - cathet), 1.0f);
	}
	glEnd();

	/* Lines. */
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

	draw_gliph_quads(clock_3d, time_val);

	glEnable(GL_TEXTURE_RECTANGLE);
	glBindTexture(GL_TEXTURE_RECTANGLE, tex_id);
	glCopyTexImage2D(GL_TEXTURE_RECTANGLE, 0, GL_RGBA, 0, 0, BITMAP_WIDTH, BITMAP_HEIGHT, 0);
}


/* Redraw window callback. */
static void
redraw_window(glx_wnd_p glx_wnd __unused, const uint32_t flags,
    const wnd_state_p ws, const mcur_pos_p mcur_pos, void *udata) {
	clock_3d_p clock_3d = udata;
	size_t i;
	time_t rawtime;
	struct tm tminfo;
	float fDelta;

	const float light0Diffuse[] = { 1.0f, 1.0f, 1.0f };
	const float light0Ambient[] = { 0.3f, 0.3f, 0.3f };
	const float light0Direction[] = { 0.0f, 0.0f, 1.0f, 0.0f };
	const float cube_x[] = { 2.0f, 0.0f, -2.0f };
	const float sphere_x[] = { 1.0f, 1.0f, -1.0f, -1.0f };
	const float sphere_y[] = { 0.2f, -0.2f, 0.2f, -0.2f };
	const float range_z = -5.5f;
	const float fX = ((float)ws->width / (float)ws->height);
	const uint64_t cur_time_ms = get_millisec();

	static float dY[] = { 0.2f, 0.2f, 0.2f };
	static int uSec = -1, uMin = -1, uHour = -1;
	static int32_t iStartMousePosX = IMPOSSIBLE_VAL;
	static int32_t iStartMousePosY = IMPOSSIBLE_VAL;
	static GLuint cube_tex[3], flame_tex;
	static float cube_angle_x[3];
	static float cube_angle_y[3];
	static uint64_t prev_time_ms = 0;
	static GLUquadricObj *sphere_obj = NULL;

	/************************ GL initializing *********************/
	if (0 != (GLX_WND_REDRAW_F_INIT & flags)) {
		glEnable(GL_POLYGON_SMOOTH);
		glShadeModel(GL_SMOOTH);
		glEnable(GL_NORMALIZE);
		glDepthFunc(GL_LEQUAL);
		glEnable(GL_MULTISAMPLE);

		glEnable(GL_COLOR_MATERIAL);
		glEnable(GL_TEXTURE_RECTANGLE);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

		/* Init sphere objects. */
		sphere_obj = gluNewQuadric();
		gluQuadricDrawStyle(sphere_obj, GLU_FILL);
		gluQuadricNormals(sphere_obj, GLU_SMOOTH);

		/* Generating textures. */
		create_digits_tex_array(clock_3d);

		glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

		glTexParameterf(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameterf(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		glGenTextures(1, &flame_tex);
		for (i = 0; i < nitems(cube_tex); i ++) {
			glGenTextures(1, &cube_tex[i]);
			/* Getting start random rotation angles for cubes. */
			cube_angle_y[i] = randval(60);
			cube_angle_x[i] = randval(360);
		}

		glFlush();
	}

	/************************** Closing window ********************/
	if (0 != (GLX_WND_REDRAW_F_DESTROY & flags)) {
		destroy_digits_tex_array(clock_3d);
		return;
	}

	/* Checking mouse cursor position and stop program if it changes. */
	if (mcur_pos->root_x > 0 &&
	    mcur_pos->root_y > 0 &&
	    iStartMousePosX == IMPOSSIBLE_VAL &&
	    iStartMousePosY == IMPOSSIBLE_VAL) {
		iStartMousePosX = mcur_pos->root_x;
		iStartMousePosY = mcur_pos->root_y;
	} else if ((iStartMousePosX != IMPOSSIBLE_VAL || iStartMousePosY != IMPOSSIBLE_VAL) &&
	    ((iStartMousePosX != mcur_pos->root_x) || (iStartMousePosY != mcur_pos->root_y))) {
		//clock_3d->running = 0;
	}

	/************************* Render to texture ******************/
	glDisable(GL_LIGHTING);
	glEnable(GL_TEXTURE_RECTANGLE);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
	glEnable(GL_COLOR_MATERIAL);

	/* Create framing digits on edges textures. */
	time(&rawtime);
	localtime_r(&rawtime, &tminfo);
	if (uSec != tminfo.tm_sec) {
		uSec = tminfo.tm_sec; 
		draw_time_edge_texture(clock_3d, tminfo.tm_sec, cube_tex[0]);
		if (uMin != tminfo.tm_min) {
			uMin = tminfo.tm_min; 
			draw_time_edge_texture(clock_3d, tminfo.tm_min, cube_tex[1]);
			if (uHour != tminfo.tm_hour) {
				uHour = tminfo.tm_hour; 
				draw_time_edge_texture(clock_3d, tminfo.tm_hour, cube_tex[2]);
			}
		}
	}

	/* Flame updating. */
	flame_update(clock_3d);

	/*********************** Render to screen *********************/
	/* Rotations calculation. */
	fDelta = CUBE_ROTATION_SPEED * (float)(cur_time_ms - prev_time_ms);
	prev_time_ms = cur_time_ms;
	
	for (i = 0; i < nitems(cube_angle_x); i ++) {
		cube_angle_x[i] += fDelta;
		if (cube_angle_x[i] > 360.0f) {
			cube_angle_x[i] = 0.0f;
		}
		cube_angle_y[i] += dY[i];
		if (cube_angle_y[i] > 60.0f || cube_angle_y[i] < -60.0f) {
			dY[i] = -dY[i];
		}
	}

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(50.0, (double)fX, 0.5, 500.0);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	gluLookAt(0.0, 0.0, 6.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0);
	glDepthFunc(GL_LEQUAL);
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
	
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glViewport(0, 0, (GLsizei)ws->width, (GLsizei)ws->height);

	glLoadIdentity();

	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	glEnable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);

	glBlendFunc(GL_SRC_ALPHA,GL_ONE);
	glEnable(GL_TEXTURE_RECTANGLE);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);


	/* Drawing flame quad */
	glBindTexture(GL_TEXTURE_RECTANGLE, flame_tex);
	glTexImage2D(GL_TEXTURE_RECTANGLE, 0, 3, FLAME_WIDTH, FLAME_HEIGHT,
	    0, GL_RGB, GL_UNSIGNED_BYTE, clock_3d->flame_buf);
	glDisable(GL_LIGHTING);
	
	glColor4f(1.0f, 1.0f, 1.0f, 0.9f);
	glPushMatrix();
	{
		glTranslatef(0.0f, 0.0f, -10.0f);
		glBegin(GL_QUADS);
		{
			glNormal3f(0.0f, 0.0f, 1.0f);
			glTexCoord2f(0.0f, 0.0f);
			glVertex3f((-5.0f * fX), -5.2f, 0.0f);
			glTexCoord2f(((FLAME_WIDTH / 2) - 1), 0.0f);
			glVertex3f((-5.0f * fX), 4.0f, 0.0f);
			glTexCoord2f(((FLAME_WIDTH / 2) - 1), (FLAME_HEIGHT - 1));
			glVertex3f((5.0f * fX), 4.0f, 0.0f);
			glTexCoord2f(0.0f, (FLAME_HEIGHT - 1));
			glVertex3f((5.0f * fX), -5.2f, 0.0f);
		}
		glEnd();
	}
	glPopMatrix();

	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, light0Diffuse);
	glLightfv(GL_LIGHT0, GL_AMBIENT, light0Ambient);
	glLightfv(GL_LIGHT0, GL_POSITION, light0Direction);

	glEnable(GL_COLOR_MATERIAL);
	glLightModelf(GL_LIGHT_MODEL_TWO_SIDE, GL_FALSE);

	/* Drawing time cubes. */
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	for (i = 0; i < nitems(cube_x); i ++) {
		glPushMatrix();
		{
			glTranslatef(cube_x[i], 0.0f, range_z);
			glRotatef(cube_angle_y[i], 1.0f, 0.0f, 0.0f);
			glRotatef(cube_angle_x[i], 0.0f, 1.0f, 0.0f);
			glBindTexture(GL_TEXTURE_RECTANGLE, cube_tex[i]);
			glBegin(GL_QUADS);
			{
				glNormal3f(0.0f, 0.0f, 0.2f);
				glTexCoord2f(BITMAP_WIDTH, BITMAP_HEIGHT);
				glVertex3f(0.5f, 0.5f, 0.5f);
				glTexCoord2f(0, BITMAP_HEIGHT);
				glVertex3f(-0.5f, 0.5f, 0.5f);
				glTexCoord2f(0.0f, 0.0f);
				glVertex3f(-0.5f, -0.5f, 0.5f);
				glTexCoord2f(BITMAP_WIDTH, 0.0f);
				glVertex3f(0.5f, -0.5f, 0.5f);

				glNormal3f(0.0f, 0.0f,-0.2f);
				glTexCoord2f(BITMAP_WIDTH, 0.0f);
				glVertex3f(-0.5f, -0.5f, -0.5f);
				glTexCoord2f(BITMAP_WIDTH, BITMAP_HEIGHT);
				glVertex3f(-0.5f, 0.5f, -0.5f);
				glTexCoord2f(0.0f, BITMAP_HEIGHT);
				glVertex3f(0.5f, 0.5f, -0.5f);
				glTexCoord2f(0.0f, 0.0f);
				glVertex3f(0.5f, -0.5f, -0.5f);

				glNormal3f(0.0f, 0.2f, 0.0f);
				glTexCoord2f(0.0f, 0.0f);
				glVertex3f(0.5f, 0.5f, 0.5f);
				glTexCoord2f(BITMAP_WIDTH, 0.0f);
				glVertex3f(0.5f, 0.5f, -0.5f);
				glTexCoord2f(BITMAP_WIDTH, BITMAP_HEIGHT);
				glVertex3f(-0.5f, 0.5f, -0.5f);
				glTexCoord2f(0.0f, BITMAP_HEIGHT);
				glVertex3f(-0.5f, 0.5f, 0.5f);

				glNormal3f(0.0f,-0.2f, 0.0f);
				glTexCoord2f(BITMAP_WIDTH, BITMAP_HEIGHT);
				glVertex3f(-0.5f,-0.5f, -0.5f);
				glTexCoord2f(0.0f, BITMAP_HEIGHT);
				glVertex3f(0.5f, -0.5f, -0.5f);
				glTexCoord2f(0.0f, 0.0f);
				glVertex3f(0.5f, -0.5f, 0.5f);
				glTexCoord2f(BITMAP_WIDTH, 0.0f);
				glVertex3f(-0.5f, -0.5f, 0.5f);

				glNormal3f(0.2f, 0.0f, 0.0f);
				glTexCoord2f(0.0f, BITMAP_HEIGHT);
				glVertex3f(0.5f, 0.5f, 0.5f);
				glTexCoord2f(0.0f, 0.0f);
				glVertex3f(0.5f, -0.5f, 0.5f);
				glTexCoord2f(BITMAP_WIDTH, 0.0f);
				glVertex3f(0.5f, -0.5f, -0.5f);
				glTexCoord2f(BITMAP_WIDTH, BITMAP_HEIGHT);
				glVertex3f(0.5f, 0.5f, -0.5f);

				glNormal3f(-0.2f, 0.0f, 0.0f);
				glTexCoord2f(0.0f, 0.0f);
				glVertex3f(-0.5f, -0.5f, -0.5f);
				glTexCoord2f(BITMAP_WIDTH, 0.0f);
				glVertex3f(-0.5f, -0.5f, 0.5f);
				glTexCoord2f(BITMAP_WIDTH, BITMAP_HEIGHT);
				glVertex3f(-0.5f, 0.5f, 0.5f);
				glTexCoord2f(0.0f, BITMAP_HEIGHT);
				glVertex3f(-0.5f, 0.5f, -0.5f);
			}
			glEnd();
		}
		glPopMatrix();
	}

#if 0
	/* Drawing flame reflections on cube edges. */
	/* Getting path of full flame texture. */
	glTexParameterf(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameterf(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glBindTexture(GL_TEXTURE_RECTANGLE, flame_tex);
	glColor4f(0.3f, 0.0f, 0.0f, 0.5f);
	for (i = 0; i < nitems(cube_x); i ++) {
		glPushMatrix();
		{
			glTranslatef(cube_x[i], 0.0f, range_z);
			glRotatef(cube_angle_y[i], 1.0f, 0.0f, 0.0f);
			glRotatef(cube_angle_x[i], 0.0f, 1.0f, 0.0f);
			glBegin(GL_QUADS);
			{
				glNormal3f(0.0f, 0.0f, 1.0f);
				glTexCoord2f((BITMAP_WIDTH / 8), (BITMAP_HEIGHT / 16));
				glVertex3f(0.5f, 0.5f, 0.5f);
				glTexCoord2f((BITMAP_WIDTH / 8), (BITMAP_HEIGHT / 8));
				glVertex3f(-0.5f, 0.5f, 0.5f);
				glTexCoord2f((BITMAP_WIDTH / 16), (BITMAP_HEIGHT / 8));
				glVertex3f(-0.5f, -0.5f, 0.5f);
				glTexCoord2f((BITMAP_WIDTH / 16), (BITMAP_HEIGHT / 16));
				glVertex3f(0.5f, -0.5f, 0.5f);

				glNormal3f(0.0f, 0.0f,-1.0f);
				glTexCoord2f((BITMAP_WIDTH / 16), (BITMAP_HEIGHT / 16));
				glVertex3f(-0.5f, -0.5f, -0.5f);
				glTexCoord2f((BITMAP_WIDTH / 8), (BITMAP_HEIGHT / 16));
				glVertex3f(-0.5f, 0.5f, -0.5f);
				glTexCoord2f((BITMAP_WIDTH / 8), (BITMAP_HEIGHT / 8));
				glVertex3f(0.5f, 0.5f, -0.5f);
				glTexCoord2f((BITMAP_WIDTH / 16), (BITMAP_HEIGHT / 8));
				glVertex3f(0.5f, -0.5f, -0.5f);

				glNormal3f(0.0f, 1.0f, 0.0f);
				glTexCoord2f((BITMAP_WIDTH / 16), (BITMAP_HEIGHT / 16));
				glVertex3f(0.5f, 0.5f, 0.5f);
				glTexCoord2f((BITMAP_WIDTH / 8), (BITMAP_HEIGHT / 16));
				glVertex3f(0.5f, 0.5f, -0.5f);
				glTexCoord2f((BITMAP_WIDTH / 8), (BITMAP_HEIGHT / 8));
				glVertex3f(-0.5f, 0.5f, -0.5f);
				glTexCoord2f((BITMAP_WIDTH / 16), (BITMAP_HEIGHT / 8));
				glVertex3f(-0.5f, 0.5f, 0.5f);

				glNormal3f(0.0f, -1.0f, 0.0f);
				glTexCoord2f((BITMAP_WIDTH / 8), (BITMAP_HEIGHT / 8));
				glVertex3f(-0.5f, -0.5f, -0.5f);
				glTexCoord2f((BITMAP_WIDTH / 16), (BITMAP_HEIGHT / 8));
				glVertex3f(0.5f, -0.5f, -0.5f);
				glTexCoord2f((BITMAP_WIDTH / 16), (BITMAP_HEIGHT / 16));
				glVertex3f(0.5f, -0.5f, 0.5f);
				glTexCoord2f((BITMAP_WIDTH / 8), (BITMAP_HEIGHT / 16));
				glVertex3f(-0.5f, -0.5f, 0.5f);

				glNormal3f(1.0f, 0.0f, 0.0f);
				glTexCoord2f((BITMAP_WIDTH / 8), (BITMAP_HEIGHT / 8));
				glVertex3f(0.5f, 0.5f, 0.5f);
				glTexCoord2f((BITMAP_WIDTH / 16), (BITMAP_HEIGHT / 8));
				glVertex3f(0.5f, -0.5f, 0.5f);
				glTexCoord2f((BITMAP_WIDTH / 16), (BITMAP_HEIGHT / 16));
				glVertex3f(0.5f, -0.5f, -0.5f);
				glTexCoord2f((BITMAP_WIDTH / 8), (BITMAP_HEIGHT / 16));
				glVertex3f(0.5f, 0.5f, -0.5f);

				glNormal3f(-1.0f, 0.0f, 0.0f);
				glTexCoord2f((BITMAP_WIDTH / 16), (BITMAP_HEIGHT / 8));
				glVertex3f(-0.5f, -0.5f, -0.5f);
				glTexCoord2f((BITMAP_WIDTH / 16), (BITMAP_HEIGHT / 16));
				glVertex3f(-0.5f, -0.5f, 0.5f);
				glTexCoord2f((BITMAP_WIDTH / 8), (BITMAP_HEIGHT / 16));
				glVertex3f(-0.5f, 0.5f, 0.5f);
				glTexCoord2f((BITMAP_WIDTH / 8), (BITMAP_HEIGHT / 8));
				glVertex3f(-0.5f, 0.5f, -0.5f);
			}
			glEnd();
		}
		glPopMatrix();
	}
#endif

	/* Drawing spheres between cubes. */
	glDisable(GL_TEXTURE_RECTANGLE);
	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);
	glColor3f(0.4f, 0.2f, 0.2f);
	for (i = 0; i < nitems(sphere_x); i ++) {
		glPushMatrix();
		{
			glNormal3f(0.0f, 0.0f, 1.0f);
			glTranslatef(sphere_x[i], sphere_y[i], range_z);
			gluSphere(sphere_obj, 0.1, 16, 16);
		}
		glPopMatrix();
	}

	glFlush();
}

static void
events_update(glx_wnd_p glx_wnd __unused, const XEvent *event,
    void *udata) {
	clock_3d_p clock_3d = udata;

	switch (event->type) {
	case ButtonPress:
	case KeyPress:
		clock_3d->running = 0;
		break;
	}
}


int
main(int argc __unused, char **argv __unused) {
	int error;
	clock_3d_t clock_3d;

	memset(&clock_3d, 0x00, sizeof(clock_3d));
	clock_3d.running ++;

	error = glx_wnd_create(0, 0, "3dclock", redraw_window,
	    events_update, &clock_3d, &clock_3d.glx_wnd);
	if (0 != error)
		return (error);
	//glx_wnd_hide_cursor(&clock_3d.glx_wnd);
	glx_wnd_set_window_fullscreen_popup(&clock_3d.glx_wnd);

	while (0 == glx_wnd_update_window(&clock_3d.glx_wnd) &&
	    0 != clock_3d.running) {
		sleep_millisec(1);
	}

	glx_wnd_show_cursor(&clock_3d.glx_wnd);
	glx_wnd_destroy(&clock_3d.glx_wnd);

	return (0);
}
