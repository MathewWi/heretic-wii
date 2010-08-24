#include <gccore.h>
#include <string.h>
#include <malloc.h>

#include <stdlib.h>
#include <unistd.h>

#include <SDL/SDL.h>

#include <wiiuse/wpad.h>
#include <sdcard/wiisd_io.h>
#include <fat.h>

#include "screen.h"

#include <asndlib.h>

#include <stdlib.h>
#include <stdarg.h>
#include <sys/time.h>

#include "doomdef.h"
#include "r_local.h"
#include "p_local.h"    // for P_AproxDistance
#include "sounds.h"
#include "i_sound.h"
#include "soundst.h"


// Macros

#define stricmp strcasecmp
#define DEFAULT_ARCHIVEPATH     ""
#define PRIORITY_MAX_ADJUST 10
#define DIST_ADJUST (MAX_SND_DIST/PRIORITY_MAX_ADJUST)

// Globals
boolean joystickpresent;

char *str_trace= "";

int16_t ShortSwap(int16_t b)
{

	b=(b<<8) | ((b>>8) & 0xff);

	return b;
}

int32_t LongSwap(int32_t b)
{

	b=(b<<24) | ((b>>24) & 0xff) | ((b<<8) & 0xff0000) | ((b>>8) & 0xff00);

	return b;
}

int is_16_9=0;

//byte *pcscreen, *destscreen;

#include <wiiuse/wpad.h>

#define TIME_SLEEP_SCR 5*60

static syswd_t scr_poweroff;

int time_sleep=0;


static void scr_poweroff_handler(syswd_t alarm, void * arg)
{	
	if(time_sleep>1) time_sleep--;
	if(time_sleep==1) SetVideoSleep(1);
	
}

unsigned temp_pad=0,new_pad=0,old_pad=0;

WPADData * wmote_datas=NULL;


unsigned wiimote_read()
{
int n;

int ret=-1;

unsigned type=0;

unsigned butt=0;

	wmote_datas=NULL;

	//w_index=-1;

	for(n=0;n<4;n++) // busca el primer wiimote encendido y usa ese
		{
		ret=WPAD_Probe(n, &type);

		if(ret>=0)
			{
			
			butt=WPAD_ButtonsHeld(n);
			
				wmote_datas=WPAD_Data(n);

		//	w_index=n;

			break;
			}
		}

	if(n==4) butt=0;

		temp_pad=butt;

		new_pad=temp_pad & (~old_pad);old_pad=temp_pad;

	if(new_pad)
		{
		time_sleep=TIME_SLEEP_SCR;
		SetVideoSleep(0);
		}

return butt;
}


int exit_by_reset=0;

int return_reset=2;

void reset_call() {exit_by_reset=return_reset;}
void power_call() {exit_by_reset=3;}


// Public Data

static int DisplayTicker = 0;

// sd mounted?

int sd_ok=0;

extern Mtx	modelView; // screenlib matrix (used for 16:9 screens)

u32 *wii_scr, *wii_scr2; // wii texture displayed

GXTexObj text_scr, text_scr2;


#define TEXT_W SCREENWIDTH
#define TEXT_H SCREENHEIGHT

void wii_test(u32 color)
{
int n;
	
	for(n=0;n<TEXT_W * TEXT_H; n++) wii_scr[n]=color;

	CreateTexture(&text_scr, TILE_RGBA8 , wii_scr, TEXT_W , TEXT_H, 0);
	
	SetTexture(&text_scr);

	DrawFillBox(0, -12, SCR_WIDTH, SCR_HEIGHT+24, 999, 0xffffffff);

	SetTexture(NULL);

	Screen_flip();
}

void My_Quit(void)
{
	WPAD_Shutdown();
	ASND_End();

	SYS_RemoveAlarm(scr_poweroff);

	if(sd_ok)
		{
		fatUnmount("sd");
		__io_wiisd.shutdown();sd_ok=0;
		}

	sleep(1);

	if(exit_by_reset==2)
		SYS_ResetSystem(SYS_RETURNTOMENU, 0, 0);
	if(exit_by_reset==3)
		SYS_ResetSystem(SYS_POWEROFF_STANDBY, 0, 0);
	return;
}

