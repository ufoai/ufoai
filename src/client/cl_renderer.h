/**
 * @file
 */

/*
All original material Copyright (C) 2002-2015 UFO: Alien Invasion.

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

#pragma once

#include "renderer/r_image.h"
#include "renderer/r_model.h"
#include "renderer/r_program.h"

#include <SDL.h>

/* e.g. used for sequences and particle editor */
#define RDF_NOWORLDMODEL    1
#define RDF_IRGOGGLES       2	/**< actor is using ir goggles and everything with RF_IRGOGGLES is visible for him */

#define WEATHER_NONE	0
#define WEATHER_FOG 	1

#define VID_NORM_WIDTH		1024
#define VID_NORM_HEIGHT		768

#define MAX_PTL_ART		1024
#define MAX_PTLS		2048

/** @brief coronas are soft, alpha-blended, rounded polys */
typedef struct corona_s {
	vec3_t org;
	float radius;
	vec3_t color;
} corona_t;

#define MAX_CORONAS 		128

#define MAX_GL_LIGHTS 8

typedef struct {
	model_t* model;			/**< the loaded model */
	const char* name;		/**< model path (resolved in the renderer on model loading time) */

	float* origin;			/**< pointer to node/menumodel origin */
	float* angles;			/**< pointer to node/menumodel angles */
	float* scale;			/**< pointer to node/menumodel scale */
	float* center;			/**< pointer to node/menumodel center */

	int frame, oldframe;	/**< animation frames */
	float backlerp;			/**< linear interpolation from previous frame */

	int skin;				/**< skin number */
	int mesh;				/**< which mesh? @note md2 models only have one mesh */
	float* color;
} modelInfo_t;

typedef struct ptlCmd_s {
	byte cmd;	/**< the type of the command - @sa pc_t */
	byte type;	/**< the type of the data referenced by this particle command */
	int ref;	/**< This is the location of the data for this particle command. If negative this is relative
				 * to the particle, otherwise relative to particle command hunk */
} ptlCmd_t;

typedef struct ptlDef_s {
	char name[MAX_VAR];	/**< script id of the particle */
	ptlCmd_t* init;		/**< only called at particle spawn time */
	ptlCmd_t* run;		/**< called every frame */
	ptlCmd_t* think;	/**< depends on the tps value of the particle */
	ptlCmd_t* round;	/**< called for each ended round */
	ptlCmd_t* physics;	/**< called when the particle origin hits something solid */
} ptlDef_t;

/** @brief particle art type */
typedef enum artType_s {
	ART_PIC,
	ART_MODEL
} artType_t;

typedef struct ptlArt_s {
	char name[MAX_VAR];	/**< the path of the particle art - must be the first entry because the parsing code relies on this */
	int frame;
	int skin;			/**< the skin of the model */
	union {
		const image_t* image;
		model_t* model;
	} art;
	artType_t type;		/**< the type of the particle art */
} ptlArt_t;

