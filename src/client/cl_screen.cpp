/**
 * @file
 * @brief Master for refresh, status bar, console, chat, notify, etc
 *
 * Full screen console.
 * Put up loading plaque.
 * Blanked background with loading plaque.
 * Blanked background with menu.
 * Full screen image for quit and victory.
 * End of unit intermissions.
 */

/*
All original material Copyright (C) 2002-2015 UFO: Alien Invasion.

Original file from Quake 2 v3.21: quake2-2.31/client/
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "client.h"
#include "cl_screen.h"
#include "cl_console.h"
#include "battlescape/cl_localentity.h"
#include "battlescape/cl_actor.h"
#include "battlescape/cl_view.h"
#include "battlescape/cl_hud.h"
#include "renderer/r_main.h"
#include "renderer/r_draw.h"
#include "renderer/r_local.h"
#include "ui/ui_main.h"
#include "ui/ui_draw.h"
#include "ui/ui_nodes.h"
#include "ui/ui_windows.h"
#include "ui/ui_dragndrop.h"
#include "ui/ui_render.h"
#include "../ports/system.h"

static float scr_con_current;			/* approaches scr_conlines at scr_conspeed */
static float scr_conlines;				/* 0.0 to 1.0 lines of console to display */

static bool screenInitialized = false;/* ready to draw */

static cvar_t* scr_conspeed;
static cvar_t* scr_consize;
static cvar_t* scr_rspeed;
static cvar_t* scr_cursor;
static cvar_t* scr_showcursor;

static char cursorImage[MAX_QPATH];

/**
 * @sa Font_DrawString
 */
static void SCR_DrawString (int x, int y, const char* string)
{
	if (Q_strnull(string))
		return;

	Con_DrawString(string, x, y, strlen(string));
}

/**
 * @sa SCR_DrawLoading
 */
static void SCR_DrawLoadingBar (int x, int y, int w, int h, int percent)
{
	const vec4_t color = {0.3f, 0.3f, 0.3f, 0.7f};

	R_DrawFill(x, y, w, h, color);

	if (percent != 0) {
		const vec4_t colorBar = {0.8f, 0.8f, 0.8f, 0.7f};
		R_DrawFill((int)(x + (h * 0.2)), (int)(y + (h * 0.2)), (int)((w - (h * 0.4)) * percent * 0.01), (int)(h * 0.6), colorBar);
	}
}

/**
 * @brief Precache and loading screen at startup
 * @sa CL_InitAfter
 * @param[in] string Draw the loading string - if the scripts are not parsed, this is
 * not possible, so use false for very early calls
 * @param percent Progress value for the status bar
 */
void SCR_DrawLoadingScreen (bool string, int percent)
{
	R_BeginFrame();

	const image_t* image = R_FindImage("pics/background/loading", it_pic);
	if (image)
		R_DrawImage(viddef.virtualWidth / 2 - image->width / 2, viddef.virtualHeight / 2 - image->height / 2, image);
	if (string) {
		/* Not used with gettext because it would make removing it too easy. */
		UI_DrawString("f_menubig", ALIGN_UC,
			(int)(viddef.virtualWidth / 2), 30,
			0, viddef.virtualWidth, 50, "Download this game for free at http://ufoai.sf.net");
	}
	SCR_DrawLoadingBar((int)(viddef.virtualWidth / 2) - 300, viddef.virtualHeight - 30, 600, 20, percent);

	R_EndFrame();
}

/**
 * @brief Draws the current downloading status
 * @sa SCR_DrawLoadingBar
 * @sa CL_StartHTTPDownload
 * @sa CL_HTTP_Progress
 */
static void SCR_DrawDownloading (void)
{
	const char* dlmsg = va(_("Downloading [%s]"), cls.downloadName);
	UI_DrawString("f_menubig", ALIGN_UC,
		(int)(viddef.virtualWidth / 2),
		(int)(viddef.virtualHeight / 2 - 60),
		(int)(viddef.virtualWidth / 2),
		viddef.virtualWidth, 50, dlmsg, 1);

	SCR_DrawLoadingBar((int)(viddef.virtualWidth / 2) - 300, viddef.virtualHeight - 30, 600, 20, (int)cls.downloadPercent);
}

