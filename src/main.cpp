
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <libgen.h> // for dirname
#include <sys/stat.h> // for mkdir

#include <SDL.h>
#include <ctype.h>
#include <string.h>


#include "SDL_mixer.h"
extern Uint16 play_buffer[];

#include "neo4all.h"
#include "z80/z80intrf.h"

#include "console.h"
#include "menu/menu.h"


#ifdef DEBUG_FRAMESKIP
static unsigned total_frames=0;
static unsigned total_frameskip=0;
static Uint32 neo4all_start_time=0;
static Uint32 neo4all_frame_waiting=0;
static double neo4all_framerate=0.0;
#endif

#ifdef AES
#include "aes/aes.h"
#else
#include "cdpatch.h"
#endif

#ifdef AUTO_EVENTS
#if AUTO_EVENTS == 0
int trazando=1;
#else
int trazando=0;
#endif
#else
#if defined(DEBUG_Z80) || defined(DEBUG_FAME)
int trazando=1; // 0;
#endif
#endif


int neogeo_emulating=0;
int neogeo_yet_running=0;
#if defined(AUTO_EVENTS) || defined(AES_PREFETCHING)
int	neogeo_frameskip=0;
#else
int	neogeo_frameskip=-1;
#endif

/*
#ifdef AUTO_EVENTS
int	neogeo_sound_enable=0;
#else
int	neogeo_sound_enable=1;
#endif
*/
int	neogeo_sound_enable=1;

extern char neo4all_image_file[];
char neo4all_image_dir[1024];

static int neogeo_frameskip_count=0;

unsigned z80_cycles_inited=0;
#define Z80_CYCLES_INIT 90

int	neogeo_accurate=0;

static char		neocd_wm_title[255]=VERSION1;

unsigned char neogeo_game_vector[0x80];
//int neogeo_irq2start, neogeo_irq2repeat, neogeo_irq2taken, neogeo_irq2control;
int neogeo_current_line, neogeo_frame_counter_fc;

int neo4all_skip_next_frame=0;
Uint32 neo4all_z80_cycles=NEO4ALL_Z80_UNDER_CYCLES;
Uint32 neo4all_68k_cycles=NEO4ALL_68K_NORMAL_CYCLES;

//-- Global Variables --------------------------------------------------------
char			*neogeo_rom_memory = NULL;
char			*neogeo_prg_memory = NULL;
char			*neogeo_fix_memory = NULL;
char			*neogeo_spr_memory = NULL;
char			*neogeo_pcm_memory = NULL;

//char			global_error[80];
#ifndef AES
int			neogeo_prio_mode = 0;
#endif
int			neogeo_ipl_done = 0;
char			neogeo_region=REGION;
#ifndef AES
unsigned char		config_game_name[80];
#endif

static Uint32		neo4all_time; /* next time marker */


//-- 68K Core related stuff --------------------------------------------------
int				mame_debug = 0;
int				previouspc = 0;
int				ophw = 0;
int				cur_mrhard = 0;


//-- Function Prototypes -----------------------------------------------------
void	neogeo_init(void);
void	neogeo_reset(void);
void	neogeo_shutdown(void);
void	MC68000_Cause_Interrupt(int);
void	neogeo_exception(void);
void	neogeo_run(void);
void	draw_main(void);
void	neogeo_quit(void);
void	not_implemented(void);
void	neogeo_machine_settings(void);
void	neogeo_debug_mode(void);
void	neogeo_cdda_check(void);
void	neogeo_cdda_control(void);
void	neogeo_do_cdda( int command, int trck_number_bcd);
void	neogeo_read_gamename(void);

/*
static void dumps(char *st, unsigned char *mem, unsigned inicio, unsigned n)
{
	unsigned i;
	printf("\n%s(%08X,%i): ",st,inicio,n);
	for(i=inicio;i<(inicio+n);i++)
		printf("%02X ",mem[i]);
	puts("");
}
void dump_bios(unsigned inicio, unsigned n)
{
	dumps("DUMP_BIOS",(unsigned char *)aes4all_memory_bios,inicio,n);
}
void dump_cpu(unsigned inicio, unsigned n)
{
	dumps("DUMP_CPU",(unsigned char *)aes4all_memory_cpu,inicio,n);
}
void dump_gfx(unsigned inicio, unsigned n)
{
	dumps("DUMP_GFX",(unsigned char *)aes4all_memory_gfx,inicio,n);
}

static void checksums(char *st, unsigned *mem, unsigned n)
{
	unsigned i, ret=0;
	for(i=0;i<n;i++)
		ret+=*mem++;
	printf("%s=0x%X\n",st,ret);
}
void print_video_checksum(void)
{
	checksums("video_checksum",(unsigned *)video_vidram,(131072/4));
	checksums("spr_checksum",(unsigned *)aes4all_memory_gfx,(aes4all_memory_gfx_size/4));
}
void print_mamez80_checksum(void)
{
	checksums("mame_z80mem",(unsigned *)mame_z80mem,(65536/4));
}
*/