int main(int argc, char **argv)
{
	struct timespec tb;
	myargc = 0;//argc;
	myargv = NULL;//argv;

	if (*((u32*)0x80001800) && strncmp("STUBHAXX", (char *)0x80001804, 8) == 0) return_reset=1;
	else return_reset=2;

	if(CONF_Init()==0)
	{
		is_16_9=CONF_GetAspectRatio()!=0;
		
	}

	SYS_SetResetCallback(reset_call); // esto es para que puedas salir al pulsar boton de RESET
	SYS_SetPowerCallback(power_call); // esto para apagar con power

	InitScreen();  // Inicialización del Video

	//pcscreen = destscreen = memalign(32, SCREENWIDTH * SCREENHEIGHT);
	wii_scr= memalign(32, TEXT_W * (TEXT_H+8) *4); // for game texture
	wii_scr2= memalign(32, TEXT_W * (TEXT_H+8) *4); // for menu texture

	wii_test(0x0);

	if(is_16_9)
		{
		ChangeProjection(0,SCR_HEIGHT<=480 ? -12: 0,848,SCR_HEIGHT+(SCR_HEIGHT<=480 ? 16: 0));
		guMtxIdentity(modelView);

		GX_SetCurrentMtx(GX_PNMTX0); // selecciona la matriz
		guMtxTrans(modelView, 104.0f, 0.0f, 0.0f);
		GX_LoadPosMtxImm(modelView,	GX_PNMTX0); // carga la matriz mundial como identidad
		
		}
	
	char title1[]="Heretic for Wii";
	char title2[]="Ported by Arikado";
	char title3[]="http://arikadosblog.blogspot.com";

	//char title1[]=

	letter_size(16, 32);

	PY=32;PX=(640-strlen(title1)*16)/2;
	s_printf ("%s", title1);

	PY=32+64;PX=(640-strlen(title2)*16)/2;
	s_printf("%s", title2);

	PY=32+192;PX=(640-strlen(title3)*16)/2;
	s_printf("%s", title3);

	Screen_flip();

	letter_size(12, 24);

    PAD_Init();

	WPAD_Init();
	WPAD_SetIdleTimeout(60*5); // 5 minutes 

	WPAD_SetDataFormat(WPAD_CHAN_ALL, WPAD_FMT_BTNS_ACC_IR); // ajusta el formato para acelerometros en todos los wiimotes

	if(__io_wiisd.startup())
		{
		sd_ok = fatMountSimple("sd", &__io_wiisd);
		if(!sd_ok) 
			{	__io_wiisd.shutdown();
				WPAD_Shutdown();
				sleep(1); 

				if(exit_by_reset==2)
					SYS_ResetSystem(SYS_RETURNTOMENU, 0, 0);
				if(exit_by_reset==3)
					SYS_ResetSystem(SYS_POWEROFF_STANDBY, 0, 0);

				return 0;
			}
		}

	ASND_Init();
	ASND_Pause(0);
	

	SYS_CreateAlarm(&scr_poweroff);
	tb.tv_sec = 1;tb.tv_nsec = 0;
	SYS_SetPeriodicAlarm(scr_poweroff, &tb, &tb, scr_poweroff_handler, NULL);
	atexit (My_Quit);
	
	sleep(3);

	D_DoomMain();

    return 0;
}

/*
============================================================================

							CONSTANTS

============================================================================
*/

#define SC_INDEX                0x3C4
#define SC_RESET                0
#define SC_CLOCK                1
#define SC_MAPMASK              2
#define SC_CHARMAP              3
#define SC_MEMMODE              4


#define GC_INDEX                0x3CE
#define GC_SETRESET             0
#define GC_ENABLESETRESET 1
#define GC_COLORCOMPARE 2
#define GC_DATAROTATE   3
#define GC_READMAP              4
#define GC_MODE                 5
#define GC_MISCELLANEOUS 6
#define GC_COLORDONTCARE 7
#define GC_BITMASK              8




//==================================================
//
// joystick vars
//
//==================================================

boolean joystickpresent;
unsigned joystickx, joysticky;
         

boolean I_ReadJoystick (void) // returns false if not connected
{
    return false;
}

//==================================================

#define VBLCOUNTER              34000           // hardware tics to a frame

#define TIMERINT 8
#define KEYBOARDINT 9

#define MOUSEB1 1
#define MOUSEB2 2
#define MOUSEB3 4

boolean mousepresent;
//static  int tsm_ID = -1; // tsm init flag

//===============================

int ticcount;


boolean novideo; // if true, stay in text mode for debugging


//==========================================================================

//--------------------------------------------------------------------------
//
// FUNC I_GetTime
//
// Returns time in 1/35th second tics.
//
//--------------------------------------------------------------------------
#define ticks_to_msecs(ticks)      ((u32)((ticks)/(TB_TIMER_CLOCK)))

u32 gettick();
int I_GetTime (void)
{

   // ticcount = (SDL_GetTicks()*35)/1000;

   // struct timeval tv;
   // gettimeofday( &tv, 0 ); 

    //printf( "GT: %lx %lx\n", tv.tv_sec, tv.tv_usec );

 //   ticcount = ((tv.tv_sec * 1000000) + tv.tv_usec) / 28571;
  //  ticcount = ((tv.tv_sec / 35) + (tv.tv_usec / 28571));

    ticcount= (ticks_to_msecs(gettick()) / 35) & 0x7fffffff;
    return( ticcount );
}


/*
============================================================================

								USER INPUT

============================================================================
*/

//--------------------------------------------------------------------------
//
// PROC I_WaitVBL
//
//--------------------------------------------------------------------------

void I_WaitVBL(int vbls)
{
//	if( novideo )
	//{
	//	return;
	//}


	while( vbls-- )
	{
     //   usleep( 16667000/1000 );
	}

}

//--------------------------------------------------------------------------
//
// PROC I_SetPalette
//
// Palette source must use 8 bit RGB elements.
//
//--------------------------------------------------------------------------