typedef struct ptl_s {
	bool inuse;				/**< particle active? */
	bool invis;				/**< is this particle invisible */

	r_program_t* program;	/**< this glsl program is bound before the particle is executed */

	ptlArt_t* pic;			/**< Picture link. */
	ptlArt_t* model;		/**< Model link. */

	blend_t blend;			/**< blend mode */
	style_t style;			/**< style mode */
	vec2_t size;
	vec3_t scale;
	vec4_t color;
	vec3_t s;				/**< current position */
	vec3_t origin;			/**< start position - set initial s position to get this value */
	vec3_t offset;
	vec3_t angles;
	vec3_t lightColor;
	float lightIntensity;
	float lightSustain;
	int levelFlags;

	int skin;				/**< model skin to use for this particle */

	struct ptl_s* children;	/**< list of children */
	struct ptl_s* next;		/**< next peer in list */
	struct ptl_s* parent;	/**< pointer to parent */

	/* private */
	ptlDef_t* ctrl;			/**< pointer to the particle definition */
	int startTime;
	int frame, endFrame;
	float fps;				/**< how many frames per second (animate) */
	float lastFrame;		/**< time (in seconds) when the think function was last executed (perhaps this can be used to delay or speed up particle actions). */
	float tps;				/**< think per second - call the think function tps times each second, the first call at 1/tps seconds */
	float lastThink;
	fade_t thinkFade, frameFade;
	float t;		/**< time that the particle has been active already */
	float dt;		/**< time increment for rendering this particle (delta time) */
	float life;		/**< specifies how long a particle will be active (seconds) */
	int rounds;		/**< specifies how many rounds a particle will be active */
	int roundsCnt;
	float scrollS;
	float scrollT;
	vec3_t a;		/**< acceleration vector */
	vec3_t v;		/**< velocity vector */
	vec3_t oldV;	/**< old velocity vector (in case the particle e.g. bounces) */
	vec3_t omega;	/**< the rotation vector for the particle (newAngles = oldAngles + frametime * omega) */
	bool physics;	/**< basic physics */
	bool autohide;	/**< only draw the particle if the current position is
						 * not higher than the current level (useful for weather
						 * particles) */
	bool stayalive;	/**< used for physics particles that hit the ground */
	bool weather;	/**< used to identify weather particles (can be switched
						 * off via cvar cl_particleweather) */
	bool hitSolid;	/**< true if a trace (only physic particles) hits something solid */
	bool stick;		/**< true if a particle sticks to the solid the trace hits before (only physic particles) */
	bool bounce;	/**< true if the particle should bouce when a solid is hot (only physic particles) */
} ptl_t;

typedef struct {
	bool ready;	/**< false if on new level or vid restart - if this is true the map can be rendered */

	float fieldOfViewX, fieldOfViewY;
	vec3_t viewOrigin;
	vec3_t viewAngles;
	float time;					/**< time is used to auto animate */
	int rendererFlags;			/**< RDF_NOWORLDMODEL, etc */
	int worldlevel;
	int brushCount, aliasCount, batchCount;
	int FFPToShaderCount, shaderToShaderCount, shaderToFFPCount;

	int weather;				/**< weather effects */
	vec4_t fogColor;
	vec4_t ambientColor;
	vec4_t modelAmbientColor;	/**< clamped to avoid black models */
	vec4_t sunDiffuseColor;
	vec4_t sunSpecularColor;
	vec4_t sunVector;			/**< pointing towards the sun, should be x y z 0 to match the OpengGL logic */

	/* entity, dynamic lights and corona lists are repopulated each frame, don't use them as persistent */
	int numEntities;
	int numDynamicLights;
	light_t dynamicLights[MAX_GL_LIGHTS];

	/* static lights (populated when loading the world) */
	light_t staticLights[MAX_STATIC_LIGHTS];
	int numStaticLights;

	int numCoronas;
	corona_t coronas[MAX_CORONAS];

	trace_t trace;				/**< occlusion testing */
	struct entity_s* traceEntity;

	mapTiles_t* mapTiles;
} rendererData_t;

extern rendererData_t refdef;

/* threading state */
typedef enum {
	THREAD_DEAD,
	THREAD_IDLE,
	THREAD_CLIENT,
	THREAD_BSP,
	THREAD_RENDERER
} threadstate_t;

typedef enum {
	LONGLINES_WRAP,
	LONGLINES_CHOP,
	LONGLINES_PRETTYCHOP,

	LONGLINES_LAST
} longlines_t;

typedef struct renderer_threadstate_s {
	SDL_Thread* thread;
	threadstate_t state;
} renderer_threadstate_t;

extern renderer_threadstate_t r_threadstate;

void R_Color(const vec4_t rgba);

void R_ModBeginLoading(const char* tiles, bool day, const char* pos, const char* mapName, const char* mapZone);
void R_SwitchModelMemPoolTag(void);

void R_LoadImage(const char* name, byte** pic, int* width, int* height);

void R_FontShutdown(void);
void R_FontInit(void);
void R_FontRegister(const char* name, int size, const char* path, const char* style);
void R_FontSetTruncationMarker(const char* marker);

void R_FontTextSize(const char* fontId, const char* text, int maxWidth, longlines_t method, int* width, int* height, int* lines, bool* isTruncated);
int R_FontDrawString(const char* fontId, align_t align, int x, int y, int absX, int maxWidth, int lineHeight, const char* c, int boxHeight, int scrollPos, int* curLine, longlines_t method);