void print_frameskip(void)
{
#ifdef DEBUG_FRAMESKIP
	double p=(((double)total_frameskip)*((double)100.0))/((double)total_frames);
	printf("Total frames=%i, skipped=%i (%.2f%%)",total_frames,total_frameskip,p);
	p=(((double)neo4all_frame_waiting)*((double)100.0))/((double)total_frames);
	printf(", wait=%i (%2f%%).\n",neo4all_frame_waiting,p);
	printf("Framerate = %.4f\n",neo4all_framerate);
#endif
}

#ifdef DEBUG_FRAMESKIP
static __inline__ void calcula_framerate(void)
{
	static Uint32 start_numframes=0;
	if (!neo4all_start_time)
	{
		neo4all_start_time=SDL_GetTicks();
		start_numframes=total_frames;
	}
	else
	{
		Uint32 now=SDL_GetTicks();
		if (now-neo4all_start_time>=1000)
		{
			if (neo4all_framerate!=0.0)
				neo4all_framerate=(neo4all_framerate+((double)(total_frames-start_numframes)))/2.0;
			else
				neo4all_framerate=(double)(total_frames-start_numframes);
			neo4all_start_time=now;
			start_numframes=total_frames;
		}
	}
}
#endif


void init_autoframeskip(void)
{
	/* update neocd time */
	neo4all_time=SDL_GetTicks()+REFRESHTIME;
	neo4all_skip_next_frame=0;
#ifdef DEBUG_FRAMESKIP
	neo4all_start_time=0;
	total_frames=0;
	total_frameskip=0;
	neo4all_frame_waiting=0;
	neo4all_framerate=0.0;
#endif
#if defined (Z80_EMULATED) && !defined(AES)
	my_timer();
	z80_cycles = neo4all_z80_cycles >> 8;
	_z80raise(0);
#endif
}


static void ram_startup(void)
{
#ifdef AES
// NEOGEO AES
	bzero(aes4all_ram, 0x10000);
	bzero(aes4all_nvram, 0x10000);
#else
// NEOGEO CD
	static int passed=0;

	bzero(neogeo_prg_memory, 0x200000);
	if (!passed)
	{
		passed++;
#ifdef WIN32
		swab((const char*)startup_data, (char*)startup_data, STARTUP_DATA_LEN);
#else
		swab(startup_data, startup_data, STARTUP_DATA_LEN);
#endif
	}
	memcpy(neogeo_prg_memory + 0x10F300, (void *)&startup_data, STARTUP_DATA_LEN);
#endif
#if defined(Z80_EMULATED) && !defined(AES)
	bzero(mame_z80mem, 0x10000);
	bzero(subcpu_memspace, 0x10000);
#endif
}

int neo4all_init_memory(void)
{
	console_puts("NEOGEO: Allocating memory...");
#ifdef AES
#ifndef USE_MMAP
	static unsigned back_mem=0;
	unsigned mem;
	if (back_mem) free((void *)back_mem);
        mem = back_mem = (unsigned)calloc(aes4all_memory_total,1);
#else
	unsigned mem = (unsigned)aes4all_mmap;
#endif
	aes4all_memory = (void *)mem;
	neogeo_prg_memory = (char *)mem;
	aes4all_memory_cpu = (void *)mem;
	mem+=aes4all_memory_cpu_size;

	aes4all_memory_bios = (void *)mem;
	neogeo_rom_memory = (char *)mem;
	mem+=aes4all_memory_bios_size;

	aes4all_memory_sfix_game = (void *)mem;
	mem+=aes4all_memory_sfix_game_size;

	aes4all_memory_sfix_board = (void *)mem;
	neogeo_fix_memory = (char *)mem;
	mem+=aes4all_memory_sfix_board_size;

	aes4all_memory_sm1 = (void *)mem;
//printf("aes4all_memory_sm1=%p (%x)\n",aes4all_memory_sm1,*((unsigned char *)aes4all_memory_sm1)); exit(0);
	mem+=aes4all_memory_sm1_size;

	aes4all_memory_sound1 = (void *)mem;
	neogeo_pcm_memory = (char *)mem;
	mem+=aes4all_memory_sound1_size;

	if (aes4all_memory_sound2_size)
	{
		aes4all_memory_sound2 = (void *)mem;
		mem+=aes4all_memory_sound2_size;
	}
	else
		aes4all_memory_sound2 = aes4all_memory_sound1;

	aes4all_memory_gfx = (void *)mem;
	neogeo_spr_memory = (char *)mem;
	mem+=aes4all_memory_gfx_size;

	aes4all_memory_ng_lo = (void *)mem;
	mem+=aes4all_memory_ng_lo_size;

	aes4all_memory_fix_board_usage = (void *)mem;
	video_fix_usage=(unsigned char *)mem;
	mem+=aes4all_memory_fix_board_usage_size;

	aes4all_memory_fix_game_usage = (void *)mem;
	mem+=aes4all_memory_fix_game_usage_size;

	aes4all_memory_pen_usage = (void *)mem;

#else
	neogeo_prg_memory = (char *)calloc(1, 0x200000);
	if (neogeo_prg_memory==NULL) {
		console_puts("failed !");
		return 0;
	}

	neogeo_spr_memory = (char *)calloc(1, 0x400000);
	if (neogeo_spr_memory==NULL) {
		console_puts("failed !");
		return 0;
	}

	neogeo_rom_memory = (char *)calloc(1, 0x80000);
	if (neogeo_rom_memory==NULL) {
		console_puts("failed !");
		return 0;
	}

	neogeo_fix_memory = (char *)calloc(1, 0x20000);
	if (neogeo_fix_memory==NULL) {
		console_puts("failed !");
		return 0;
	}

	neogeo_pcm_memory = (char *)calloc(1, 0x100000);
	if (neogeo_pcm_memory==NULL) {
		console_puts("failed !");
		return 0;
	}
#endif

	// Initialize Memory Mapping
	initialize_memmap();

	// Initialize Memory Card
	memcard_init();

	return 1;
}

