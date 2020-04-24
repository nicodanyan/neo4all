/*  gngeo a neogeo emulator
 *  Copyright (C) 2001 Peponas Mathieu
 * 
 *  This program is free software; you can redistribute it and/or modify  
 *  it under the terms of the GNU General Public License as published by   
 *  the Free Software Foundation; either version 2 of the License, or    
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. 
 */


#include<stdio.h>
#include<stdlib.h>

#ifdef WIN32
#define __putchar__(A) 
#define __fflush__(A)
#else
#define __putchar__(A) putchar(A)
#define __fflush__(A) fflush(A)
#endif

/* progress bar function */
static unsigned char pg_last;
static unsigned char pg_size;
static unsigned oldpos=0;

void create_progress_bar(char *desc) {
    int i;
#ifdef USE_GUI
    loading_pbar_set_label(desc);
    oldpos=0;
#else
    pg_size=62;//74-strlen(desc);
    pg_last=0;

    printf("%12s [",desc);
    for (i=0;i<pg_size;i++)
	printf("-");
    printf("]\r%12s [",desc);
    __fflush__(stdout);
#endif
}

void update_progress_bar(unsigned current_pos,unsigned size) {
#ifdef USE_GUI
    //printf("%ul %f\n",current_pos-oldpos,((current_pos-oldpos)*100.0/(float)size));
    if (((current_pos-oldpos)*100.0/(float)size)>=5.0) {
	loading_pbar_update(current_pos,size);
	oldpos=current_pos;
    }
#else
    unsigned char pg_current=(pg_size*current_pos)/(double)size;
    int i;

    
    for(i=pg_last;i<pg_current;i++) {
	__putchar__('*');
    }
    pg_last=pg_current;
    __fflush__(stdout);
#endif
}

void terminate_progress_bar(void) {
#ifdef USE_GUI
    loading_pbar_set_label(NULL);
#else
    int i;
    for(i=pg_last;i<pg_size;i++) {
	__putchar__('*');
    }
    printf("\r");
#endif
}

