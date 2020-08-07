/**************************************
****   INPUT.C  -  Input devices   ****
**************************************/

/*-- Include Files ---------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <SDL.h>

#include "../neo4all.h"

#include "../menu/menu.h"

extern int menu_moving;


#ifdef INPUT_INLINE
#define INPUT_STATIC_INLINE static __inline__
#else
#define INPUT_STATIC_INLINE static
#endif

/* Joystick definitions */
#define NUMJOYSTICKS    2
#define NUMAXES         2
#define AXISMIN         0

#define AXISMAX         65536
#define NUMJOYBUTTONS   10

#define AXISCENTRE      AXISMAX / 2
#define AXISTHRESHOLD   AXISCENTRE / 2   /* 2% joystick threshold */

/*--------------------------------------------------------------------------*/
#define P1UP    0x00000001
#define P1DOWN  0x00000002
#define P1LEFT  0x00000004
#define P1RIGHT 0x00000008
#define P1A     0x00000010
#define P1B     0x00000020
#define P1C     0x00000040
#define P1D     0x00000080

#define P2UP    0x00000100
#define P2DOWN  0x00000200
#define P2LEFT  0x00000400
#define P2RIGHT 0x00000800
#define P2A     0x00001000
#define P2B     0x00002000
#define P2C     0x00004000
#define P2D     0x00008000

#define P1START 0x00010000
#define P1SEL   0x00020000
#define P2START 0x00040000
#define P2SEL   0x00080000

#define SPECIAL 0x01000000

/*--------------------------------------------------------------------------*/
Uint32 keys   =~0;
Uint32 keyup  [SDLK_LAST];
Uint32 keydown[SDLK_LAST];

SDL_Joystick *joystick[NUMJOYSTICKS];
Uint32 joydown[NUMJOYSTICKS][NUMJOYBUTTONS];
Uint32 joyup  [NUMJOYSTICKS][NUMJOYBUTTONS];

Uint32 joymask[NUMJOYSTICKS][NUMAXES][AXISMAX];
Uint32 joyset [NUMJOYSTICKS][NUMAXES][AXISMAX];

unsigned char neo4all_intern_coin=7;

static int input_initted=0;

extern SDLKey menuControl_bt0[];
extern SDLKey menuControl_bt1[];


#ifdef AES
static int input_back_aes_system=-1234;
void input_init_aes_system(int system)
{

	if (system)
	{
    		keyup[menuControl_bt0[5]]=joyup[0][9]=P1SEL|keyup[menuControl_bt0[5]];
		keyup[menuControl_bt1[5]]=joyup[1][9]=P2SEL|keyup[menuControl_bt1[5]];
	}
	else
	{
		keyup[menuControl_bt0[5]]=joyup[0][9]=SPECIAL|keyup[menuControl_bt0[5]];
		keyup[menuControl_bt1[5]]=joyup[1][9]=SPECIAL|keyup[menuControl_bt1[5]];
	}
	joydown[0][9]=~joyup[0][9];
	joydown[1][9]=~joyup[1][9];
	input_back_aes_system=system;
}

static void input_reinit_aes_system(void)
{
	if (input_back_aes_system!=1234)
		input_init_aes_system(input_back_aes_system);
}
#else
#define input_init_aes_system(SYS)
#define input_reinit_aes_system()
#endif

#ifdef DINGOO
static int pulsando_escape=0;

static void show_mhz(void)
{
    extern int dingoo_get_clock(void);
    char n[40];
    sprintf((char *)&n[0],"Dingoo at %iMHz",dingoo_get_clock());
    set_message((char *)&n[0], 50);
}

static void show_brightness(void)
{
    extern int dingoo_get_brightness(void);
    char n[40];
    sprintf((char *)&n[0],"Brightness %i%%",dingoo_get_brightness());
    set_message((char *)&n[0], 40);
}