int neo4all_load_bios(void)
{
	char file[1024];

	sprintf(file, "%s/.neo4all", getenv("HOME"));
	mkdir(file, 0777);

#ifdef AES
// NEOGEO AES
	aes4all_prefetch_all();
#else
// NEOGEO CD

	// Load BIOS
	FILE *fp;
	do {
#ifndef RG350
		/*sprintf(file, "%s/neocd.bin", neo4all_image_dir);
		fp = fopen(file, "rb");
		if (fp) break;

		sprintf(file, "%s", neo4all_image_dir);
		dirname(file);
		sprintf(file, "%s/neocd.bin", file);
		fp = fopen(file, "rb");
		if (fp) break;

		fp = fopen(ROM_PREFIX "/neocd.bin","rb");
		if (fp) break;
                 console_println();
		console_printf("NDA ENV: %s\n", getenv("HOME"));
		sprintf(file, "%s/.neo4all/neocd.bin", getenv("HOME"));
		if (fp) break;*/

		fp = fopen("neocd.bin", "rb");
#else
		fp = fopen("/media/data/local/home/.neo4all/neocd.bin", "rb");
#endif //!RG350
		if (fp) break;
	} while(0);

	if (fp==NULL) {
#ifdef SHOW_CONSOLE
		console_println();
		console_printf("Fatal Error: Could not load NEOCD.BIN\n");
		console_wait();
#else
		init_text(0);
		drawNoBIOS();
#endif
		return 0;
	}
	fread(neogeo_rom_memory, 1, 0x80000, fp);
	fclose(fp);

	char *fixtmp=(char *)calloc(1, 65536);
	memcpy(fixtmp,&neogeo_rom_memory[458752],65536);

	fix_conv((unsigned char *)fixtmp,(unsigned char *)&neogeo_rom_memory[458752],65536,(unsigned char *)rom_fix_usage);
	/*swab(neogeo_rom_memory, neogeo_rom_memory, 131072);*/
	free(fixtmp);
	fixtmp=NULL;

	ram_startup();

	// Check BIOS validity
	if (*((short*)(neogeo_rom_memory+0xA822)) != 0x4BF9)
	{
		console_println();
		console_printf("Fatal Error: Invalid BIOS file.");
		console_wait();
		return 0;
	}

	/*** Patch BIOS load files w/ now loading message ***/
	*((short*)(neogeo_rom_memory+0x552)) = 0xFABF;
	*((short*)(neogeo_rom_memory+0x554)) = 0x4E75;
	/*** Patch BIOS load files w/out now loading ***/
	*((short*)(neogeo_rom_memory+0x564)) = 0xFAC0;
	*((short*)(neogeo_rom_memory+0x566)) = 0x4E75;
	/*** Patch BIOS CDROM Check ***/
	*((short*)(neogeo_rom_memory+0xB040)) = 0x4E71;
	*((short*)(neogeo_rom_memory+0xB042)) = 0x4E71;
	/*** Patch BIOS upload command ***/
	*((short*)(neogeo_rom_memory+0x546)) = 0xFAC1;
	*((short*)(neogeo_rom_memory+0x548)) = 0x4E75;

	/*** Patch BIOS CDDA check ***/
	*((short*)(neogeo_rom_memory+0x56A)) = 0xFAC3;
	*((short*)(neogeo_rom_memory+0x56C)) = 0x4E75;

	/*** Full reset, please ***/
	*((short*)(neogeo_rom_memory+0xA87A)) = 0x4239;
	*((short*)(neogeo_rom_memory+0xA87C)) = 0x0010;
	*((short*)(neogeo_rom_memory+0xA87E)) = 0xFDAE;

	/*** Trap exceptions ***/
	*((short*)(neogeo_rom_memory+0xA5B6)) = 0x4AFC;
#endif
	return 1;
}

