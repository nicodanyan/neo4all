#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "video.h"
#include "../neo4all.h"
#include "videogl.h"
#include "sprgl.h"

// #ifndef AES

#ifdef DREAMCAST
#ifdef __cplusplus
extern "C" {
#endif
void draw_font(unsigned short *br, unsigned short *paldata, unsigned *gfxdata);
#ifdef __cplusplus
}
#endif
#else
static __inline__ void draw_font(unsigned short *br, unsigned short *paldata, unsigned *gfxdata)
{
    int y;
    for(y=0;y<8;y++) {
	register unsigned int myword = gfxdata[0];
	br[0]=paldata[(myword)&0xf];
	br[1]=paldata[(myword>>4)&0xf];
	br[2]=paldata[(myword>>8)&0xf];
	br[3]=paldata[(myword>>12)&0xf];
	br[4]=paldata[(myword>>16)&0xf];
	br[5]=paldata[(myword>>20)&0xf];
	br[6]=paldata[(myword>>24)&0xf];
	br[7]=paldata[(myword>>28)&0xf];
	
	br+=8;
	gfxdata++;
    }
}
#endif

static __inline__ void create_font(unsigned int tileno,int color,unsigned short *_br)
{
#ifdef DEBUG_GL
    tiles_fail++;
#endif
#if !defined(USE_SQ) && !defined(USE_DMA)
    register unsigned short *br=_br;
#else
    register unsigned short *br=(unsigned short*)neo4all_texture_buffer;
#endif
//printf("DRAW_TILE %i, color=0x%X\n",tileno,color);
    register unsigned short *paldata=(unsigned short *)&video_paletteram_pc[color<<4];
    register unsigned int *gfxdata = (unsigned int *)&neogeo_fix_memory[tileno<<5];

    paldata[0]=0; // BLEND !!!

    draw_font(br,paldata,gfxdata);

#ifdef DREAMCAST
#ifdef USE_DMA
    dcache_flush_range(neo4all_texture_buffer,8*8*2);
    while (!pvr_dma_ready());
    pvr_txr_load_dma(neo4all_texture_buffer,_br,8*8*2,-1,NULL,NULL);
#else
#ifdef USE_SQ
    pvr_txr_load(neo4all_texture_buffer,_br,8*8*2);
#endif
#endif
#else
#if defined(USE_SQ) || defined(USE_DMA)
    memcpy(_br,neo4all_texture_buffer,8*8*2);
#endif
#endif
}

#ifdef DREAMCAST
extern pvr_poly_cxt_t gl_poly_cxt;
static pvr_poly_hdr_t polyhdr;
static pvr_dr_state_t  dr_state;

static __inline__ void prepare_pvr_init(void)
{
	gl_poly_cxt.txr.filter= neo4all_filter;
	gl_poly_cxt.gen.alpha = PVR_ALPHA_DISABLE;
	gl_poly_cxt.txr.alpha = PVR_TXRALPHA_ENABLE;
	gl_poly_cxt.blend.src = PVR_BLEND_SRCALPHA; //PVR_BLEND_ONE;
	gl_poly_cxt.blend.dst = PVR_BLEND_INVSRCALPHA; //PVR_BLEND_ZERO;
	gl_poly_cxt.gen.culling = PVR_CULLING_NONE;
	gl_poly_cxt.txr.width = 8;
	gl_poly_cxt.txr.height = 8;
	gl_poly_cxt.txr.format = GL_ARGB1555;
}

static __inline__ void prepare_pvr_per_font(void *texture_mem)
{
	gl_poly_cxt.txr.base = texture_mem;
	pvr_poly_compile(&polyhdr, &gl_poly_cxt);
	pvr_prim(&polyhdr, sizeof(pvr_poly_hdr_t));
	pvr_dr_init(dr_state);
}

#endif