static void show_volumen(void)
{
    extern int dingoo_get_volumen(void);
    char n[40];
    sprintf((char *)&n[0],"Volumen %i%%",dingoo_get_volumen());
    set_message((char *)&n[0], 40);
}

static void inc_dingoo_mhz(void)
{
	extern void dingoo_set_clock(unsigned int);
	extern unsigned int dingoo_get_clock(void);
	dingoo_set_clock(dingoo_get_clock()+25);
	show_mhz();
}

static void dec_dingoo_mhz(void)
{
	extern void dingoo_set_clock(unsigned int);
	extern unsigned int dingoo_get_clock(void);
	dingoo_set_clock(dingoo_get_clock()-25);
	show_mhz();
}

static void inc_dingoo_brightness(void)
{
	extern void dingoo_set_brightness(int);
	extern int dingoo_get_brightness(void);
	dingoo_set_brightness(dingoo_get_brightness()+5);
	show_brightness();
}

static void dec_dingoo_brightness(void)
{
	extern void dingoo_set_brightness(int);
	extern int dingoo_get_brightness(void);
	dingoo_set_brightness(dingoo_get_brightness()-5);
	show_brightness();
}

static void inc_dingoo_volumen(void)
{
	extern void dingoo_set_volumen(int);
	extern int dingoo_get_volumen(void);
	dingoo_set_volumen(dingoo_get_volumen()+5);
	show_volumen();
}

static void dec_dingoo_volumen(void)
{
	extern void dingoo_set_volumen(int);
	extern int dingoo_get_volumen(void);
	dingoo_set_volumen(dingoo_get_volumen()-5);
	show_volumen();
}

#endif