static void profiler_init(void)
{
#ifdef PROFILER_NEO4ALL
	neo4all_prof_init();
	neo4all_prof_add("MAIN LOOP");		// NEO4ALL_PROFILER_MAIN
	neo4all_prof_add("M68K");		// NEO4ALL_PROFILER_M68K
	neo4all_prof_add("Z80");		// NEO4ALL_PROFILER_Z80
	neo4all_prof_add("MEMORY");		// NEO4ALL_PROFILER_MEM
	neo4all_prof_add("VECTOR");		// NEO4ALL_PROFILER_VECTOR
	neo4all_prof_add("BLITTER");		// NEO4ALL_PROFILER_BLIT
	neo4all_prof_add("SOUND");		// NEO4ALL_PROFILER_SOUND
#endif

}

//----------------------------------------------------------------------------
int	main(int argc, char* argv[])
{
if (argc > 0) {
	strncpy(neo4all_image_file,argv[1],1024);
	sprintf(neo4all_image_dir, "%s", neo4all_image_file);
	dirname(neo4all_image_dir);
}

#ifdef AES
#ifndef AES_PREFETCHING
	aes4all_prefetch_bufffer=(unsigned *)malloc(4*32768);
#endif
#endif

#ifdef STDOUTPUT
	puts("MAIN!!!");
#endif

	if((SDL_Init(SDL_INIT_VIDEO|SDL_INIT_CDROM|SDL_INIT_AUDIO|SDL_INIT_JOYSTICK)==-1)) {
		console_printf("Could not initialize SDL: %s.\n", SDL_GetError());
		return -1;
    }
	// Register exit procedure
	atexit(neogeo_shutdown);

	if (!video_init())
		return -2;

	show_icon();

	init_text(1);

#ifndef AES
	if (!neo4all_init_memory())
		return -3;
#endif

	input_init();

#ifdef Z80_EMULATED
	init_sdl_audio();
#ifndef AES
	_z80_init();
#endif
#endif

#ifndef AES
	if (!neo4all_load_bios())
		return -4;
#endif

#if defined(SHOW_MENU) && !defined(AUTO_EVENTS)
	if (argc > 0) {
		if (!neogeo_cdrom_init1())
			exit(-1);
	} else {
		do {
			run_mainMenu();
		} while(!neogeo_cdrom_init1());
	}

#else
	if (!neogeo_cdrom_init1())
		exit(-1);
#endif

#ifdef AES
	if (!neo4all_init_memory())
		return -3;

	if (!neo4all_load_bios())
		return -4;

#ifdef Z80_EMULATED
	_z80_init();
#endif
#endif

	console_pag();
	console_println();
	console_puts(VERSION1);
	console_puts(VERSION2);
	console_println();
#ifdef ENABLE_CDDA
	cdda_current_drive=neogeo_cdrom_current_drive;
	cdda_init();
#endif

	// Initialize everything
	neogeo_init();
	profiler_init();
	neogeo_run();
	neogeo_shutdown();
	return 0;
}

//----------------------------------------------------------------------------
void	neogeo_init(void)
{
	_68k_init();
	_68k_reset();
}