/**
 * @brief Draws the current loading pic of the map from base/pics/maps/loading
 * @sa SCR_DrawLoadingBar
 */
void SCR_DrawLoading (int percent)
{
	if (cls.downloadName[0]) {
		SCR_DrawDownloading();
		return;
	}

	R_BeginFrame();

	UI_Draw();

	SCR_DrawLoadingBar((int)(viddef.virtualWidth / 2) - 300, viddef.virtualHeight - 30, 600, 20, percent);

	R_EndFrame();
}

/**
 * @brief Allows rendering code to cache all needed sbar graphics
 */
static void SCR_TouchPics (void)
{
	if (scr_cursor->integer) {
		if (scr_cursor->integer > 9 || scr_cursor->integer < 0)
			SCR_ChangeCursor(1);

		R_FindImage("pics/cursors/wait", it_pic);
		R_FindImage("pics/cursors/ducked", it_pic);
		R_FindImage("pics/cursors/reactionfire", it_pic);
		R_FindImage("pics/cursors/reactionfiremany", it_pic);
		Com_sprintf(cursorImage, sizeof(cursorImage), "pics/cursors/cursor%i", scr_cursor->integer);
		if (!R_FindImage(cursorImage, it_pic)) {
			Com_Printf("SCR_TouchPics: Could not register cursor: %s\n", cursorImage);
			cursorImage[0] = '\0';
			SCR_ChangeCursor(1);
		}
	} else
		cursorImage[0] = '\0';
}

/**
 * @brief Draws the 3D-cursor in battlemode and the icons/info next to it.
 */
static void SCR_DrawCursor (void)
{
	if (scr_showcursor->integer == 0)
		return;

	if (!scr_cursor->integer)
		return;

	if (scr_cursor->modified) {
		scr_cursor->modified = false;
		SCR_TouchPics();
	}

	if (!cursorImage[0])
		return;

	if (!UI_DNDIsDragging()) {
		const char* pic;

		if (!cls.isOurRound() && CL_BattlescapeRunning())
			pic = "pics/cursors/wait";
		else
			pic = cursorImage;

		const image_t* image = R_FindImage(pic, it_pic);
		if (image)
			R_DrawImage(mousePosX - image->width / 2, mousePosY - image->height / 2, image);

		if (IN_GetMouseSpace() == MS_WORLD && CL_BattlescapeRunning()) {
			HUD_UpdateCursor();
		}
	} else {
		UI_DrawCursor();
	}
}


/**
 * @brief Scroll it up or down
 */
void SCR_RunConsole (void)
{
	/* decide on the height of the console */
	if (cls.keyDest == key_console)
		scr_conlines = scr_consize->value;	/* half screen */
	else
		scr_conlines = 0;		/* none visible */

	if (scr_conlines < scr_con_current) {
		scr_con_current -= scr_conspeed->value * cls.frametime;
		if (scr_conlines > scr_con_current)
			scr_con_current = scr_conlines;

	} else if (scr_conlines > scr_con_current) {
		scr_con_current += scr_conspeed->value * cls.frametime;
		if (scr_conlines < scr_con_current)
			scr_con_current = scr_conlines;
	}
}

/**
 * @sa SCR_UpdateScreen
 * @sa Con_DrawConsole
 * @sa Con_DrawNotify
 */
static void SCR_DrawConsole (void)
{
	Con_CheckResize();

	if (!viddef.viewWidth || !viddef.viewHeight) {
		/* active full screen menu */
		/* draw the console like in game */
		if (scr_con_current > 0.0)
			Con_DrawConsole(scr_con_current);
		return;
	}

	if (scr_con_current > 0.0) {
		Con_DrawConsole(scr_con_current);
	}
}

/**
 * @sa SCR_UpdateScreen
 * @sa SCR_EndLoadingPlaque
 * @sa SCR_DrawLoading
 */
void SCR_BeginLoadingPlaque (void)
{
	cls.disableScreen = CL_Milliseconds();
}

/**
 * @sa SCR_BeginLoadingPlaque
 */
void SCR_EndLoadingPlaque (void)
{
	cls.disableScreen = 0;
}