u32 paleta[256], paleta2[256];


void I_SetPalette(byte *palette)
{
int n;

usegamma=2;
//usegamma=4;
	for(n=0;n<256;n++)
		{
		paleta[n]= gammatable[usegamma][palette[0]]
			| (gammatable[usegamma][palette[1]]<<8) 
			| (gammatable[usegamma][palette[2]]<<16) | 0xff000000;
		
		paleta2[n]=paleta[n]; // menu palette

		palette+=3;
		}
// trick
paleta2[0]=0x00000000;
}

/*
============================================================================

							GRAPHICS MODE

============================================================================
*/

int wiimote_scr_info=0;

static int use_cheat=0;

extern byte *screen2;

extern boolean MenuActive;

void wii_scr_update()
{
	int n,m,l,ll;

	static int blink=0;

	I_StartFrame();

//	if(novideo) return;

    l=ll=0;

	for(n=0;n<SCREENHEIGHT; n++) 
	{
		for(m=0;m<SCREENWIDTH; m++)
			{
			wii_scr[l+m]=paleta[screen[ll+m]]; // used by game
			wii_scr2[l+m]=paleta2[screen2[ll+m]]; // used for draw menu
			}
		
		l+=TEXT_W;
		ll+=SCREENWIDTH;
	}

	memset((void *) screen2, 0, SCREENWIDTH*SCREENHEIGHT);

	CreateTexture(&text_scr, TILE_SRGBA8 , wii_scr, TEXT_W , TEXT_H, 0);

	SetTexture(&text_scr);

	DrawFillBox(0, 0, SCR_WIDTH, SCR_HEIGHT, 0, MenuActive ? 0xffa0a0a0 :0xffffffff); // reduce bright if Menu is active
	
	//Draw crosshair if using IR
	if(wmote_datas && wmote_datas->exp.type==WPAD_EXP_NUNCHUK){
	SetTexture(NULL);
	DrawRoundFillBox(wmote_datas->ir.x, wmote_datas->ir.y, 20, 20, 0, 0xffffffff);
	}

	CreateTexture(&text_scr2, TILE_SRGBA8 , wii_scr2, TEXT_W , TEXT_H, 0);

	SetTexture(&text_scr2);

	DrawFillBox(0, 0, SCR_WIDTH, SCR_HEIGHT, 0, 0xffffffff);

	SetTexture(NULL);

	
	if(wiimote_scr_info & 1)
		{
		//Rewrite this with a proper Wii settings GUI later --Arikado
		
		/*SetTexture(NULL);
		DrawFillBox(0, 0, SCR_WIDTH, SCR_HEIGHT, 0, (use_cheat>4) ? 0xff8f8f00 : 0xffff0000);

		PY=8;
		PX=8;
		letter_size(16, 32);
		autocenter=1;
		s_printf("Wiimote + Nunchuk info");
		autocenter=0;
		letter_size(12, 28);
		
		PY+=64;PX=8;
		s_printf("Nunchuk Stick -> To Move");
		
		PY+=32;PX=8;
		s_printf("Button HOME -> Menu");

		PY+=32;PX=8;
		s_printf("hold B with A -> Enter in Menu");
		
		PY+=32;PX=8;
		s_printf("Button A -> double click for Open/Use");
		
		PY+=32;PX=8;
		s_printf("Button B -> Fire, hold with A to Use Item");

		PY+=32;PX=8;
		s_printf("Button Z -> Run, hold with Wiimote L,R,U,D (Weapon)");
		
		PY+=32;PX=8;
		s_printf("Button C -> Jump, hold with Wiimote U,D,A (for fly)");

		PY+=32;PX=8;
		s_printf("Wiimote Left, Right -> Strafe (Except with Z)");
		
		PY+=32;PX=8;
		s_printf("Wiimote Up, Down -> Look Up/Down (Except with C & Z)");

		PY+=32;PX=8;
		s_printf("Button PLUS & MINUS -> Select Item");

		PY+=32;PX=8;
		s_printf("Button One -> Use Map");

		PY+=32;PX=8;
		s_printf("Button Two -> to see this info or exit");

		PY+=32;PX=8;
		s_printf("Note: L,R,U,D is for Left, Right, Up, Down");

		autocenter=1;
		PY+=32;PX=8;
		if(!((blink>>4) & 1)) s_printf("Press '2' to exit");
		blink++;
		autocenter=0;*/

		}
	




	Screen_flip();
}

/*
==============
=
= I_Update
=
==============
*/

int UpdateState;
extern int screenblocks;

