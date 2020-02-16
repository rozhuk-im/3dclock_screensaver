/*
 *  Copyright (c) 2002-2020 Naezzhy Petr(Наезжий Пётр) <petn@mail.ru>
 *  Copyright (c) 2020-2024 Rozhuk Ivan <rozhuk.im@gmail.com>
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


#define CUBES_COUNT		3

#define FONT_HEIGHT		256

#define BITMAP_WIDTH		512
#define BITMAP_HEIGHT		512

#define FLAME_WIDTH		1024
#define FLAME_HEIGHT		1024

#define CUBE_ROTATION_SPEED	0.006f

#define FONT_NAME		"./fonts/Roboto-Bold.ttf"

static const float light0Diffuse[] = { 1.0f, 1.0f, 1.0f, 1.0f };
static const float light0Ambient[] = { 0.3f, 0.3f, 0.3f, 0.3f };
static const float light0Direction[] = { 0.0f, 0.0f, 1.0f, 0.0f };
static const float cube_x[] = { -2.0f, 0.0f, 2.0f };
static const float sphere_x[] = { 1.0f, 1.0f, -1.0f, -1.0f };
static const float sphere_y[] = { 0.2f, -0.2f, 0.2f, -0.2f };
static const float range_z = -5.5f;


typedef struct rgb_s {
	uint8_t		r;
	uint8_t		g;
	uint8_t		b;
} rgb_t, *rgb_p;

typedef struct digit_descriptor_s {
	uint32_t	width;
	uint32_t	height;
	int32_t		top;
	int32_t		left;
	GLuint		texture;
} digit_desc_t, *digit_desc_p;

typedef struct cube_s {
	uint32_t	digit;
	GLuint		texture;
	float		x;
	float		y;
	float		d_y;
	float		angle_x;
	float		angle_y;
} cube_t, *cube_p;

typedef struct cube_3d_clock_s {
	volatile int	running;
	rgb_t		flame_buf[FLAME_WIDTH][FLAME_HEIGHT];
	digit_desc_t	digit_desc[10];
	cube_t		cubes[CUBES_COUNT];
	int32_t		mpos_x;
	int32_t		mpos_y;
	GLuint		flame_tex;
	uint64_t	prev_time_ms;
	GLUquadricObj	*sphere_obj;
	glx_wnd_t	glx_wnd;
} c3d_clk_t, *c3d_clk_p;



static inline uint64_t
get_millisec(void) {
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);

	return ((uint64_t)((ts.tv_sec * 1000) + (ts.tv_nsec / 1000000)));
}

static inline void
sleep_millisec(uint32_t ms) {
	struct timespec	ts;

	ts.tv_sec = (ms / 1000);
	ts.tv_nsec = ((ms * 1000000) - (ts.tv_sec * 1000000000));
	nanosleep(&ts, NULL);
}

static inline uint32_t
randval(uint32_t max_val) {
	return (arc4random() % (max_val + 1));
}

static void
flame_update(c3d_clk_p c3d_clk) {
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
			c3d_clk->flame_buf[i][j] = palit[tmp];
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
			c3d_clk->flame_buf[i][j] = palit[tmp];
		}
	}
#endif
	//for (i = 0; i < (FLAME_WIDTH * FLAME_HEIGHT); i ++) {
	//	c3d_clk->flame_buf[0][i] = palit[pal_buf[0][i]];
	//}
}


/* Generates digits textures from tt fonts. */
static int	
create_digits_tex_array(c3d_clk_p c3d_clk) {
	int error = -1;
	FT_Library lib = NULL;
	FT_Face font = NULL;
	FT_GlyphSlot gliph = NULL;
	uint8_t *bitmap = NULL;
	size_t i, x, y, bm_size;

	memset(&c3d_clk->digit_desc, 0x00, sizeof(c3d_clk->digit_desc));

	if (0 != FT_Init_FreeType(&lib))
		return (-1);

	if (0 != FT_New_Face(lib, FONT_NAME, 0, &font))
		goto err_out;

	if (0 != FT_Set_Char_Size(font, (FONT_HEIGHT << 6),
	    (FONT_HEIGHT << 6), 96, 96))
		goto err_out;

	gliph = font->glyph;
	for (i = 0; i < nitems(c3d_clk->digit_desc); i ++) {
		if (0 != FT_Load_Char(font, ('0' + i), FT_LOAD_RENDER))
			goto err_out;

		c3d_clk->digit_desc[i].width = gliph->bitmap.width;
		c3d_clk->digit_desc[i].height = gliph->bitmap.rows;
		c3d_clk->digit_desc[i].top = gliph->bitmap_top;
		c3d_clk->digit_desc[i].left = gliph->bitmap_left;

		/* Two bytes for each pixel. */
		bm_size = (2 * c3d_clk->digit_desc[i].width *
		    c3d_clk->digit_desc[i].height);
		bitmap = (uint8_t*)malloc(bm_size);
		if (NULL == bitmap) {
			error = ENOMEM;
			goto err_out;
		}
		memset(bitmap, 0xff, bm_size);

		/* Filling bitmap grey&alpha bytes. */
		for (y = 0; y < c3d_clk->digit_desc[i].height; y ++) {
			for (x = 0; x < c3d_clk->digit_desc[i].width; x ++) {
				bitmap[2 * (x + y * gliph->bitmap.width) + 1] =
				    (uint8_t)(0.97f * gliph->bitmap.buffer[x + y * gliph->bitmap.width]);
			}
		}

		/* Creating symbol texture. */
		glGenTextures(1, &c3d_clk->digit_desc[i].texture);
		if (0 == c3d_clk->digit_desc[i].texture)
			goto err_out;
		glEnable(GL_TEXTURE_RECTANGLE);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		glBindTexture(GL_TEXTURE_RECTANGLE, c3d_clk->digit_desc[i].texture);
		glTexParameterf(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameterf(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexImage2D(GL_TEXTURE_RECTANGLE, 0, GL_RGBA, 
		    (GLsizei)c3d_clk->digit_desc[i].width,
		    (GLsizei)c3d_clk->digit_desc[i].height, 0,
		    GL_LUMINANCE_ALPHA,	GL_UNSIGNED_BYTE, bitmap);

		free(bitmap);
		bitmap = NULL;
	}
	error = 0;

err_out:
	if (0 != error) {
		for (i = 0; i < nitems(c3d_clk->digit_desc); i ++) {
			glDeleteTextures(1, &c3d_clk->digit_desc[i].texture);
		}
	}
	free(bitmap);
	FT_Done_Face(font);
	FT_Done_FreeType(lib);

	return (error);
}

static void
destroy_digits_tex_array(c3d_clk_p c3d_clk)
{
	for (size_t i = 0; i < nitems(c3d_clk->digit_desc); i ++) {
		glDeleteTextures(1, &c3d_clk->digit_desc[i].texture);
	}
	memset(&c3d_clk->digit_desc, 0x00, sizeof(c3d_clk->digit_desc));
}


/* Draw textured quads. */
static void
draw_time_edge_texture(c3d_clk_p c3d_clk, const uint32_t time_val,
    const GLuint tex_id) {
	size_t i;
	uint32_t x;
	uint8_t time_digits[2];
	digit_desc_p digit;
	const uint32_t y = ((BITMAP_HEIGHT - FONT_HEIGHT) / 2);
	const float border_width = 20.0f;
	const float line_width = 3.0f;
	const float cathet = 90.0f;
	const float line_const = 0.8f;

	if (99 < time_val)
		return;
	time_digits[0] = (uint8_t)(time_val / 10);
	time_digits[1] = (uint8_t)(time_val % 10);

	glViewport(0, 0, BITMAP_WIDTH, BITMAP_HEIGHT);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, BITMAP_WIDTH, 0, BITMAP_HEIGHT, 0, 20);
	gluLookAt(0, 0, 1, 0, 0, 0, 0, 1, 0);

	glDisable(GL_TEXTURE_RECTANGLE);

	glMatrixMode(GL_MODELVIEW);
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

	/* Draw gliph quads. */
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

	digit = &c3d_clk->digit_desc[time_digits[0]];
	x = ((BITMAP_WIDTH / 2) - (digit->width + (uint32_t)digit->left));
	for (i = 0; i < 2; i ++) {
		digit = &c3d_clk->digit_desc[time_digits[i]];
		glBindTexture(GL_TEXTURE_RECTANGLE, digit->texture);

		glBegin(GL_QUADS);
		{
			glTexCoord2f(digit->width, 0.0f);
			glVertex3f((x + digit->width),
			    (y + digit->height), 0.5f);

			glTexCoord2f(0.0f, 0.0f);
			glVertex3f(x, (y + digit->height), 0.5f);

			glTexCoord2f(0.0f, digit->height);
			glVertex3f(x, y, 0.5f);

			glTexCoord2f(digit->width, digit->height);
			glVertex3f((x + digit->width), y, 0.5f);
		}
		glEnd();

		x += (digit->width + (uint32_t)digit->left);
	}

	glEnable(GL_TEXTURE_RECTANGLE);
	glBindTexture(GL_TEXTURE_RECTANGLE, tex_id);
	glCopyTexImage2D(GL_TEXTURE_RECTANGLE, 0, GL_RGBA, 0, 0,
	    BITMAP_WIDTH, BITMAP_HEIGHT, 0);
}



static int
cube_init(cube_p cube, const float x, const float y, const float d_y) {

	if (NULL == cube)
		return (EINVAL);

	memset(cube, 0x00, sizeof(cube_t));
	cube->digit = (~((uint32_t)0));
	glGenTextures(1, &cube->texture);
	cube->x = x;
	cube->y = y;
	cube->d_y = d_y;
	/* Getting start random rotation angles for cubes. */
	cube->angle_x = randval(360);
	cube->angle_y = randval(60);

	return (0);
}

static void
cube_destroy(cube_p cube) {

	if (NULL == cube)
		return;
	glDeleteTextures(1, &cube->texture);
	memset(cube, 0x00, sizeof(cube_t));
}

static int
cube_update(c3d_clk_p c3d_clk, cube_p cube, const uint32_t time_val,
    const float rotation_delta) {

	if (NULL == cube)
		return (EINVAL);

	if (time_val != cube->digit) {
		cube->digit = time_val;
		draw_time_edge_texture(c3d_clk, time_val, cube->texture);
	}

	cube->angle_x += rotation_delta;
	if (cube->angle_x > 360.0f) {
		cube->angle_x = 0.0f;
	}
	cube->angle_y += cube->d_y;
	if (cube->angle_y > 60.0f || cube->angle_y < -60.0f) {
		cube->d_y = -cube->d_y;
	}

	return (0);
}

/* Redraw window callback. */
static void
redraw_window(glx_wnd_p glx_wnd __unused, const uint32_t flags,
    const wnd_state_p ws, const mcur_pos_p mcur_pos, void *udata) {
	c3d_clk_p c3d_clk = udata;
	size_t i;
	time_t rawtime;
	struct tm tminfo;
	float rotation_delta;
	uint32_t time_val[CUBES_COUNT];
	const float aspect = ((float)ws->width / (float)ws->height);
	const uint64_t cur_time_ms = get_millisec();

	/************************ GL initializing *********************/
	if (0 != (GLX_WND_REDRAW_F_INIT & flags)) {
		glEnable(GL_POLYGON_SMOOTH);
		glEnable(GL_LINE_SMOOTH);
		glShadeModel(GL_SMOOTH);
		glEnable(GL_NORMALIZE);
		glDepthFunc(GL_LEQUAL);
		glEnable(GL_MULTISAMPLE);

		glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
		glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);

		glEnable(GL_COLOR_MATERIAL);
		glEnable(GL_TEXTURE_RECTANGLE);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

		c3d_clk->mpos_x = INT32_MAX;
		c3d_clk->mpos_y = INT32_MAX;
		/* Init sphere objects. */
		c3d_clk->sphere_obj = gluNewQuadric();
		gluQuadricDrawStyle(c3d_clk->sphere_obj, GLU_FILL);
		gluQuadricNormals(c3d_clk->sphere_obj, GLU_SMOOTH);

		/* Generating textures. */
		create_digits_tex_array(c3d_clk);

		glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

		glTexParameterf(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameterf(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		glGenTextures(1, &c3d_clk->flame_tex);
		for (i = 0; i < CUBES_COUNT; i ++) {
			cube_init(&c3d_clk->cubes[i], cube_x[i], 0.0f, 0.2f);
		}

		glFlush();
	}

	/************************** Closing window ********************/
	if (0 != (GLX_WND_REDRAW_F_DESTROY & flags)) {
		for (i = 0; i < CUBES_COUNT; i ++) {
			cube_destroy(&c3d_clk->cubes[i]);
		}
		glDeleteTextures(1, &c3d_clk->flame_tex);
		destroy_digits_tex_array(c3d_clk);
		return;
	}

	/* Checking mouse cursor position and stop program if it changes. */
	if (mcur_pos->root_x > 0 &&
	    mcur_pos->root_y > 0 &&
	    c3d_clk->mpos_x == INT32_MAX &&
	    c3d_clk->mpos_y == INT32_MAX) {
		c3d_clk->mpos_x = mcur_pos->root_x;
		c3d_clk->mpos_y = mcur_pos->root_y;
	} else if ((c3d_clk->mpos_x != INT32_MAX || c3d_clk->mpos_y != INT32_MAX) &&
	    ((c3d_clk->mpos_x != mcur_pos->root_x) || (c3d_clk->mpos_y != mcur_pos->root_y))) {
		//c3d_clk->running = 0;
	}

	/************************* Render to texture ******************/
	glDisable(GL_LIGHTING);
	glEnable(GL_TEXTURE_RECTANGLE);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
	glEnable(GL_COLOR_MATERIAL);

	/* Flame updating. */
	flame_update(c3d_clk);

	/*********************** Render to screen *********************/
	/* Create framing digits on edges textures. */
	time(&rawtime);
	localtime_r(&rawtime, &tminfo);
	time_val[0] = (uint32_t)tminfo.tm_hour;
	time_val[1] = (uint32_t)tminfo.tm_min;
	time_val[2] = (uint32_t)tminfo.tm_sec;
	/* Rotations calculation. */
	rotation_delta = CUBE_ROTATION_SPEED * (float)(cur_time_ms - c3d_clk->prev_time_ms);
	c3d_clk->prev_time_ms = cur_time_ms;
	
	for (i = 0; i < CUBES_COUNT; i ++) {
		cube_update(c3d_clk, &c3d_clk->cubes[i], time_val[i],
		    rotation_delta);
	}

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(50.0, (double)aspect, 0.5, 500.0);

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
	glBindTexture(GL_TEXTURE_RECTANGLE, c3d_clk->flame_tex);
	glTexImage2D(GL_TEXTURE_RECTANGLE, 0, 3, FLAME_WIDTH, FLAME_HEIGHT,
	    0, GL_RGB, GL_UNSIGNED_BYTE, c3d_clk->flame_buf);
	glDisable(GL_LIGHTING);
	
	glColor4f(1.0f, 1.0f, 1.0f, 0.9f);
	glPushMatrix();
	{
		glTranslatef(0.0f, 0.0f, -10.0f);
		glBegin(GL_QUADS);
		{
			glNormal3f(0.0f, 0.0f, 1.0f);
			glTexCoord2f(0.0f, 0.0f);
			glVertex3f((-5.0f * aspect), -5.2f, 0.0f);
			glTexCoord2f(((FLAME_WIDTH / 2) - 1), 0.0f);
			glVertex3f((-5.0f * aspect), 4.0f, 0.0f);
			glTexCoord2f(((FLAME_WIDTH / 2) - 1), (FLAME_HEIGHT - 1));
			glVertex3f((5.0f * aspect), 4.0f, 0.0f);
			glTexCoord2f(0.0f, (FLAME_HEIGHT - 1));
			glVertex3f((5.0f * aspect), -5.2f, 0.0f);
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
	for (i = 0; i < CUBES_COUNT; i ++) {
		glPushMatrix();
		{
			glTranslatef(c3d_clk->cubes[i].x,
			    c3d_clk->cubes[i].y, range_z);
			glRotatef(c3d_clk->cubes[i].angle_y, 1.0f, 0.0f, 0.0f);
			glRotatef(c3d_clk->cubes[i].angle_x, 0.0f, 1.0f, 0.0f);
			glBindTexture(GL_TEXTURE_RECTANGLE, c3d_clk->cubes[i].texture);
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
	glBindTexture(GL_TEXTURE_RECTANGLE, c3d_clk->flame_tex);
	glColor4f(0.3f, 0.0f, 0.0f, 0.5f);
	for (i = 0; i < CUBES_COUNT; i ++) {
		glPushMatrix();
		{
			glTranslatef(c3d_clk->cubes[i].x,
			    c3d_clk->cubes[i].y, range_z);
			glRotatef(c3d_clk->cubes[i].angle_y, 1.0f, 0.0f, 0.0f);
			glRotatef(c3d_clk->cubes[i].angle_x, 0.0f, 1.0f, 0.0f);
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
			gluSphere(c3d_clk->sphere_obj, 0.1, 16, 16);
		}
		glPopMatrix();
	}

	glFlush();
}

static void
events_update(glx_wnd_p glx_wnd __unused, const XEvent *event,
    void *udata) {
	c3d_clk_p c3d_clk = udata;

	switch (event->type) {
	case ButtonPress:
	case KeyPress:
		c3d_clk->running = 0;
		break;
	}
}


int
main(int argc __unused, char **argv __unused) {
	int error;
	c3d_clk_t c3d_clk;

	memset(&c3d_clk, 0x00, sizeof(c3d_clk));
	c3d_clk.running ++;

	error = glx_wnd_create(0, 0, "cube3d clock", redraw_window,
	    events_update, &c3d_clk, &c3d_clk.glx_wnd);
	if (0 != error)
		return (error);
	glx_wnd_hide_cursor(&c3d_clk.glx_wnd);
	glx_wnd_set_window_fullscreen_popup(&c3d_clk.glx_wnd);

	while (0 == glx_wnd_update_window(&c3d_clk.glx_wnd) &&
	    0 != c3d_clk.running) {
		sleep_millisec(1);
	}

	glx_wnd_show_cursor(&c3d_clk.glx_wnd);
	glx_wnd_destroy(&c3d_clk.glx_wnd);

	return (0);
}