/*--------------------------------------------------------------------------*/
void input_init(void) {
    int i;

    SDL_EventState(SDL_MOUSEMOTION, SDL_IGNORE);

    for(i=0;i<SDLK_LAST;i++)
	keyup[i]=0;

    for(i=0; i<NUMJOYBUTTONS; i++)
        joyup[0][i] = joyup[1][i] = 0;

    /* Player 1 */
    keyup[SDLK_UP]    				= P1UP;
    keyup[SDLK_DOWN]  				= P1DOWN;
    keyup[SDLK_LEFT]  				= P1LEFT;
    keyup[SDLK_RIGHT] 				= P1RIGHT;
    keyup[menuControl_bt0[0]] = joyup[0][0]	= P1A | keyup[menuControl_bt0[0]];
    keyup[menuControl_bt0[1]] = joyup[0][1]	= P1B | keyup[menuControl_bt0[1]];
    keyup[menuControl_bt0[2]] = joyup[0][2]	= P1C | keyup[menuControl_bt0[2]];
    keyup[menuControl_bt0[3]] = joyup[0][3]	= P1D | keyup[menuControl_bt0[3]];
    keyup[menuControl_bt0[4]] = joyup[0][8]	= P1START | keyup[menuControl_bt0[4]];
    keyup[menuControl_bt0[5]] = joyup[0][9]	= P1SEL | keyup[menuControl_bt0[5]];


    /* Player 2 */
    keyup[SDLK_w]    				= P2UP;
    keyup[SDLK_s]  	  			= P2DOWN;
    keyup[SDLK_d]    				= P2RIGHT;
    keyup[SDLK_a]    				= P2LEFT;
    keyup[menuControl_bt1[0]] = joyup[1][0]	= P2A | keyup[menuControl_bt1[0]];
    keyup[menuControl_bt1[1]] = joyup[1][1]	= P2B | keyup[menuControl_bt1[1]];
    keyup[menuControl_bt1[2]] = joyup[1][2]	= P2C | keyup[menuControl_bt1[2]];
    keyup[menuControl_bt1[3]] = joyup[1][3]	= P2D | keyup[menuControl_bt1[3]];
    keyup[menuControl_bt1[4]] = joyup[1][8]	= P2START | keyup[menuControl_bt1[4]];
    keyup[menuControl_bt1[5]] = joyup[1][9]	= P2SEL | keyup[menuControl_bt1[5]];

    /* Special */
    keyup[SDLK_F1]    = SPECIAL;
    keyup[SDLK_F2]    = SPECIAL;
    keyup[SDLK_F3]    = SPECIAL;
    keyup[SDLK_F4]    = SPECIAL;
    keyup[SDLK_F12]   = SPECIAL;
    keyup[SDLK_ESCAPE]= SPECIAL;
    keyup[SDLK_BACKSPACE]= SPECIAL;

    input_reinit_aes_system();

    /* set key down mask */
    for(i=0;i<SDLK_LAST;i++) {
        keydown[i]=~keyup[i];
    }

    /* set joy button down mask */
    for(i=0; i<NUMJOYBUTTONS; i++) {
        joydown[0][i]=~joyup[0][i];
        joydown[1][i]=~joyup[1][i];
    }

    /* configure joystick axes */
    /* left and up */
    for(i=AXISMIN; i<AXISCENTRE-AXISTHRESHOLD; i++) {
        joymask[0][0][i]=~P1LEFT;
        joymask[1][0][i]=~P2LEFT;
        joymask[0][1][i]=~P1UP;
        joymask[1][1][i]=~P2UP;
        joyset[0][0][i]=P1RIGHT;
        joyset[1][0][i]=P2RIGHT;
        joyset[0][1][i]=P1DOWN;
        joyset[1][1][i]=P2DOWN;
    }

    /* centre */
    for(i=AXISCENTRE-AXISTHRESHOLD; i<AXISCENTRE+AXISTHRESHOLD; i++) {
        joymask[0][0][i]=~0;
        joymask[1][0][i]=~0;
        joymask[0][1][i]=~0;
        joymask[1][1][i]=~0;
        joyset[0][0][i]=P1RIGHT|P1LEFT;
        joyset[1][0][i]=P2RIGHT|P2LEFT;
        joyset[0][1][i]=P1DOWN|P1UP;
        joyset[1][1][i]=P2DOWN|P2UP;
    }

    /* right and down */
    for(i=AXISCENTRE+AXISTHRESHOLD; i<AXISMAX; i++) {
        joymask[0][0][i]=~P1RIGHT;
        joymask[1][0][i]=~P2RIGHT;
        joymask[0][1][i]=~P1DOWN;
        joymask[1][1][i]=~P2DOWN;
        joyset[0][0][i]=P1LEFT;
        joyset[1][0][i]=P2LEFT;
        joyset[0][1][i]=P1UP;
        joyset[1][1][i]=P2UP;
    }

    SDL_JoystickEventState(SDL_ENABLE);
    /* open joysticks */
    for(i=0; i<SDL_NumJoysticks() && i<NUMJOYSTICKS; i++) {
        joystick[i]=SDL_JoystickOpen(i);
/*
        if(joystick[i]!=NULL) {
#ifndef DREAMCAST
            printf("\nOpened Joystick %d\n",i);
            printf("Name: %s\n", SDL_JoystickName(i));
            printf("Number of Axes: %d\n", SDL_JoystickNumAxes(joystick[i]));
            printf("Number of Buttons: %d\n", SDL_JoystickNumButtons(joystick[i]));
#endif
        }
*/
    }

    input_reset();
    input_initted=1;
#if defined(DINGOO) && !defined(RETROFW)
    static int yet=0;
    if (!yet) {
	    show_mhz();
	    yet++;
    }
#endif
}

void input_reinit(void)
{
	if (input_initted)
		input_init();
}


void input_shutdown(void) {
    int i;

    /* Close joysticks */
    for(i=0; i<NUMJOYSTICKS; i++) {
        if(joystick[i]!=NULL) {
            SDL_JoystickClose(joystick[i]);
            joystick[i]=NULL;
        }
    }
}

static int pulsando_menu=0;