//----------------------------------------------------------------------------
int	neogeo_hreset(void)
{
	static int num_resets=0;

#if defined(DEBUG_Z80) || defined(DEBUG_FAME)
	if (num_resets) trazando=1;
#endif

	neogeo_emulating=0;
	console_pag();
	console_println();
	console_puts("Reset!\n");
	if (num_resets)
		sound_unmute();
	pd4990a_init();
	video_reset();
	input_reset();
#ifdef Z80_EMULATED
	_z80_init();
	_z80reset();
	streams_sh_stop();
	YM2610_sh_reset();
	AY8910_reset();
#ifndef AES
	_z80exec(100000);
	my_timer();
	_z80exec(100000);
#endif

#endif
	ram_startup();
	if (num_resets)
	{
//		SDL_Delay(333);
		sound_mute();
	}
#ifndef AES
	neogeo_ipl_done = 0;
	while(!neogeo_ipl_done)
	{
		unsigned fmax=0;
		neogeo_cdrom_load_title();
		fmax=neogeo_cdrom_process_ipl(0);
		if (!fmax)
		{
			console_println();
			console_printf("Error: Error while check IPL.TXT.\n");
			console_wait();
#ifdef SHOW_MENU
			return -1;
#else
			exit(-3);
#endif
		}
		sound_play_loading();
		if (!neogeo_cdrom_process_ipl(fmax)) {
			console_println();
			console_printf("Error: Error while processing IPL.TXT.\n");
			console_wait();
#ifdef SHOW_MENU
			return -1;
#else
			exit(-2);
#endif
		}
		else
			neogeo_ipl_done = 1;
	}

	FILE * fp;

	/* read game name */
	neogeo_read_gamename();

	/* Special patch for Samurai Spirits RPG */
	if (strcmp((const char *)config_game_name, "TEST PROGRAM USA") == 0)
	{
		strcpy((char *)config_game_name, "SAMURAI SPIRITS RPG");

		memcpy(neogeo_prg_memory + 0x132000, patch_data, PATCH_DATA_LEN);
		swab(neogeo_prg_memory + 0x132000, neogeo_prg_memory + 0x132000, 112);

	}

#if !defined(DINGUX)
	/* update window title with game name */
	strcat(neocd_wm_title," - ");
	strcat(neocd_wm_title,(const char *)config_game_name);
	SDL_WM_SetCaption(neocd_wm_title,neocd_wm_title);
#endif

	// First time init
	_68k_reset();
	_68k_set_register(_68K_REG_PC,0xc0a822);
	_68k_set_register(_68K_REG_SR,0x2700);
	_68k_set_register(_68K_REG_A7,0x10F300);
//	_68k_set_register(_68K_REG_ASP,0x10F400);

	m68k_write_memory_32(0x10F6EE, m68k_read_memory_32(0x68L)); // $68 *must* be copied at 10F6EE

	if (m68k_read_memory_8(0x107)&0x7E)
	{
		if (m68k_read_memory_16(0x13A))
		{
			m68k_write_memory_32(0x10F6EA, (m68k_read_memory_16(0x13A)<<1) + 0xE00000);
		}
		else
		{
			m68k_write_memory_32(0x10F6EA, 0);
			m68k_write_memory_8(0x00013B, 0x01);
		}
	}
	else
		m68k_write_memory_32(0x10F6EA, 0xE1FDF0);

	/* Set System Region */
	m68k_write_memory_8(0x10FD83,neogeo_region);

#ifdef ENABLE_CDDA
	cdda_current_track = 0;
	cdda_get_disk_info();
#endif
#else

	aes4all_nvram_lock=0;
	_68k_reset();

#endif
	memcpy(neogeo_game_vector,neogeo_prg_memory,0x80);
#ifdef Z80_EMULATED
	_z80_init();
	_z80reset();
//	streams_sh_start();
#ifndef AES
	_z80exec(100000);
#endif
	YM2610_sh_reset();
	AY8910_reset();
#endif
//	neogeo_irq2start=0; neogeo_irq2repeat=1000; neogeo_irq2taken=0, neogeo_irq2control=0;
	neogeo_frame_counter=0; neogeo_frame_counter_speed=4; neogeo_frame_counter_fc=0;
	neogeo_current_line=0;
#ifdef SHOW_MENU
	if (!num_resets)
        	menu_unraise();
#endif
	sound_unmute();
	init_autoframeskip();
        z80_cycles_inited=0;
	num_resets++;
	neogeo_emulating=1;
	return 0;
}

//----------------------------------------------------------------------------
void	neogeo_reset(void)
{
	_68k_reset();
#ifdef AES
	bzero(aes4all_ram, 0x10000);

// AQUI CODIGO DEL RESET AES

#else
	_68k_set_register(_68K_REG_PC,0x122);
	_68k_set_register(_68K_REG_SR,0x2700);
	_68k_set_register(_68K_REG_A7,0x10F300);
//	_68k_set_register(_68K_REG_ASP,0x10F400);

	m68k_write_memory_8(0x10FD80, 0x82);
	m68k_write_memory_8(0x10FDAF, 0x01);
	m68k_write_memory_8(0x10FEE1, 0x0A);
	m68k_write_memory_8(0x10F675, 0x01);
	m68k_write_memory_8(0x10FEBF, 0x00);
	m68k_write_memory_32(0x10FDB6, 0);
	m68k_write_memory_32(0x10FDBA, 0);

	/* System Region */
	m68k_write_memory_8(0x10FD83,neogeo_region);

#ifdef ENABLE_CDDA
	cdda_current_track = 0;
#endif
#endif
#ifdef Z80_EMULATED
	_z80reset();
#endif
}

//----------------------------------------------------------------------------
void	neogeo_shutdown(void)
{
	console_println();
	console_printf("NEOGEO: System Shutdown.\n");
	// Close everything and free memory
#ifdef AES

// AQUI EL CODIGO PARA TERMINAR AES

#else
	cdda_shutdown();
	neogeo_cdrom_shutdown();
#endif
#ifdef Z80_EMULATED
	sound_shutdown();
#endif
	input_shutdown();
	video_shutdown();
	memcard_shutdown();

#ifndef AES
	if (neogeo_prg_memory)
		free(neogeo_prg_memory);
	neogeo_prg_memory=NULL;
	if (neogeo_rom_memory)
		free(neogeo_rom_memory);
	neogeo_rom_memory=NULL;
	if (neogeo_spr_memory)
		free(neogeo_spr_memory);
	neogeo_spr_memory=NULL;
	if (neogeo_fix_memory)
		free(neogeo_fix_memory);
	neogeo_fix_memory=NULL;
	if (neogeo_pcm_memory)
		free(neogeo_pcm_memory);
	neogeo_pcm_memory=NULL;
#else

// AQUI PARA LIBERAR MEMORIA CON AES

#endif

	SDL_Quit();

	return;
}