void I_Update (void)
{
	int i;
	byte *dest;
	int tics;
	static int lasttic;
	int update=0;


	if(DisplayTicker)
	{
		if(/*screenblocks > 9 ||*/ UpdateState&(I_FULLSCRN|I_MESSAGES))
		{
			//dest = (byte *)screen;
			dest = (byte *) screen;
		}
		else
		{
			dest = (byte *) screen;
		}
		tics = ticcount-lasttic;
		lasttic = ticcount;
	
		if(tics > 20)
		{
			tics = 20;
		}
		for(i = 0; i < tics; i++)
		{
			*dest = 0xff;
			dest += 2;
		}
		for(i = tics; i < 20; i++)
		{
			*dest = 0x00;
			dest += 2;
		}
		
	}

	

	if(UpdateState == I_NOUPDATE)
	{
		return;
	}
	
	if(UpdateState&I_FULLSCRN)
	{
		//memcpy(pcscreen, screen, SCREENWIDTH*SCREENHEIGHT);
		UpdateState = I_NOUPDATE; // clear out all draw types

		update=1;

	}



	if(UpdateState&I_FULLVIEW)
	{
		if(UpdateState&I_MESSAGES && screenblocks > 7)
		{
			/*
			for(i = 0; i <
				(viewwindowy+viewheight)*SCREENWIDTH; i += SCREENWIDTH)
			{
				memcpy(pcscreen+i, screen+i, SCREENWIDTH);
			}*/
			UpdateState &= ~(I_FULLVIEW|I_MESSAGES);
			update=1;

          
		}
		else
		{
			/*
			for(i = viewwindowy*SCREENWIDTH+viewwindowx; i <
				(viewwindowy+viewheight)*SCREENWIDTH; i += SCREENWIDTH)
			{
				memcpy(pcscreen+i, screen+i, viewwidth);
			}
			*/
			UpdateState &= ~I_FULLVIEW;

			update=1;

		}
	
	}
	
	if(UpdateState&I_STATBAR)
	{
		/*memcpy(pcscreen+SCREENWIDTH*(SCREENHEIGHT-SBARHEIGHT),
			screen+SCREENWIDTH*(SCREENHEIGHT-SBARHEIGHT),
			SCREENWIDTH*SBARHEIGHT);*/
		UpdateState &= ~I_STATBAR;


		update=1;
	}
	if(UpdateState&I_MESSAGES)
	{
		//memcpy(pcscreen, screen, SCREENWIDTH*28);
		UpdateState &= ~I_MESSAGES;

	update=1;
	}

if(update) wii_scr_update();
}

//--------------------------------------------------------------------------
//
// PROC I_InitGraphics
//
//--------------------------------------------------------------------------

void I_InitGraphics(void)
{

	boolean grabMouse = 0;


		I_SetPalette( W_CacheLumpName("PLAYPAL", PU_CACHE) );
}

//--------------------------------------------------------------------------
//
// PROC I_ShutdownGraphics
//
//--------------------------------------------------------------------------

void I_ShutdownGraphics(void)
{
}

//===========================================================================


//
// I_StartTic
//
void I_StartTic (void)
{

}


/*
===============
=
= I_StartFrame
=
===============
*/


#define J_DEATHZ 48 // death zone for sticks

//extern void set_my_cheat(int indx);
boolean usergame;