INPUT_STATIC_INLINE void goMenu(void)
{
#ifndef SHOW_MENU
	SDL_Event ev;
	sound_disable();
	cdda_pause();
	input_reset();
	do{
		text_draw_nomenu();
    	}while(!SDL_PollEvent(&ev));
	while(SDL_PollEvent(&ev)) SDL_Delay(10);
	cdda_resume();
	sound_enable();
	init_autoframeskip();
	pulsando_menu=0;
#ifdef DINGOO
	pulsando_escape=0;
#endif
#else
	if (keys==~0)
	{
		int nook=1;
		pulsando_menu=0;
#ifdef DINGOO
		pulsando_escape=0;
#endif
#ifdef PROFILER_NEO4ALL
		neo4all_prof_show();
#endif
		print_frameskip();
#ifdef SHOW_MENU
		sound_disable();
		cdda_pause();
		input_reset();
		menu_moving=1;
		menu_raise();
		while(nook)
		{
			int cas=run_mainMenu();
			if (cas>=2)
			{
#ifndef AES
				cdda_resume();
				SDL_Delay(100);
				cdda_stop();
#ifdef CDISO
				if (cas==2)
					neogeo_cdrom_init1();
#endif
#else
				if (cas==2)
				{
					aes4all_load();
					neo4all_init_memory();
					neo4all_load_bios();
				}
#endif
				nook=neogeo_hreset();
#ifndef AES
				if (nook)
					drawNoNeoGeoCD();
#endif
			}
			else
			{
				nook=0;
				cdda_resume();
			}
		}
		menu_unraise();
		sound_enable();
#endif
		input_reset();
		init_autoframeskip();
#ifdef PROFILER_NEO4ALL
		neo4all_prof_init();
#endif
	}
#endif
}

static void insert_coin(int n)
{
	if (!n)
		neo4all_intern_coin&=0x6;
	else
		neo4all_intern_coin&=0x5;
}

static void update_coin(void)
{
	static unsigned f=0;
	if (!(f&127))
		neo4all_intern_coin=0x7;
	f++;
}

INPUT_STATIC_INLINE void specialKey (SDLKey key) {
    switch(key) {
	case SDLK_F10:
    case SDLK_END:
	case SDLK_HOME:
		pulsando_menu=1;
		break;
#ifndef DINGOO
	case SDLK_F1: video_fullscreen_toggle(); break;
	case SDLK_F2: insert_coin(0); break;
	case SDLK_F3: insert_coin(1); break;
    case SDLK_F4: sound_toggle(); break;
    case SDLK_F12: video_save_snapshot(); break;
    // case SDLK_ESCAPE: exit(0); break;
#else
	case SDLK_TAB: insert_coin(0); break;
	case SDLK_ESCAPE: pulsando_escape=1; break;
#endif
        default:
		pulsando_menu=0;
    }
}

INPUT_STATIC_INLINE void keyDown (SDLKey key) {
    if(keyup[key]&SPECIAL) {
        specialKey(key);
    } else {
    pulsando_menu=0;

#ifdef RETROFW
    switch(key) {
        case SDLK_END:
        case SDLK_HOME:
            pulsando_menu=1;
            break;
    }
#endif // RETROFW

#ifdef DINGOO
	if (pulsando_escape) {
    		switch(key) {
#ifndef RETROFW
			case SDLK_RIGHT: inc_dingoo_volumen(); break;
			case SDLK_LEFT: dec_dingoo_volumen(); break;
			case SDLK_UP: inc_dingoo_brightness(); break;
			case SDLK_DOWN: dec_dingoo_brightness(); break;
			case SDLK_LCTRL: inc_dingoo_mhz(); break;
			case SDLK_LALT: dec_dingoo_mhz(); break;
#endif // !RETROFW
			case SDLK_RETURN: pulsando_menu=1; break;
		}
	} else
#endif
        keys &= keydown[key];
    }
}