//----------------------------------------------------------------------------
void	neogeo_exception(void)
{
	console_printf("NEOGEO: Exception Trapped at %08x !\n", previouspc);
	exit(0);
}

//----------------------------------------------------------------------------
/*
void MC68000_Cause_Interrupt(int level)
{
	_68k_raise_interrupt(level,M68K_AUTOVECTORED_INT);
	_68k_emulate(10000);
	_68k_lower_interrupt(level);
}
*/

//----------------------------------------------------------------------------
void	neogeo_exit(void)
{
	console_puts("NEOGEO: Exit requested by software...");
	exit(0);
}

void neogeo_adjust_frameskip(int new_frameskip)
{
	neogeo_frameskip=new_frameskip;
	neogeo_frameskip_count=0;
	init_autoframeskip();
}

#ifdef SHOW_MENU
void neogeo_adjust_cycles(int new_68k, int new_z80)
{
	switch(new_68k)
	{
		case 0:
			neo4all_68k_cycles=NEO4ALL_68K_UNDER_CYCLES;
			break;
		case 2:
			neo4all_68k_cycles=NEO4ALL_68K_OVER_CYCLES;
			break;
		default:
			neo4all_68k_cycles=NEO4ALL_68K_NORMAL_CYCLES;
	}
	switch(new_z80)
	{
		case 0:
			neo4all_z80_cycles=NEO4ALL_Z80_UNDER_CYCLES;
			break;
		case 2:
			neo4all_z80_cycles=NEO4ALL_Z80_OVER_CYCLES;
			break;
		default:
			neo4all_z80_cycles=NEO4ALL_Z80_NORMAL_CYCLES;
	}
	adjust_timer(neo4all_z80_cycles);
}

#else

static char *_autodir_="CHANGE IT:" ROM_PREFIX  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0";

char *autodir=(char *)&_autodir_[10];

void neogeo_adjust_fine_cycles(int new_68k, int new_z80)
{
	if (new_68k<(NEO4ALL_68K_UNDER_CYCLES/2))
		new_68k=(NEO4ALL_68K_UNDER_CYCLES/2);
	else
	if (new_68k>(NEO4ALL_68K_OVER_CYCLES*2))
		new_68k=(NEO4ALL_68K_OVER_CYCLES*2);

	if (new_z80<(NEO4ALL_Z80_UNDER_CYCLES/2))
		new_z80=(NEO4ALL_Z80_UNDER_CYCLES/2);
	else
	if (new_z80>(NEO4ALL_Z80_OVER_CYCLES*2))
		new_z80=(NEO4ALL_Z80_OVER_CYCLES*2);

	neo4all_68k_cycles=new_68k;
	neo4all_z80_cycles=new_z80;

	adjust_timer(neo4all_z80_cycles);
}
#endif


