/**
 * $Id$
 * Copyright (C) 2008 - 2011 Nils Asmussen
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <sys/common.h>
#include <sys/dbg/kb.h>
#include <sys/config.h>
#include <sys/util.h>
#include <sys/log.h>
#include <sys/printf.h>
#include <sys/video.h>
#include <esc/esccodes.h>
#include <string.h>
#include <stdarg.h>

#define VIDEO_BASE			0x8005000000000000
#define MAX_COLS			128
#define TAB_WIDTH			4

extern void logByte(char c);
static void vid_move(void);
static void vid_copyScrToScr(void *dst,const void *src,size_t rows);
static void vid_copyScrToMem(void *dst,const void *src,size_t rows);
static void vid_copyMemToScr(void *dst,const void *src,size_t rows);
static void vid_clear(void);
static uchar vid_handlePipePad(void);
static void vid_handleColorCode(const char **str);

static fPrintc printFunc = vid_putchar;
static ulong col = 0;
static ulong row = 0;
static uchar color = 0;
static ulong targets = TARGET_SCREEN | TARGET_LOG;

void vid_init(void) {
	vid_clearScreen();
	color = (BLACK << 4) | WHITE;
}

void vid_backup(char *buffer,ushort *r,ushort *c) {
	vid_copyScrToMem(buffer,(void*)VIDEO_BASE,VID_ROWS);
	*r = row;
	*c = col;
}

void vid_restore(const char *buffer,ushort r,ushort c) {
	vid_copyMemToScr((void*)VIDEO_BASE,buffer,VID_ROWS);
	row = r;
	col = c;
}

void vid_setTargets(uint ntargets) {
	targets = ntargets;
}

void vid_clearScreen(void) {
	vid_clear();
	col = row = 0;
}

void vid_setPrintFunc(fPrintc func) {
	printFunc = func;
}

void vid_unsetPrintFunc(void) {
	printFunc = vid_putchar;
}

void vid_putchar(char c) {
	size_t i;
	uint64_t *video;
	/* do an explicit newline if necessary */
	if(col >= VID_COLS) {
		if(conf_get(CONF_LINEBYLINE))
			kb_get(NULL,KEV_PRESS,true);
		row++;
		col = 0;
	}
	vid_move();
	video = (uint64_t*)VIDEO_BASE + row * MAX_COLS + col;

	if(c == '\n') {
		if(conf_get(CONF_LINEBYLINE))
			kb_get(NULL,KEV_PRESS,true);
		row++;
		col = 0;
	}
	else if(c == '\r')
		col = 0;
	else if(c == '\t') {
		i = TAB_WIDTH - col % TAB_WIDTH;
		while(i-- > 0)
			vid_putchar(' ');
	}
	else {
		*video = color << 8 | c;
		col++;
	}
}

void vid_printf(const char *fmt,...) {
	va_list ap;
	va_start(ap,fmt);
	vid_vprintf(fmt,ap);
	va_end(ap);
}

void vid_vprintf(const char *fmt,va_list ap) {
	sPrintEnv env;
	env.print = printFunc;
	env.escape = vid_handleColorCode;
	env.pipePad = vid_handlePipePad;
	if(targets & TARGET_SCREEN)
		prf_vprintf(&env,fmt,ap);
	if(targets & TARGET_LOG)
		log_vprintf(fmt,ap);
}

static void vid_move(void) {
	/* last line? */
	if(row >= VID_ROWS) {
		size_t x;
		/* copy all chars one line back */
		vid_copyScrToScr((void*)VIDEO_BASE,(uint64_t*)VIDEO_BASE + MAX_COLS,VID_ROWS - 1);
		/* clear last line */
		uint64_t *screen = (uint64_t*)VIDEO_BASE + MAX_COLS * (VID_ROWS - 1);
		for(x = 0; x < VID_COLS; x++)
			*screen++ = 0;
		row--;
	}
}

static void vid_copyScrToScr(void *dst,const void *src,size_t rows) {
	size_t x,y;
	uint64_t *idst = (uint64_t*)dst;
	uint64_t *isrc = (uint64_t*)src;
	for(y = 0; y < rows; y++) {
		for(x = 0; x < VID_COLS; x++)
			*idst++ = *isrc++;
		idst += MAX_COLS - VID_COLS;
		isrc += MAX_COLS - VID_COLS;
	}
}

static void vid_copyScrToMem(void *dst,const void *src,size_t rows) {
	size_t x,y;
	uint16_t *idst = (uint16_t*)dst;
	uint64_t *isrc = (uint64_t*)src;
	for(y = 0; y < rows; y++) {
		for(x = 0; x < VID_COLS; x++)
			*idst++ = *isrc++;
		isrc += MAX_COLS - VID_COLS;
	}
}

static void vid_copyMemToScr(void *dst,const void *src,size_t rows) {
	size_t x,y;
	uint64_t *idst = (uint64_t*)dst;
	uint16_t *isrc = (uint16_t*)src;
	for(y = 0; y < rows; y++) {
		for(x = 0; x < VID_COLS; x++)
			*idst++ = *isrc++;
		idst += MAX_COLS - VID_COLS;
	}
}

static void vid_clear(void) {
	size_t x,y;
	uint64_t *screen = (uint64_t*)VIDEO_BASE;
	for(y = 0; y < VID_ROWS; y++) {
		for(x = 0; x < VID_COLS; x++)
			*screen++ = 0;
		screen += MAX_COLS - VID_COLS;
	}
}

static uchar vid_handlePipePad(void) {
	return VID_COLS - col;
}

static void vid_handleColorCode(const char **str) {
	int n1,n2,n3;
	int cmd = escc_get(str,&n1,&n2,&n3);
	switch(cmd) {
		case ESCC_COLOR: {
			uchar fg = n1 == ESCC_ARG_UNUSED ? WHITE : MIN(9,n1);
			uchar bg = n2 == ESCC_ARG_UNUSED ? BLACK : MIN(9,n2);
			color = (bg << 4) | fg;
		}
		break;
		default:
			util_panic("Invalid escape-code ^[%d;%d,%d,%d]",cmd,n1,n2,n3);
			break;
	}
}