void I_StartFrame (void)
{
	// hermes
    
	event_t ev; // joystick event
	event_t event; // keyboard event

	int k_up=0,k_down=0,k_left=0,k_right=0,k_esc=0,k_enter=0,k_tab=0, k_del=0, k_pag_down=0;
	int k_jump=0,k_leftsel=0,k_rightsel=0,k_alt=0;
	int k_1=0,k_2=0,k_3=0,k_4=0,k_fly=0;

	static int w_jx=1,w_jy=1;
	
	ir_t * ir;

    PAD_ScanPads();
	s32 pad_stickx = PAD_StickX(0);
	s32 pad_sticky = PAD_StickY(0);
	s32 pad_substickx = PAD_SubStickX(0);
	s32 pad_substicky = PAD_SubStickY(0);

	WPAD_ScanPads();

	wiimote_read();
	WPAD_IR(WPAD_CHAN_0, &wmote_datas->ir);
	ev.type = ev_joystick;
	ev.data1 =  0;
	ev.data2 =  0;
	ev.data3 =  0;


	/*if(wmote_datas)
		{
		if((new_pad & WPAD_BUTTON_B) && (wiimote_scr_info & 1)) use_cheat++;

			if(new_pad & WPAD_BUTTON_2) 
			{
			wiimote_scr_info^=1;	
			}
		}*/
		
	wiimote_scr_info&=1;

    //Wiimote + Nunchuk Controls
    if(wmote_datas && wmote_datas->exp.type==WPAD_EXP_NUNCHUK)
	{
		// Menu
		if(new_pad & WPAD_BUTTON_HOME) k_esc=1;
        
		if(!MenuActive){
		// stick
		//Up
		if(wmote_datas->exp.nunchuk.js.pos.y> (wmote_datas->exp.nunchuk.js.center.y+J_DEATHZ))
			{time_sleep=TIME_SLEEP_SCR;SetVideoSleep(0);if(w_jy & 1) k_up=1; w_jy&= ~1; ev.data3 = -1;} else w_jy|=1;
		//Down
		if(wmote_datas->exp.nunchuk.js.pos.y< (wmote_datas->exp.nunchuk.js.center.y-J_DEATHZ))
			{time_sleep=TIME_SLEEP_SCR;SetVideoSleep(0);if(w_jy & 2) k_down=1; w_jy&= ~2;ev.data3 = 1;} else w_jy|=2;
        //Left Strafe
		if((wmote_datas->exp.nunchuk.js.ang>=270-45 && wmote_datas->exp.nunchuk.js.ang<=270+45) && wmote_datas->exp.nunchuk.js.mag>=0.9)
			{k_left=1; k_alt=1;}
		//Right Strafe
		if((wmote_datas->exp.nunchuk.js.ang>=90-45 && wmote_datas->exp.nunchuk.js.ang<=90+45) && wmote_datas->exp.nunchuk.js.mag>=0.9) 
			{k_right=1;k_alt=1;}
		}
		
        if(!MenuActive){	
		//Turning via IR
		//Right
		if(wmote_datas->ir.x > 350)
			{time_sleep=TIME_SLEEP_SCR;SetVideoSleep(0);if(w_jx & 1) k_right=1; w_jx&= ~1; ev.data2 = 1;} 
		else w_jx|=1;
		//Left
		if(wmote_datas->ir.x < 290)
			{time_sleep=TIME_SLEEP_SCR;SetVideoSleep(0);if(w_jx & 2) k_left=1; w_jx&= ~2;ev.data2 = -1;} 
		else w_jx|=2;
		//Up
		if(wmote_datas->ir.y < 140)
			k_pag_down=1;
		//Down
		if(wmote_datas->ir.y > 340)
		    k_del=1;
		}
		
		if(MenuActive){
		if(WPAD_ButtonsDown(0)&WPAD_BUTTON_DOWN)
		{time_sleep=TIME_SLEEP_SCR;SetVideoSleep(0);if(w_jy & 2) k_down=1; w_jy&= ~2;ev.data3 = 1;} else w_jy|=2;
		if(WPAD_ButtonsDown(0)&WPAD_BUTTON_UP)
		{time_sleep=TIME_SLEEP_SCR;SetVideoSleep(0);if(w_jy & 1) k_up=1; w_jy&= ~1; ev.data3 = -1;} else w_jy|=1;
		}
		
		if(old_pad & WPAD_NUNCHUK_BUTTON_Z) ev.data1|=4; // Run

		// Map
		if(new_pad & WPAD_NUNCHUK_BUTTON_C) k_tab=1;

		// Select alphanumeric character (for naming saves)
		if(MenuActive){
		if(old_pad & WPAD_NUNCHUK_BUTTON_Z)
			{
			if(new_pad & WPAD_BUTTON_LEFT) k_1=1;
			if(new_pad & WPAD_BUTTON_UP) k_2=1;
			if(new_pad & WPAD_BUTTON_RIGHT) k_3=1;
			if(new_pad & WPAD_BUTTON_DOWN) k_4=1;
			}
		}
		
		//Change Weapon
		if(!MenuActive){
			if(new_pad & WPAD_BUTTON_LEFT) k_1=1;
			if(new_pad & WPAD_BUTTON_UP) k_2=1;
			if(new_pad & WPAD_BUTTON_RIGHT) k_3=1;
			if(new_pad & WPAD_BUTTON_DOWN) k_4=1;
		}
        
		if(!MenuActive){
		if(((new_pad & WPAD_BUTTON_A) && (old_pad & WPAD_BUTTON_B)))	
			{k_enter=1;}  // use object
		else
			{
			if(old_pad & WPAD_BUTTON_A) {ev.data1|=2;}  // open
			if(old_pad & WPAD_BUTTON_B) ev.data1|=1; // fire
			}
		}
		
		if(MenuActive){
		if(WPAD_ButtonsDown(0) & WPAD_BUTTON_A) {k_enter=1;}  // Select Option
		}

		if(new_pad & WPAD_BUTTON_MINUS) k_leftsel=1; // sel left object
		if(new_pad & WPAD_BUTTON_PLUS) k_rightsel=1; // sel right object

		D_PostEvent (&ev);

		}
	//End Wiimote and Nunchuk Controls
	
	//Classic Controller Controls
	
		if(wmote_datas && wmote_datas->exp.type==WPAD_EXP_CLASSIC)
		{

		// Menu
		if(WPAD_ButtonsDown(0)&WPAD_CLASSIC_BUTTON_HOME) k_esc=1;

		// Movement
		//Down
		if((wmote_datas->exp.classic.ljs.ang>=180-45 && wmote_datas->exp.classic.ljs.ang<=180+45) && wmote_datas->exp.classic.ljs.mag>=0.9)
			{time_sleep=TIME_SLEEP_SCR;SetVideoSleep(0);if(w_jy & 2) k_down=1; w_jy&= ~2;ev.data3 = 1;}
        else w_jy|=2;
		//Up
		if((wmote_datas->exp.classic.ljs.ang>=360-45 || wmote_datas->exp.classic.ljs.ang<=45) && wmote_datas->exp.classic.ljs.mag>=0.9)
			{time_sleep=TIME_SLEEP_SCR;SetVideoSleep(0);if(w_jy & 1) k_up=1; w_jy&= ~1; ev.data3 = -1;} 
		else w_jy|=1;
		//Left Strafe
		if((wmote_datas->exp.classic.ljs.ang>=270-45 && wmote_datas->exp.classic.ljs.ang<=270+45) && wmote_datas->exp.classic.ljs.mag>=0.9)
			{k_left=1; k_alt=1;}
		//Right Strafe
		if((wmote_datas->exp.classic.ljs.ang>=90-45 && wmote_datas->exp.classic.ljs.ang<=90+45) && wmote_datas->exp.classic.ljs.mag>=0.9) 
			{k_right=1;k_alt=1;}
		
		//Turning
		//Right
		if((wmote_datas->exp.classic.rjs.ang>=90-45 && wmote_datas->exp.classic.rjs.ang<=90+45) && wmote_datas->exp.classic.rjs.mag>=0.9)
			{time_sleep=TIME_SLEEP_SCR;SetVideoSleep(0);if(w_jx & 1) k_right=1; w_jx&= ~1; ev.data2 = 1;} 
		else w_jx|=1;
		//Left
		if((wmote_datas->exp.classic.rjs.ang>=270-45 && wmote_datas->exp.classic.rjs.ang<=270+45) && wmote_datas->exp.classic.rjs.mag>=0.9)
			{time_sleep=TIME_SLEEP_SCR;SetVideoSleep(0);if(w_jx & 2) k_left=1; w_jx&= ~2;ev.data2 = -1;} 
		else w_jx|=2;
		//Up
		if((wmote_datas->exp.classic.rjs.ang>=360-45 || wmote_datas->exp.classic.rjs.ang<=45) && wmote_datas->exp.classic.rjs.mag>=0.9)
			k_pag_down=1;
		//Down
		if((wmote_datas->exp.classic.rjs.ang>=180-45 && wmote_datas->exp.classic.rjs.ang<=180+45) && wmote_datas->exp.classic.rjs.mag>=0.9)
		    k_del=1;

		if(WPAD_ButtonsDown(0)&WPAD_CLASSIC_BUTTON_X || WPAD_ButtonsHeld(0)&WPAD_CLASSIC_BUTTON_X) ev.data1|=4; // Run

		if(WPAD_ButtonsDown(0)&WPAD_CLASSIC_BUTTON_ZR) k_tab=1; //Toggle Map

		// Change weapon
		if(WPAD_ButtonsDown(0)&WPAD_CLASSIC_BUTTON_LEFT)  k_1=1;
		if(WPAD_ButtonsDown(0)&WPAD_CLASSIC_BUTTON_RIGHT) k_2=1;
		if(WPAD_ButtonsDown(0)&WPAD_CLASSIC_BUTTON_UP)    k_3=1;
		if(WPAD_ButtonsDown(0)&WPAD_CLASSIC_BUTTON_DOWN)  k_4=1;

		if(WPAD_ButtonsDown(0)&WPAD_CLASSIC_BUTTON_Y)	 {k_enter=1;}  // Use Object
		
		if(WPAD_ButtonsDown(0)&WPAD_CLASSIC_BUTTON_A) {ev.data1|=2;}  // Open
		
		if((WPAD_ButtonsDown(0)&WPAD_CLASSIC_BUTTON_B) || (WPAD_ButtonsHeld(0)&WPAD_CLASSIC_BUTTON_B)) ev.data1|=1; // Fire
			

		if(WPAD_ButtonsDown(0)&WPAD_CLASSIC_BUTTON_FULL_R) k_leftsel=1; // Select left object
		if(WPAD_ButtonsDown(0)&WPAD_CLASSIC_BUTTON_FULL_L) k_rightsel=1; // Select right object

		D_PostEvent (&ev);

		}
	
	//End Classic Controller Controls
	
	
	//GC Controls
	
	if(!wmote_datas || (wmote_datas->exp.type!=WPAD_EXP_CLASSIC && wmote_datas->exp.type!=WPAD_EXP_NUNCHUK))
	{
	
		// Menu
		if(PAD_ButtonsDown(0)&PAD_BUTTON_START) k_esc=1;

		// Movement
		//Down
		if(pad_sticky < -20)
			{time_sleep=TIME_SLEEP_SCR;SetVideoSleep(0);if(w_jy & 2) k_down=1; w_jy&= ~2;ev.data3 = 1;}
        else w_jy|=2;
		//Up
		if(pad_sticky > 20)
			{time_sleep=TIME_SLEEP_SCR;SetVideoSleep(0);if(w_jy & 1) k_up=1; w_jy&= ~1; ev.data3 = -1;} 
		else w_jy|=1;
		if(pad_stickx < -20) {k_left=1; k_alt=1;} //Left Strafe
		if(pad_stickx > 20)  {k_right=1;k_alt=1;} // Right Strafe
		
		//Turning
		//Right
		if(pad_substickx > 20)
			{time_sleep=TIME_SLEEP_SCR;SetVideoSleep(0);if(w_jx & 1) k_right=1; w_jx&= ~1; ev.data2 = 1;} 
		else w_jx|=1;
		//Left
		if(pad_substickx < -20)
			{time_sleep=TIME_SLEEP_SCR;SetVideoSleep(0);if(w_jx & 2) k_left=1; w_jx&= ~2;ev.data2 = -1;} 
		else w_jx|=2;
		//Up
		if(pad_substicky < -20) k_del=1;
		//Down
		if(pad_substicky > 20) k_pag_down=1;

		if(PAD_ButtonsDown(0)&PAD_BUTTON_Y || PAD_ButtonsHeld(0)&PAD_BUTTON_Y) ev.data1|=4; // Run

		if(PAD_ButtonsDown(0)&PAD_TRIGGER_Z) k_tab=1; //Toggle Map

		// Change weapon
		if(PAD_ButtonsDown(0)&PAD_BUTTON_LEFT)  k_1=1;
		if(PAD_ButtonsDown(0)&PAD_BUTTON_RIGHT) k_2=1;
		if(PAD_ButtonsDown(0)&PAD_BUTTON_UP)    k_3=1;
		if(PAD_ButtonsDown(0)&PAD_BUTTON_DOWN)  k_4=1;

		if(PAD_ButtonsDown(0)&PAD_BUTTON_X)	 {k_enter=1;}  // Use Object
		
		if(PAD_ButtonsDown(0)&PAD_BUTTON_B) {ev.data1|=2;}  // Open
		
		if((PAD_ButtonsDown(0)&PAD_BUTTON_A) || (PAD_ButtonsHeld(0)&PAD_BUTTON_A)) ev.data1|=1; // Fire
			

		if(PAD_ButtonsDown(0)&PAD_TRIGGER_R) k_leftsel=1; // Select left object
		if(PAD_ButtonsDown(0)&PAD_TRIGGER_L) k_rightsel=1; // Select right object

		D_PostEvent (&ev);
	
	}
	
	//End GC Controls
	

	// jump
	event.data1 = '/';
	if(k_jump) event.type = ev_keydown; else event.type = ev_keyup;
	D_PostEvent(&event);

	event.data1 = KEY_UPARROW;
	if(k_up) event.type = ev_keydown; else event.type = ev_keyup;
	D_PostEvent(&event);

	event.data1 = KEY_DOWNARROW;
	if(k_down) event.type = ev_keydown; else event.type = ev_keyup;
	D_PostEvent(&event);

	event.data1 = KEY_LEFTARROW;
	if(k_left) event.type = ev_keydown; else event.type = ev_keyup;
	D_PostEvent(&event);

	event.data1 = KEY_RIGHTARROW;
	if(k_right) event.type = ev_keydown; else event.type = ev_keyup;
	D_PostEvent(&event);

	// strafe
	event.data1 = KEY_ALT;
	if(k_alt) event.type = ev_keydown; else event.type = ev_keyup;
	D_PostEvent(&event);

	// enter
	event.data1 = KEY_ENTER;
	if(k_enter) event.type = ev_keydown; else event.type = ev_keyup;
	D_PostEvent(&event);

	// menu
	event.data1 = KEY_ESCAPE;
	if(k_esc) event.type = ev_keydown; else event.type = ev_keyup;
	D_PostEvent(&event);

	// mapa
	event.data1 = KEY_TAB;
	if(k_tab) event.type = ev_keydown; else event.type = ev_keyup;
	D_PostEvent(&event);

	event.data1 = KEY_LEFTBRACKET;
	if(k_leftsel) event.type = ev_keydown; else event.type = ev_keyup;
	D_PostEvent(&event);

	event.data1 = KEY_RIGHTBRACKET;
	if(k_rightsel) event.type = ev_keydown; else event.type = ev_keyup;
	D_PostEvent(&event);

	//  mira abajo
	event.data1 = KEY_DEL;
	if(k_del) event.type = ev_keydown; else event.type = ev_keyup;
	D_PostEvent(&event);

	// mira arriba
	event.data1 = KEY_PGDN;
	if(k_pag_down) event.type = ev_keydown; else event.type = ev_keyup;
	D_PostEvent(&event);

	// weapon
	event.data1 = '1';
	if(k_1) event.type = ev_keydown; else event.type = ev_keyup;
	D_PostEvent(&event);
	event.data1 = '2';
	if(k_2) event.type = ev_keydown; else event.type = ev_keyup;
	D_PostEvent(&event);
	event.data1 = '3';
	if(k_3) event.type = ev_keydown; else event.type = ev_keyup;
	D_PostEvent(&event);
	event.data1 = '4';
	if(k_4) event.type = ev_keydown; else event.type = ev_keyup;
	D_PostEvent(&event);

	// fly up
	event.data1 = KEY_PGUP;
	if(k_fly==-1) event.type = ev_keydown; else event.type = ev_keyup;
	D_PostEvent(&event);

	// fly down
	event.data1 = KEY_INS;
	if(k_fly==1) event.type = ev_keydown; else event.type = ev_keyup;
	D_PostEvent(&event);

	// fly drop
	event.data1 = KEY_HOME;
	if(k_fly==-2) event.type = ev_keydown; else event.type = ev_keyup;
	D_PostEvent(&event);

}