static void SCR_TimeRefresh_f (void)
{
	if (cls.state != ca_active)
		return;

	const int start = Sys_Milliseconds();

	if (Cmd_Argc() == 2) {		/* run without page flipping */
		R_BeginFrame();
		for (int i = 0; i < 128; i++) {
			refdef.viewAngles[1] = i / 128.0 * 360.0;
			r_threadstate.state = THREAD_BSP;
			R_RenderFrame();
		}
		R_EndFrame();
	} else {
		for (int i = 0; i < 128; i++) {
			refdef.viewAngles[1] = i / 128.0 * 360.0;

			R_BeginFrame();
			r_threadstate.state = THREAD_BSP;
			R_RenderFrame();
			R_EndFrame();
		}
	}

	const int stop = Sys_Milliseconds();
	const float time = (stop - start) / 1000.0f;
	Com_Printf("%f seconds (%f fps)\n", time, 128 / time);
}

/**
 * @brief This is called every frame, and can also be called explicitly to flush text to the screen
 * @sa UI_Draw
 * @sa CL_ViewRender
 * @sa SCR_DrawConsole
 * @sa SCR_DrawCursor
 */
void SCR_UpdateScreen (void)
{
	if (cls.waitingForStart)
		return;

	/* if the screen is disabled (loading plaque is up, or vid mode changing)
	 * do nothing at all */
	if (cls.disableScreen) {
		if (CL_Milliseconds() - cls.disableScreen > 120000 && refdef.ready) {
			cls.disableScreen = 0;
			Com_Printf("Loading plaque timed out.\n");
			return;
		}
	}

	/* not initialized yet */
	if (!screenInitialized)
		return;

	R_BeginFrame();

	/* draw scene, if it is need */
	CL_ViewRender();

	/* draw the ui on top of the render view */
	UI_Draw();

	SCR_DrawConsole();

	if (cl_fps->integer)
		SCR_DrawString(viddef.context.width - 20 - con_fontWidth * 10, 0, va("fps: %3.1f", cls.framerate));
	if (scr_rspeed->integer) {
		if (CL_OnBattlescape())
			SCR_DrawString(viddef.context.width - 20 - con_fontWidth * 30, 80, va("brushes: %6i alias: %6i\n", refdef.brushCount, refdef.aliasCount));
		else
			SCR_DrawString(viddef.context.width - 20 - con_fontWidth * 30, 80, va("alias: %6i\n", refdef.aliasCount));

		SCR_DrawString(viddef.context.width - 20 - con_fontWidth * 30, 80 + con_fontHeight, va("batches: %6i\n", refdef.batchCount));
		if (r_programs->integer) {
			SCR_DrawString(viddef.context.width - 20 - con_fontWidth * 30, 80 + con_fontHeight * 2, va("FFP->shader switches:    %6i\n", refdef.FFPToShaderCount));
			SCR_DrawString(viddef.context.width - 20 - con_fontWidth * 30, 80 + con_fontHeight * 3, va("shader->shader switches: %6i\n", refdef.shaderToShaderCount));
			SCR_DrawString(viddef.context.width - 20 - con_fontWidth * 30, 80 + con_fontHeight * 4, va("shader->FFP switches:    %6i\n", refdef.shaderToFFPCount));
		}
	}

	SCR_DrawCursor();

	R_EndFrame();
}

void SCR_ChangeCursor (int cursor)
{
	if (cursor > 0)
		Cvar_SetValue("cursor", cursor);
}

/**
 * @sa CL_Init
 */
void SCR_Init (void)
{
	scr_conspeed = Cvar_GetOrCreate("scr_conspeed", "3", 0, "Console open/close speed");
	scr_consize = Cvar_GetOrCreate("scr_consize", "1.0", 0, "Console size");
	scr_rspeed = Cvar_GetOrCreate("r_speeds", "0", CVAR_ARCHIVE, "Show some rendering stats");
	scr_cursor = Cvar_GetOrCreate("cursor", "1", 0, "Which cursor should be shown - 0-9");
	/** @todo remove me - but this was an archive cvar once */
	scr_cursor->flags = 0;
	scr_showcursor = Cvar_GetOrCreate("scr_showcursor", "1", 0, "Show/hide mouse cursor- 0-1");

	/* register our commands */
	Cmd_AddCommand("timerefresh", SCR_TimeRefresh_f, "Run a benchmark");

	SCR_TouchPics();

	screenInitialized = true;
}

void SCR_Shutdown (void)
{
	screenInitialized = false;
}