//---------------------------------------------------------------------------- 
void   video_draw_font(unsigned int code, unsigned int color, int sx, int sy)
{
//printf("font %i, sx=%i sy=%i, color=%i\n",code,sx,sy,color);
   unsigned ntile=n_tile_list+n_font_list;
   register unsigned key=((code<<16)|(video_palette_selected+color));
   void *font_buffer=fcache_hash_find(key);
   if (!font_buffer)
   {
	font_buffer=fcache_hash_insert(key);
   	if (!font_buffer)
	{
#ifdef DEBUG_GL
puts("FONT CACHE OVERRUN!!!!!!");
#endif
		return;
	}
	create_font(code,color,(unsigned short *)font_buffer);
   }

#ifdef DEBUG_GL
  ntiles++;
#endif

   tile_list[ntile].sx=sx;
   tile_list[ntile].sy=sy;
/*
   tile_list[ntile].zx=8;
   tile_list[ntile].zy=8;
   tile_list[ntile].type=3;
*/
   tile_list[ntile].buffer=font_buffer;
   n_font_list++;
}


void video_draw_fix(void)
{
	Uint16 x, y;
	Uint16 code, colour;
	Uint16 * fixarea=(Uint16 *)&video_vidram[0xe004];

	for (y=0; y < 28; y++)
	{
		for (x = 0; x < 40; x++)
		{
			code = fixarea[x << 5];
			colour = (code&0xf000)>>12;
			code  &= 0xfff;

			if (video_fix_usage[code])
				video_draw_font(code,colour,(x<<3),(y<<3));
		}
		fixarea++;
	}
}


void video_draw_font_textures(void)
{
#ifdef DEBUG_GL
	static unsigned minimo=0x12345678;
	unsigned actual=fcache_hash_unused();
	static unsigned maximo=0;
	if (n_font_list>maximo)
		maximo=n_font_list;
	if (actual<minimo)
		minimo=actual;
	printf("- %i font_textures (%i max). Cache: %i fonts unused (%i min).\n",n_font_list,maximo,actual,minimo);
#endif

   unsigned i,j;

#ifdef DREAMCAST
   prepare_pvr_init();
#endif
   for(i=n_tile_list,j=0;j<n_font_list;i++,j++)
   {
	register void *font_buffer=tile_list[i].buffer;
	register int sx=tile_list[i].sx;
	register int sy=tile_list[i].sy;
	register int zx=8;
	register int zy=8;

#ifndef DREAMCAST
//	glBindTexture(GL_TEXTURE_2D, tile_opengl_tex[i]);
	loadTextureParams();
	glTexImage2D(GL_TEXTURE_2D, 0, 4, 8, 8, 0, 
		GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, font_buffer);
	glBegin(GL_QUADS);
#else
	prepare_pvr_per_font(font_buffer);
#endif
#ifndef AES
	VERTICE_FLIP_NONE
#else
	VERTICE_FLIP_X
#endif
#ifndef DREAMCAST
	glEnd();
#endif
	tile_z+=TILE_Z_INC;
   }

#ifdef CACHE_GL_AUTOCLEAN
   if ((neo4all_glframes&127)==64)
   {
	   fcache_hash_old_cleaner(FCACHE_BREAKTIME);
#ifdef DEBUG_GL
	   unsigned ahora=fcache_hash_unused();
	   printf("  glframes=%i, FONT ELIMINADAS=%i\n",neo4all_glframes,ahora-actual);
#endif
   }
#endif
}

#ifdef MENU_ALPHA
void video_draw_font_textures_gl(void)
{
   unsigned i,j;
   extern GLint screen_texture;

   for(i=n_tile_list,j=0;j<n_font_list;i++,j++)
   {
	register void *font_buffer=tile_list[i].buffer;
	register int sx=tile_list[i].sx;
	register int sy=tile_list[i].sy;
	register int zx=8;
	register int zy=8;
	glBindTexture(GL_TEXTURE_2D, screen_texture); //tile_opengl_tex[i]);
	loadTextureParams();
#ifndef DREAMCAST
	glTexImage2D(GL_TEXTURE_2D, 0, 4, 8, 8, 0, 
		GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, font_buffer);
#else
	glKosTex2D(GL_ARGB1555,512,512,font_buffer);
#endif
	glBegin(GL_QUADS);
	GL_VERTICE_FLIP_NONE
	glEnd();
	tile_z+=TILE_Z_INC;
   }
}
#endif

/*
#else
void video_draw_fix(void){ }
void video_draw_font_textures(void){ }
#endif // AES
*/