/*
============================================================================

							MOUSE

============================================================================
*/


/*
================
=
= StartupMouse
=
================
*/

void I_StartupCyberMan(void);

void I_StartupMouse (void)
{
	static boolean mousepresent = 0;

//	I_StartupCyberMan();
}




/*
===============
=
= I_StartupJoystick
=
===============
*/



void I_StartupJoystick (void)
{
// hermes

joystickpresent = true;

centerx = 0;
centery = 0;

}

/*
===============
=
= I_Init
=
= hook interrupts and set graphics mode
=
===============
*/

void I_Init (void)
{
	extern void I_StartupTimer(void);

//	novideo = 0;
	
	I_StartupMouse();

	I_StartupJoystick();
//	ST_Message("  S_Init... ");

	S_Init();
	S_Start();
}


/*
===============
=
= I_Shutdown
=
= return to default system state
=
===============
*/

void I_Shutdown (void)
{
	I_ShutdownGraphics ();
	S_ShutDown ();
}


/*
================
=
= I_Error
=
================
*/
char scr_str[256];

void I_Error (char *error, ...)
{
	va_list argptr;

	D_QuitNetGame ();
	I_Shutdown ();
	va_start (argptr,error);
	vsprintf (scr_str,error, argptr);
	va_end (argptr);
	
	PX=32;
	PY=16;
	letter_size(12, 24);
	s_printf ("%s\n", scr_str);
	Screen_flip();
	sleep(10);
	exit (1);
}