INPUT_STATIC_INLINE void keyUp (SDLKey key) {
    	if(!(keyup[key]&SPECIAL)) {

#ifdef DINGOO
	if (!pulsando_escape)
#endif
    	keys |= keyup[key];
    }
#ifdef DINGOO
    else
	    if (key==SDLK_ESCAPE) pulsando_escape=0;
#endif
    pulsando_menu=0;
}

INPUT_STATIC_INLINE void joyDown (int which, int button) {
    if (which<NUMJOYSTICKS && button<NUMJOYBUTTONS) {
        keys &= joydown[which][button];
    }
}

INPUT_STATIC_INLINE void joyUp (int which, int button) {
    if (which<NUMJOYSTICKS && button<NUMJOYBUTTONS) {
        keys |= joyup[which][button];
    }
}


INPUT_STATIC_INLINE void joyMotion (int which, int axis, int value) {
    value+=AXISCENTRE;
    if (which<NUMJOYSTICKS && axis <NUMAXES && value<AXISMAX) {
        keys &= joymask[which][axis][value];
        keys |= joyset[which][axis][value];
    }
}


void processEvents(void) {
#ifndef AUTO_EVENTS
    SDL_Event event;

    update_coin();
    while(SDL_PollEvent(&event)) {
        switch(event.type) {
            case SDL_KEYDOWN: 
#ifdef RG350
                // If Power button is pressing we launch the menu
                if (event.key.keysym.sym==SDLK_HOME) {
                    pulsando_menu = 1;
                    pulsando_escape = 1;
                    goMenu();
                    } else {
                        keyDown(event.key.keysym.sym);
                }
#else
                keyDown(event.key.keysym.sym);
#endif //RG350
                
                break;
            case SDL_KEYUP: keyUp(event.key.keysym.sym); break;
            case SDL_JOYBUTTONDOWN: joyDown(event.jbutton.which, event.jbutton.button); break;
            case SDL_JOYBUTTONUP:   joyUp(event.jbutton.which, event.jbutton.button); break;
            case SDL_JOYAXISMOTION: joyMotion(event.jaxis.which, event.jaxis.axis, event.jaxis.value);
				    break;
            case SDL_QUIT:    exit(0); break;

            default:
                break;
        }
    }
    if (pulsando_menu)
    {
#ifndef DINGOO
    	if (pulsando_menu<15)
		pulsando_menu++;
	else
#endif
		goMenu();
    }
#else
    static unsigned cuenta=0;
    unsigned rango=cuenta&127;

    if (rango<64)
	    keys |= P1A;
    else
	    keys &= ~P1A;

    if (cuenta==AUTO_EVENTS)
    {
	    extern int trazando;
	    trazando=1;
#ifdef PROFILER_NEO4ALL
	    neo4all_prof_init();
#else
#if !defined(DEBUG_Z80) && !defined(DEBUG_FAME)
	    init_autoframeskip();
#endif
#endif
    }
#ifdef AUTO_MAX_EVENTS
    if (cuenta>=AUTO_MAX_EVENTS)
    {
#ifdef PROFILER_NEO4ALL
	    neo4all_prof_show();
#else
#if !defined(DEBUG_Z80) && !defined(DEBUG_FAME)
	    print_frameskip();
	    fflush(stdout);
#endif
#endif
	    exit(0);
    }
#endif
    cuenta++;
#endif
}

/*--------------------------------------------------------------------------*/
unsigned char read_player1(void) {
     return keys&0xff;
}

/*--------------------------------------------------------------------------*/
unsigned char read_player2(void)
{
    return (keys>>8)&0xff;
}

/*--------------------------------------------------------------------------*/
unsigned char read_pl12_startsel(void) {
    return (keys>>16)&0x0f;
}


void input_reset(void)
{
    SDL_Event event;

    keys   =~0;
    while(SDL_PollEvent(&event))
	    SDL_Delay(10);
}