//----------------------------------------------------------------------------
void	neogeo_run(void)
{
	console_puts("NEOGEO RUN ...");

	// If IPL.TXT not loaded, load it !
#ifndef AES
	if (!neogeo_ipl_done)
		if (neogeo_hreset()) {
			drawNoNeoGeoCD();
			exit(1);
		}
#else
	neogeo_hreset();
#endif

	/* get time for speed throttle */
	init_autoframeskip();


	neo4all_prof_start(NEO4ALL_PROFILER_MAIN);
	// Main loop
	console_puts("Lets go ...");
	neogeo_yet_running=1;
	while(1)
	{
		neo4all_prof_end(NEO4ALL_PROFILER_MAIN);
#ifdef Z80_EMULATED
		if ((!neogeo_accurate)||(!neogeo_sound_enable))
#endif
		{
			neo4all_prof_start(NEO4ALL_PROFILER_MAIN);
#ifdef Z80_EMULATED
			neo4all_prof_start(NEO4ALL_PROFILER_Z80);
			if (neogeo_sound_enable)
			{
				unsigned my_z80_cycles;
				if (z80_cycles_inited<Z80_CYCLES_INIT)
				{
					z80_cycles_inited++;
					my_z80_cycles=NEO4ALL_Z80_OVER_CYCLES;
				}
				else
					my_z80_cycles=neo4all_z80_cycles;
				// Execute Z80 timeslice (one VBL)
				_z80raise(0);
				{
    				register int	i;
				register int zc=neo4all_z80_cycles/NEOGEO_NB_INTERLACE;
				for (i = 0; i < NEOGEO_NB_INTERLACE; i++) {
					_z80exec(zc);
					my_timer();
				}
				}
			}
			neo4all_prof_end(NEO4ALL_PROFILER_Z80);
#endif
			neo4all_prof_start(NEO4ALL_PROFILER_M68K);
			// One-vbl timeslice
			_68k_emulate(neo4all_68k_cycles);
#ifdef AES
			_68k_interrupt(1);
#else
			_68k_interrupt(2);
#endif
			neo4all_prof_end(NEO4ALL_PROFILER_M68K);
		}
#ifdef Z80_EMULATED
		else
		{
			_z80raise(0);
			register int i;
			register int zc=neo4all_z80_cycles/NEOGEO_NB_INTERLACE;
			register int mc=neo4all_68k_cycles/NEOGEO_NB_INTERLACE;
			for (i = 0; i < NEOGEO_NB_INTERLACE ; i++)
			{
				_68k_emulate(mc);
				_z80exec(zc);
				my_timer();
#ifdef AES
//				_68k_interrupt(2);
#endif
				neogeo_current_line++;
			}
#ifdef AES
			_68k_interrupt(1);
#else
			_68k_interrupt(2);
#endif
			neogeo_current_line=0;
		}
#endif

		// update pd4990a
		pd4990a_addretrace();

		// check the watchdog
		if (watchdog_counter > 0) {
		    if (--watchdog_counter == 0) {
			//logerror("reset caused by the watchdog\n");
			neogeo_reset();
		    }
		}

		memcard_update();

#ifdef DEBUG_FRAMESKIP
		total_frames++;
		total_frameskip+=neo4all_skip_next_frame;
		calcula_framerate();
#endif

#ifndef AUTO_EVENTS
		neo4all_prof_start(NEO4ALL_PROFILER_BLIT);
	// Call display routine
#ifndef AES
		if (neogeo_prio_mode)
			video_draw_screen2();
		else
#endif
			video_draw_screen1();
		neo4all_prof_end(NEO4ALL_PROFILER_BLIT);
#endif

#ifndef AES
	// Check if there are pending commands for CDDA
		neogeo_cdda_check();
		cdda_loop_check();
#else
#endif

	// Update keys and Joystick
		processEvents();

		if (!Mix_Playing(1)) {
			neo4all_prof_start(NEO4ALL_PROFILER_SOUND);
			streamupdate(1024*4);
			neo4all_prof_end(NEO4ALL_PROFILER_SOUND);

			Mix_Chunk *chunk;
			chunk = (Mix_Chunk *)malloc(sizeof(Mix_Chunk));
			chunk->allocated = 0;
			chunk->alen = 1024*4;
			chunk->abuf = (Uint8*)play_buffer;
			chunk->volume = 80;
			Mix_PlayChannel(1, chunk, 0);
		}

	// Frameskip
		switch(neogeo_frameskip)
		{
			case 0:
				neo4all_skip_next_frame=0;
				break;
			case 1:
				if (!(neogeo_frameskip_count&0x1))
					neo4all_skip_next_frame=0;
				else
					neo4all_skip_next_frame=1;
				break;
			case 2:
				if (!(neogeo_frameskip_count%3))
					neo4all_skip_next_frame=0;
				else
					neo4all_skip_next_frame=1;
				break;
			case 3:
				if (!(neogeo_frameskip_count&0x3))
					neo4all_skip_next_frame=0;
				else
					neo4all_skip_next_frame=1;
				break;
			case 4:
				if (!(neogeo_frameskip_count%5))
					neo4all_skip_next_frame=0;
				else
					neo4all_skip_next_frame=1;
				break;
			case 5:
				if (!(neogeo_frameskip_count%6))
					neo4all_skip_next_frame=0;
				else
					neo4all_skip_next_frame=1;
				break;
			default:
			{
				static int cuantos=0;
				unsigned ahora=SDL_GetTicks();
				neo4all_time+=REFRESHTIME;

				if ((ahora-(REFRESHTIME/4))>neo4all_time)
				{
					cuantos++;
					if (cuantos>2) //5)
					{
						neo4all_time=ahora+2;
						neo4all_skip_next_frame=0;
						cuantos=1;
					}
					else
						neo4all_skip_next_frame=1;
				}
				else
				{
					if ((!cuantos) && (ahora+((3*REFRESHTIME)/4)<neo4all_time))
					{
#ifdef DEBUG_FRAMESKIP
						neo4all_frame_waiting++;
#endif
						unsigned to_wait=1+((neo4all_time-ahora)>>1);
						if (to_wait<50)
							SDL_Delay(to_wait);
					}
					neo4all_skip_next_frame=0;
					cuantos=0;
				}
			}
		}

		neogeo_frameskip_count++;
	}
	// Stop CDDA
	cdda_stop();

	return;
}