//--------------------------------------------------------------------------
//
// I_Quit
//
// Shuts down net game, saves defaults, prints the exit text message,
// goes to text mode, and exits.
//
//--------------------------------------------------------------------------

void I_Quit(void)
{
	D_QuitNetGame();
	M_SaveDefaults();
	I_Shutdown();

	exit(0);
}

/*
===============
=
= I_ZoneBase
=
===============
*/

byte *I_ZoneBase (int *size)
{
	static byte *ptr=NULL;
    int heap = 0x800000*2;

	extern void* SYS_AllocArena2MemLo(u32 size,u32 align);
  
	if(!ptr)
		{
		ptr =SYS_AllocArena2MemLo(heap,32);// malloc ( heap );

//		ST_Message ("  0x%x allocated for zone, ", heap);
		//ST_Message ("ZoneBase: 0x%X\n", (int)ptr);

		if ( ! ptr )
			I_Error ("  Insufficient DPMI memory!");

		memset(ptr, 255, heap);
		}

	*size = heap;
	
	return ptr;
}

/*
=============
=
= I_AllocLow
=
=============
*/

byte *I_AllocLow (int length)
{
	return malloc( length );
}

/*
============================================================================

						NETWORKING

============================================================================
*/

/* // FUCKED LINES
typedef struct
{
	char    priv[508];
} doomdata_t;
*/ // FUCKED LINES