#ifndef AES
//----------------------------------------------------------------------------
// This is a really dirty hack to make SAMURAI SPIRITS RPG work
void	neogeo_prio_switch(void)
{
	if (_68k_get_register(_68K_REG_D7) == 0xFFFF)
		return;

	if (_68k_get_register(_68K_REG_D7) == 9 &&
	    _68k_get_register(_68K_REG_A3) == 0x10DED9 &&
		(_68k_get_register(_68K_REG_A2) == 0x1081d0 ||
		(_68k_get_register(_68K_REG_A2)&0xFFF000) == 0x102000)) {
		neogeo_prio_mode = 0;
		return;
	}

	if (_68k_get_register(_68K_REG_D7) == 8 &&
	    _68k_get_register(_68K_REG_A3) == 0x10DEC7 &&
		_68k_get_register(_68K_REG_A2) == 0x102900) {
		neogeo_prio_mode = 0;
		return;
	}

	if (_68k_get_register(_68K_REG_A7) == 0x10F29C)
	{
		if ((_68k_get_register(_68K_REG_D4)&0x4010) == 0x4010)
		{
			neogeo_prio_mode = 0;
			return;
		}

		neogeo_prio_mode = 1;
	}
	else
	{
		if (_68k_get_register(_68K_REG_A3) == 0x5140)
		{
			neogeo_prio_mode = 1;
			return;
		}

		if ( (_68k_get_register(_68K_REG_A3)&~0xF) == (_68k_get_register(_68K_REG_A4)&~0xF) )
			neogeo_prio_mode = 1;
		else
			neogeo_prio_mode = 0;
	}
}

//----------------------------------------------------------------------------
void	not_implemented(void)
{
		console_printf("Error: This function isn't implemented.");
}
#endif

//----------------------------------------------------------------------------
void	neogeo_quit(void)
{
		exit(0);
}

#ifndef AES
//----------------------------------------------------------------------------
void neogeo_cdda_check(void)
{
#ifdef ENABLE_CDDA
	int		Offset;

	Offset = m68k_read_memory_32(0x10F6EA);
	if (Offset < 0xE00000)	// Invalid addr
		return;

	Offset -= 0xE00000;
	Offset >>= 1;

#ifdef Z80_EMULATED
	if (neogeo_sound_enable)
		neogeo_do_cdda(mame_z80mem[Offset&0xFFFF], mame_z80mem[(Offset+1)&0xFFFF]);

#endif
#endif
}


//----------------------------------------------------------------------------
void neogeo_cdda_control(void)
{
#ifdef ENABLE_CDDA
	neogeo_do_cdda( (_68k_get_register(_68K_REG_D0)>>8)&0xFF,
	                 _68k_get_register(_68K_REG_D0)&0xFF );
#endif
}

//----------------------------------------------------------------------------
void neogeo_do_cdda( int command, int track_number_bcd)
{
#ifdef ENABLE_CDDA
	int		track_number;
	int		offset;

	if ((command == 0)&&(track_number_bcd == 0))
		return;

	m68k_write_memory_8(0x10F64B, track_number_bcd);
	m68k_write_memory_8(0x10F6F8, track_number_bcd);
	m68k_write_memory_8(0x10F6F7, command);
	m68k_write_memory_8(0x10F6F6, command);

	offset = m68k_read_memory_32(0x10F6EA);

	if (offset)
	{
		offset -= 0xE00000;
		offset >>= 1;

		m68k_write_memory_8(0x10F678, 1);

#ifdef Z80_EMULATED
		if (neogeo_sound_enable)
		{
			mame_z80mem[offset&0xFFFF] = 0;
			mame_z80mem[(offset+1)&0xFFFF] = 0;
		}
#endif
	}

	switch( command )
	{
		case	0:
		case	1:
		case	5:
		case	4:
		case	3:
		case	7:
			track_number = ((track_number_bcd>>4)*10) + (track_number_bcd&0x0F);
			if ((track_number == 0)&&(!cdda_playing))
			{
				//sound_mute();
				cdda_resume();
			}
			else if ((track_number>1)&&(track_number<99))
			{
				//sound_mute();
				cdda_play(track_number);
				cdda_autoloop = !(command&1);
			}
			break;
		case	6:
		case	2:
			if (cdda_playing)
			{
				//sound_mute();
				cdda_pause();
			}
			break;
	}
#endif
}
#endif // AES



//----------------------------------------------------------------------------
void neogeo_read_gamename(void)
{
#ifndef AES
	unsigned char	*Ptr;
	int				temp;

	Ptr = (unsigned char *)(neogeo_prg_memory + m68k_read_memory_32(0x11A));
#ifdef WIN32
	swab((const char *)Ptr, (char *)config_game_name, 80);
#else
	swab(Ptr, config_game_name, 80);
#endif

	for(temp=0;temp<80;temp++) {
		if (!isprint(config_game_name[temp])) {
			config_game_name[temp]=0;
			break;
		}
	}
#endif
}