#define DOOMCOM_ID              0x12345678l

/* // FUCKED LINES
typedef struct
{
	long    id;
	short   intnum;                 // DOOM executes an int to execute commands

// communication between DOOM and the driver
	short   command;                // CMD_SEND or CMD_GET
	short   remotenode;             // dest for send, set by get (-1 = no packet)
	short   datalength;             // bytes in doomdata to be sent

// info common to all nodes
	short   numnodes;               // console is allways node 0
	short   ticdup;                 // 1 = no duplication, 2-5 = dup for slow nets
	short   extratics;              // 1 = send a backup tic in every packet
	short   deathmatch;             // 1 = deathmatch
	short   savegame;               // -1 = new game, 0-5 = load savegame
	short   episode;                // 1-3
	short   map;                    // 1-9
	short   skill;                  // 1-5

// info specific to this node
	short   consoleplayer;
	short   numplayers;
	short   angleoffset;    // 1 = left, 0 = center, -1 = right
	short   drone;                  // 1 = drone

// packet data to be sent
	doomdata_t      data;
} doomcom_t;
*/ // FUCKED LINES

extern  doomcom_t               *doomcom;

/*
====================
=
= I_InitNetwork
=
====================
*/

void I_InitNetwork (void)
{
	int             i;

	i = M_CheckParm ("-net");
	if (!i)
	{
	//
	// single player game
	//
		doomcom = malloc (sizeof (*doomcom) );
		memset (doomcom, 0, sizeof(*doomcom) );
		netgame = false;
		doomcom->id = DOOMCOM_ID;
		doomcom->numplayers = doomcom->numnodes = 1;
		doomcom->deathmatch = false;
		doomcom->consoleplayer = 0;
		doomcom->ticdup = 1;
		doomcom->extratics = 0;
		return;
	}

	netgame = true;
	doomcom = (doomcom_t *)atoi(myargv[i+1]);
//DEBUG
doomcom->skill = startskill;
doomcom->episode = startepisode;
doomcom->map = startmap;
doomcom->deathmatch = deathmatch;

}

void I_NetCmd (void)
{
	if (!netgame)
		I_Error ("I_NetCmd when not in netgame");
}


//==========================================================================
//
//
// I_StartupReadKeys
//
//
//==========================================================================

void I_StartupReadKeys(void)
{
   //if( KEY_ESCAPE pressed )
   //    I_Quit ();
}


//EOF