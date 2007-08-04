/**
 * @file gl_rmain.c
 * @brief
 */

/*
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

#include "gl_local.h"

#if defined DEBUG && defined _MSC_VER
#include <intrin.h>
#endif

viddef_t vid;

refimport_t ri;

GLenum gl_texture0, gl_texture1, gl_texture2, gl_texture3;
GLenum gl_combine;

float gldepthmin, gldepthmax;

glconfig_t gl_config;
glstate_t gl_state;

image_t *r_notexture;			/* use for bad textures */
image_t *r_particletexture;		/* little dot for particles */

entity_t *currententity;
model_t *currentmodel;

cBspPlane_t frustum[4];

int r_framecount;				/* used for dlight push checking */

int c_brush_polys, c_alias_polys;

float v_blend[4];				/* final blending color */

/* entity transform */
transform_t trafo[MAX_ENTITIES];

/* view origin */
vec3_t vup;
vec3_t vpn;
vec3_t vright;
vec3_t r_origin;

float r_world_matrix[16];
float r_base_world_matrix[16];

/* screen size info */
refdef_t r_newrefdef;

static cvar_t *r_norefresh;
static cvar_t *r_drawentities;
static cvar_t *r_speeds;
cvar_t *r_drawworld;
cvar_t *r_nocull;
cvar_t *r_isometric;
cvar_t *r_lerpmodels;
cvar_t *r_displayrefresh;
cvar_t *r_anisotropic;
cvar_t *r_ext_max_anisotropy;
cvar_t *r_texture_lod;			/* lod_bias */

cvar_t *gl_screenshot;
cvar_t *gl_screenshot_jpeg_quality;

cvar_t *gl_particle_min_size;
cvar_t *gl_particle_max_size;
cvar_t *gl_particle_size;
cvar_t *gl_particle_att_a;
cvar_t *gl_particle_att_b;
cvar_t *gl_particle_att_c;

cvar_t *gl_ext_swapinterval;
cvar_t *gl_ext_multitexture;
cvar_t *gl_ext_combine;
cvar_t *gl_ext_lockarrays;
cvar_t *gl_ext_texture_compression;
cvar_t *gl_ext_s3tc_compression;

cvar_t *gl_log;
cvar_t *gl_bitdepth;
cvar_t *gl_drawbuffer;
cvar_t *gl_driver;
cvar_t *gl_lightmap;
cvar_t *gl_shadows;
cvar_t *gl_shadow_debug_volume;
cvar_t *gl_shadow_debug_shade;
cvar_t *gl_ati_separate_stencil;
cvar_t *gl_stencil_two_side;

cvar_t *gl_drawclouds;
cvar_t *gl_imagefilter;
cvar_t *gl_mode;
cvar_t *gl_dynamic;
cvar_t *gl_monolightmap;
cvar_t *gl_modulate;
cvar_t *gl_round_down;
cvar_t *gl_picmip;
cvar_t *gl_maxtexres;
cvar_t *gl_showtris;
cvar_t *gl_finish;
cvar_t *gl_flashblend;
cvar_t *gl_saturatelighting;
cvar_t *gl_swapinterval;
cvar_t *gl_texturemode;
cvar_t *gl_texturealphamode;
cvar_t *gl_texturesolidmode;
cvar_t *gl_wire;
cvar_t *gl_fog;
cvar_t *gl_showbox;

cvar_t *gl_3dmapradius;

cvar_t *vid_fullscreen;
cvar_t *vid_gamma;
cvar_t *vid_ref;
cvar_t *vid_grabmouse;

cvar_t *con_font;
cvar_t *con_fontWidth;
cvar_t *con_fontHeight;
cvar_t *con_fontShift;

/**
 * @brief Reset to initial state
 */
static void GL_Reset_f (void)
{
	GL_EnableMultitexture(qfalse);
	GLSTATE_DISABLE_ALPHATEST
	GLSTATE_DISABLE_BLEND
}

/**
 * @brief Prints some OpenGL strings
 */
static void GL_Strings_f (void)
{
	ri.Con_Printf(PRINT_ALL, "GL_VENDOR: %s\n", gl_config.vendor_string);
	ri.Con_Printf(PRINT_ALL, "GL_RENDERER: %s\n", gl_config.renderer_string);
	ri.Con_Printf(PRINT_ALL, "GL_VERSION: %s\n", gl_config.version_string);
	ri.Con_Printf(PRINT_ALL, "MODE: %i, %d x %d FULLSCREEN: %i\n", gl_mode->integer, vid.width, vid.height, vid_fullscreen->integer);
	ri.Con_Printf(PRINT_ALL, "GL_EXTENSIONS: %s\n", gl_config.extensions_string);
	ri.Con_Printf(PRINT_ALL, "GL_MAX_TEXTURE_SIZE: %d\n", gl_config.maxTextureSize);
}

/**
 * @brief
 * @sa R_DrawShadowVolume
 * @sa R_DrawShadow
 */
static void R_CastShadow (void)
{
	int i;

	/* no shadows at all */
	if (!gl_shadows->integer)
		return;

	for (i = 0; i < r_newrefdef.num_entities; i++) {
		currententity = &r_newrefdef.entities[i];
		currentmodel = currententity->model;
		if (!currentmodel)
			continue;
		if (currentmodel->type != mod_alias_md2 && currentmodel->type != mod_alias_md3)
			continue;

		if (gl_shadows->integer == 2)
			R_DrawShadowVolume(currententity);
		else if (gl_shadows->integer == 1)
			R_DrawShadow(currententity);
	}
}

/**
 * @brief Returns true if the box is completely outside the frustum
 */
qboolean R_CullBox (vec3_t mins, vec3_t maxs)
{
	int i;

	if (r_nocull->integer)
		return qfalse;

	for (i = 0; i < 4; i++)
		if (BoxOnPlaneSide(mins, maxs, &frustum[i]) == 2)
			return qtrue;
	return qfalse;
}


/**
 * @brief
 */
void R_RotateForEntity (entity_t * e)
{
	qglTranslatef(e->origin[0], e->origin[1], e->origin[2]);

	qglRotatef(e->angles[1], 0, 0, 1);
	qglRotatef(-e->angles[0], 0, 1, 0);
	qglRotatef(-e->angles[2], 1, 0, 0);
}


/**
 * @brief
 */
void R_InterpolateTransform (animState_t * as, int numframes, float *tag, float *interpolated)
{
	float *current, *old;
	float bl, fl;
	int i;

	/* range check */
	if (as->frame >= numframes || as->frame < 0)
		as->frame = 0;
	if (as->oldframe >= numframes || as->oldframe < 0)
		as->oldframe = 0;

	/* calc relevant values */
	current = tag + as->frame * 16;
	old = tag + as->oldframe * 16;
	bl = as->backlerp;
	fl = 1.0 - as->backlerp;

	/* right on a frame? */
	if (bl == 0.0) {
		memcpy(interpolated, current, sizeof(float) * 16);
		return;
	}
	if (bl == 1.0) {
		memcpy(interpolated, old, sizeof(float) * 16);
		return;
	}

	/* interpolate */
	for (i = 0; i < 16; i++)
		interpolated[i] = fl * current[i] + bl * old[i];

	/* normalize */
	for (i = 0; i < 3; i++)
		VectorNormalize(interpolated + i * 4);
}


/**
 * @brief
 */
static float *R_CalcTransform (entity_t * e)
{
	vec3_t angles;
	transform_t *t;
	float *mp;
	float mt[16], mc[16];
	int i;

	/* check if this entity is already transformed */
	t = &trafo[e - r_newrefdef.entities];

	if (t->processing)
		ri.Sys_Error(ERR_DROP, "Ring in entity transformations!\n");

	if (t->done)
		return t->matrix;

	/* process this matrix */
	t->processing = qtrue;
	mp = NULL;

	/* do parent object trafos first */
	if (e->tagent) {
		/* parent trafo */
		mp = R_CalcTransform(e->tagent);

		/* tag trafo */
		if (e->tagent->model && e->tagent->model->alias.tagdata) {
			dtag_t *taghdr;
			char *name;
			float *tag;
			float interpolated[16];

			taghdr = (dtag_t *) e->tagent->model->alias.tagdata;

			/* find the right tag */
			name = (char *) taghdr + taghdr->ofs_names;
			for (i = 0; i < taghdr->num_tags; i++, name += MD2_MAX_TAGNAME)
				if (!strcmp(name, e->tagname)) {
					/* found the tag (matrix) */
					tag = (float *) ((byte *) taghdr + taghdr->ofs_tags);
					tag += i * 16 * taghdr->num_frames;

					/* do interpolation */
					R_InterpolateTransform(&e->tagent->as, taghdr->num_frames, tag, interpolated);

					/* transform */
					GLMatrixMultiply(mp, interpolated, mt);
					mp = mt;

					break;
				}
		}
	}

	/* fill in edge values */
	mc[3] = mc[7] = mc[11] = 0.0;
	mc[15] = 1.0;

	/* add rotation */
	VectorCopy(e->angles, angles);
/*	angles[YAW] = -angles[YAW]; */

	AngleVectors(angles, &mc[0], &mc[4], &mc[8]);

	/* add translation */
	mc[12] = e->origin[0];
	mc[13] = e->origin[1];
	mc[14] = e->origin[2];

	/* flip an axis */
	VectorScale(&mc[4], -1, &mc[4]);

	/* combine trafos */
	if (mp)
		GLMatrixMultiply(mp, mc, t->matrix);
	else
		memcpy(t->matrix, mc, sizeof(float) * 16);

	/* we're done */
	t->done = qtrue;
	t->processing = qfalse;

	return t->matrix;
}


/**
 * @brief
 */
static void R_TransformEntitiesOnList (void)
{
	int i;

	if (!r_drawentities->integer)
		return;

	/* clear flags */
	for (i = 0; i < r_newrefdef.num_entities; i++) {
		trafo[i].done = qfalse;
		trafo[i].processing = qfalse;
	}

	/* calculate all transformations */
	/* possibly recursive */
	for (i = 0; i < r_newrefdef.num_entities; i++)
		R_CalcTransform(&r_newrefdef.entities[i]);
}


/**
 * @brief
 */
static void R_DrawEntitiesOnList (void)
{
	int i;

	if (!r_drawentities->integer)
		return;

	/* draw non-transparent first */

	for (i = 0; i < r_newrefdef.num_entities; i++) {
		currententity = &r_newrefdef.entities[i];

		/* find out if and how an entity should be drawn */
		if (currententity->flags & RF_TRANSLUCENT)
			continue;			/* solid */

		if (currententity->flags & RF_BOX)
			R_DrawBox(currententity);
		else {
			currentmodel = currententity->model;
			if (!currentmodel) {
				Mod_DrawNullModel();
				continue;
			}
			switch (currentmodel->type) {
			case mod_alias_md2:
				/* MD2 model */
				R_DrawAliasMD2Model(currententity);
				break;
			case mod_alias_md3:
				/* MD3 model */
				R_DrawAliasMD3Model(currententity);
				break;
			case mod_brush:
				/* draw things like func_breakable */
				R_DrawBrushModel(currententity);
				break;
			case mod_sprite:
				R_DrawSpriteModel(currententity);
				break;
			case mod_obj:
				R_DrawOBJModel(currententity);
				break;
			default:
				ri.Sys_Error(ERR_DROP, "Bad %s modeltype: %i", currentmodel->name, currentmodel->type);
				break;
			}
		}
	}

	/* draw transparent entities */
	/* we could sort these if it ever becomes a problem... */
	qglDepthMask(0);			/* no z writes */
	for (i = 0; i < r_newrefdef.num_entities; i++) {
		currententity = &r_newrefdef.entities[i];
		if (!(currententity->flags & RF_TRANSLUCENT))
			continue;			/* solid */

		if (currententity->flags & RF_BOX)
			R_DrawBox(currententity);
		else {
			currentmodel = currententity->model;
			if (!currentmodel) {
				Mod_DrawNullModel();
				continue;
			}
			switch (currentmodel->type) {
			case mod_alias_md2:
				/* MD2 model */
				R_DrawAliasMD2Model(currententity);
				break;
			case mod_alias_md3:
				/* MD3 model */
				R_DrawAliasMD3Model(currententity);
				break;
			case mod_brush:
				R_DrawBrushModel(currententity);
				break;
			case mod_sprite:
				R_DrawSpriteModel(currententity);
				break;
			default:
				ri.Sys_Error(ERR_DROP, "Bad %s modeltype: %i", currentmodel->name, currentmodel->type);
				break;
			}
		}
	}
	qglDepthMask(1);			/* back to writing */
}

/**
 * @brief
 */
static int SignbitsForPlane (cBspPlane_t * out)
{
	int bits, j;

	/* for fast box on planeside test */
	bits = 0;
	for (j = 0; j < 3; j++) {
		if (out->normal[j] < 0)
			bits |= 1 << j;
	}
	return bits;
}

/**
 * @brief
 */
static void R_SetFrustum (void)
{
	int i;

	if (r_isometric->integer) {
		/* 4 planes of a cube */
		VectorScale(vright, +1, frustum[0].normal);
		VectorScale(vright, -1, frustum[1].normal);
		VectorScale(vup, +1, frustum[2].normal);
		VectorScale(vup, -1, frustum[3].normal);

		for (i = 0; i < 4; i++) {
			frustum[i].type = PLANE_ANYZ;
			frustum[i].dist = DotProduct(r_origin, frustum[i].normal);
			frustum[i].signbits = SignbitsForPlane(&frustum[i]);
		}
		frustum[0].dist -= 10 * r_newrefdef.fov_x;
		frustum[1].dist -= 10 * r_newrefdef.fov_x;
		frustum[2].dist -= 10 * r_newrefdef.fov_x * ((float) r_newrefdef.height / r_newrefdef.width);
		frustum[3].dist -= 10 * r_newrefdef.fov_x * ((float) r_newrefdef.height / r_newrefdef.width);
	} else {
		/* rotate VPN right by FOV_X/2 degrees */
		RotatePointAroundVector(frustum[0].normal, vup, vpn, -(90 - r_newrefdef.fov_x / 2));
		/* rotate VPN left by FOV_X/2 degrees */
		RotatePointAroundVector(frustum[1].normal, vup, vpn, 90 - r_newrefdef.fov_x / 2);
		/* rotate VPN up by FOV_X/2 degrees */
		RotatePointAroundVector(frustum[2].normal, vright, vpn, 90 - r_newrefdef.fov_y / 2);
		/* rotate VPN down by FOV_X/2 degrees */
		RotatePointAroundVector(frustum[3].normal, vright, vpn, -(90 - r_newrefdef.fov_y / 2));

		for (i = 0; i < 4; i++) {
			frustum[i].type = PLANE_ANYZ;
			frustum[i].dist = DotProduct(r_origin, frustum[i].normal);
			frustum[i].signbits = SignbitsForPlane(&frustum[i]);
		}
	}
}

/**
 * @brief
 */
static void R_SetupFrame (void)
{
	int i;

	r_framecount++;

	/* build the transformation matrix for the given view angles */
	FastVectorCopy(r_newrefdef.vieworg, r_origin);

	AngleVectors(r_newrefdef.viewangles, vpn, vright, vup);

	for (i = 0; i < 4; i++)
		v_blend[i] = r_newrefdef.blend[i];

	c_brush_polys = 0;
	c_alias_polys = 0;

	/* clear out the portion of the screen that the NOWORLDMODEL defines */
	if (r_newrefdef.rdflags & RDF_NOWORLDMODEL) {
		qglEnable(GL_SCISSOR_TEST);
		qglScissor(r_newrefdef.x, vid.height - r_newrefdef.height - r_newrefdef.y, r_newrefdef.width, r_newrefdef.height);
		qglClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		qglDisable(GL_SCISSOR_TEST);
	}
}

/**
 * @brief
 */
static void MYgluPerspective (GLdouble zNear, GLdouble zFar)
{
	GLdouble xmin, xmax, ymin, ymax, yaspect = (double) r_newrefdef.height / r_newrefdef.width;

	if (r_isometric->integer) {
		/* @todo: someone with stereo glasses should try to figure out what the correct stereo separation multiplier is for ortho mode */
		qglOrtho((-10 - 2 * gl_state.camera_separation) * r_newrefdef.fov_x, (10 - 2 * gl_state.camera_separation) * r_newrefdef.fov_x, -10 * r_newrefdef.fov_x * yaspect, 10 * r_newrefdef.fov_x * yaspect, -zFar, zFar);
	} else {
		xmax = zNear * tan(r_newrefdef.fov_x * M_PI / 360.0);
		xmin = -xmax;

		ymin = xmin * yaspect;
		ymax = xmax * yaspect;

		xmin += -(2 * gl_state.camera_separation) / zNear;
		xmax += -(2 * gl_state.camera_separation) / zNear;

		qglFrustum(xmin, xmax, ymin, ymax, zNear, zFar);
	}
}


/**
 * @brief
 */
static void R_SetupGL (void)
{
	int x, x2, y2, y, w, h;

	/* set up viewport */
	x = floor(r_newrefdef.x * vid.width / vid.width);
	x2 = ceil((r_newrefdef.x + r_newrefdef.width) * vid.width / vid.width);
	y = floor(vid.height - r_newrefdef.y * vid.height / vid.height);
	y2 = ceil(vid.height - (r_newrefdef.y + r_newrefdef.height) * vid.height / vid.height);

	w = x2 - x;
	h = y - y2;

	qglViewport(x, y2, w, h);

	/* set up projection matrix */
	qglMatrixMode(GL_PROJECTION);
	qglLoadIdentity();
	MYgluPerspective(4.0, 4096.0);

	qglCullFace(GL_FRONT);

	qglMatrixMode(GL_MODELVIEW);
	qglLoadIdentity();

	qglRotatef(-90, 1, 0, 0);	/* put Z going up */
	qglRotatef(90, 0, 0, 1);	/* put Z going up */
	qglRotatef(-r_newrefdef.viewangles[2], 1, 0, 0);
	qglRotatef(-r_newrefdef.viewangles[0], 0, 1, 0);
	qglRotatef(-r_newrefdef.viewangles[1], 0, 0, 1);
	qglTranslatef(-r_newrefdef.vieworg[0], -r_newrefdef.vieworg[1], -r_newrefdef.vieworg[2]);

	qglGetFloatv(GL_MODELVIEW_MATRIX, r_world_matrix);

	/* set drawing parms */
	qglEnable(GL_CULL_FACE);

	GLSTATE_DISABLE_BLEND
	GLSTATE_DISABLE_ALPHATEST
	qglEnable(GL_DEPTH_TEST);

#if 0
	qglHint(GL_LINE_SMOOTH_HINT, GL_DONT_CARE);
	qglHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_DONT_CARE);
#endif

	if (gl_fog->integer && r_newrefdef.fog && gl_state.fog_coord) {
		qglEnable(GL_FOG);
		qglFogi(GL_FOG_MODE, GL_EXP2);
		qglFogf(GL_FOG_START, 0.1 * r_newrefdef.fog);
		qglFogf(GL_FOG_END, r_newrefdef.fog);
		qglHint(GL_FOG_HINT, GL_DONT_CARE);
		qglFogf(GL_FOG_DENSITY, 0.005 * r_newrefdef.fog );
		qglFogfv(GL_FOG_COLOR, r_newrefdef.fogColor);
	}
}

/**
 * @brief
 */
static void R_Clear (void)
{
	qglClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	gldepthmin = 0;
	gldepthmax = 1;
	qglDepthFunc(GL_LEQUAL);

	qglDepthRange(gldepthmin, gldepthmax);

	if (gl_shadows->integer == 2) {
		/* set the reference stencil value */
		qglClearStencil(1);
		/* reset stencil buffer */
		qglClear(GL_STENCIL_BUFFER_BIT);
	}
}

/**
 * @brief
 */
static void R_Flash (void)
{
	R_ShadowBlend();
}

/**
 * @brief r_newrefdef must be set before the first call
 */
static void R_SetRefreshDefinition (refdef_t * fd)
{
	if (r_norefresh->integer)
		return;

	r_newrefdef = *fd;
}

/**
 * @brief r_newrefdef must be set before the first call
 */
static void R_RenderView (refdef_t * fd)
{
	if (r_norefresh->integer)
		return;

	r_newrefdef = *fd;

/*	if (!r_worldmodel && !(r_newrefdef.rdflags & RDF_NOWORLDMODEL)) */
/*		ri.Sys_Error(ERR_DROP, "R_RenderView: NULL worldmodel"); */

	if (r_speeds->integer) {
		c_brush_polys = 0;
		c_alias_polys = 0;
	}

	if (gl_finish->integer)
		qglFinish();

	R_SetupFrame();

	R_SetFrustum();

	R_SetupGL();

	if (gl_wire->integer)
		qglPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	/* draw brushes on current worldlevel */
	R_DrawLevelBrushes();
	R_DrawTriangleOutlines();

	R_TransformEntitiesOnList();
	R_DrawEntitiesOnList();

	if (gl_shadows->integer == 2)
		R_CastShadow();

	if (gl_wire->integer)
		qglPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	R_DrawAlphaSurfaces();

	if (gl_shadows->integer == 1)
		R_CastShadow();

	R_DrawPtls();

	R_RenderDlights();

	R_Flash();

	/* apply 2d bloom effect */
	GL_BloomBlend();
}

#if 0
/**
 * @brief
 * @note Currently unused
 * @sa R_SetGL2D
 */
void R_LeaveGL2D(void)
{
	qglMatrixMode(GL_MODELVIEW);
	qglPopMatrix();

	qglMatrixMode(GL_PROJECTION);
	qglPopMatrix();

	qglPopAttrib();
}
#endif

/**
 * @brief
 * @sa R_LeaveGL2D
 */
static void R_SetGL2D (void)
{
	/* set 2D virtual screen size */
	qglViewport(0, 0, vid.width, vid.height);
	qglMatrixMode(GL_PROJECTION);
	qglLoadIdentity();
	qglOrtho(0, vid.width, vid.height, 0, 9999, -9999);
	qglMatrixMode(GL_MODELVIEW);
	qglLoadIdentity();
	qglDisable(GL_DEPTH_TEST);
	qglDisable(GL_CULL_FACE);
	GLSTATE_DISABLE_BLEND
	qglDisable(GL_FOG);
	GLSTATE_ENABLE_ALPHATEST
	qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	GL_TexEnv(GL_MODULATE);
	qglColor4f(1, 1, 1, 1);
}

/**
 * @brief
 * @sa R_BeginFrame
 * @sa GLimp_EndFrame
 * @sa GLimp_StartFrame
 */
static void R_RenderFrame (refdef_t * fd)
{
	R_RenderView(fd);
	R_SetGL2D();

	if (r_speeds->integer) {
		fd->c_brush_polys = c_brush_polys;
		fd->c_alias_polys = c_alias_polys;
	}
}

static const cmdList_t r_commands[] = {
	{"imagelist", GL_ImageList_f, NULL},
	{"fontcachelist", Font_ListCache_f, NULL},
	{"screenshot", GL_ScreenShot_f, "Take a screenshot"},
	{"modellist", Mod_Modellist_f, NULL},
	{"gl_strings", GL_Strings_f, NULL},
	{"gl_reset", GL_Reset_f, "Reset to initial state"},

	{NULL, NULL, NULL}
};

/**
 * @brief
 */
static void R_Register (void)
{
	const cmdList_t *commands;

	r_norefresh = ri.Cvar_Get("r_norefresh", "0", 0, "Fix the screen to the last thing you saw. Only used for debugging.");
	r_drawentities = ri.Cvar_Get("r_drawentities", "1", 0, NULL);
	r_drawworld = ri.Cvar_Get("r_drawworld", "1", 0, NULL);
	r_isometric = ri.Cvar_Get("r_isometric", "0", CVAR_ARCHIVE, "Draw the world in isometric mode");
	r_lerpmodels = ri.Cvar_Get("r_lerpmodels", "1", 0, NULL);
	r_nocull = ri.Cvar_Get("r_nocull", "0", 0, NULL);
	r_speeds = ri.Cvar_Get("r_speeds", "0", 0, NULL);
	r_displayrefresh = ri.Cvar_Get("r_displayrefresh", "0", CVAR_ARCHIVE, NULL);
	r_anisotropic = ri.Cvar_Get("r_anisotropic", "1", CVAR_ARCHIVE, NULL);
	r_ext_max_anisotropy = ri.Cvar_Get("r_ext_max_anisotropy", "0", 0, NULL);
	r_texture_lod = ri.Cvar_Get("r_texture_lod", "0", CVAR_ARCHIVE, NULL);

	gl_screenshot = ri.Cvar_Get("gl_screenshot", "jpg", CVAR_ARCHIVE, "png, jpg or tga are valid screenshot formats");
	gl_screenshot_jpeg_quality = ri.Cvar_Get("gl_screenshot_jpeg_quality", "75", CVAR_ARCHIVE, "jpeg quality in percent for jpeg screenshots");

	gl_particle_min_size = ri.Cvar_Get("gl_particle_min_size", "2", CVAR_ARCHIVE, NULL);
	gl_particle_max_size = ri.Cvar_Get("gl_particle_max_size", "40", CVAR_ARCHIVE, NULL);
	gl_particle_size = ri.Cvar_Get("gl_particle_size", "40", CVAR_ARCHIVE, NULL);
	gl_particle_att_a = ri.Cvar_Get("gl_particle_att_a", "0.01", CVAR_ARCHIVE, NULL);
	gl_particle_att_b = ri.Cvar_Get("gl_particle_att_b", "0.0", CVAR_ARCHIVE, NULL);
	gl_particle_att_c = ri.Cvar_Get("gl_particle_att_c", "0.01", CVAR_ARCHIVE, NULL);

	gl_modulate = ri.Cvar_Get("gl_modulate", "1", CVAR_ARCHIVE, NULL);
	gl_log = ri.Cvar_Get("gl_log", "0", 0, NULL);
	gl_bitdepth = ri.Cvar_Get("gl_bitdepth", "0", CVAR_ARCHIVE, NULL);
	gl_mode = ri.Cvar_Get("gl_mode", "6", CVAR_ARCHIVE, "Display resolution");
	gl_lightmap = ri.Cvar_Get("gl_lightmap", "0", 0, NULL);
	gl_shadows = ri.Cvar_Get("gl_shadows", "1", CVAR_ARCHIVE, NULL);
	gl_shadow_debug_volume = ri.Cvar_Get("gl_shadow_debug_volume", "0", CVAR_ARCHIVE, NULL);
	gl_shadow_debug_shade = ri.Cvar_Get("gl_shadow_debug_shade", "0", CVAR_ARCHIVE, NULL);
	gl_ati_separate_stencil = ri.Cvar_Get("gl_ati_separate_stencil", "1", CVAR_ARCHIVE, NULL);
	gl_stencil_two_side = ri.Cvar_Get("gl_stencil_two_side", "1", CVAR_ARCHIVE, NULL);
	gl_drawclouds = ri.Cvar_Get("gl_drawclouds", "0", CVAR_ARCHIVE, NULL);
	gl_imagefilter = ri.Cvar_Get("gl_imagefilter", "1", CVAR_ARCHIVE, NULL);
	gl_dynamic = ri.Cvar_Get("gl_dynamic", "1", 0, "Render dynamic lightmaps");
	gl_round_down = ri.Cvar_Get("gl_round_down", "1", 0, NULL);
	gl_picmip = ri.Cvar_Get("gl_picmip", "0", 0, NULL);
	gl_maxtexres = ri.Cvar_Get("gl_maxtexres", "2048", CVAR_ARCHIVE, NULL);
	gl_showtris = ri.Cvar_Get("gl_showtris", "0", 0, NULL);
	gl_finish = ri.Cvar_Get("gl_finish", "0", CVAR_ARCHIVE, NULL);
	gl_flashblend = ri.Cvar_Get("gl_flashblend", "0", 0, "Controls the way dynamic lights are drawn");
	gl_monolightmap = ri.Cvar_Get("gl_monolightmap", "0", 0, NULL);
#if defined(_WIN32)
	gl_driver = ri.Cvar_Get("gl_driver", "opengl32", CVAR_ARCHIVE, NULL);
#elif defined (__APPLE__) || defined (MACOSX)
	gl_driver = ri.Cvar_Get("gl_driver", "/System/Library/Frameworks/OpenGL.framework/Libraries/libGL.dylib", CVAR_ARCHIVE, NULL);
#else
	gl_driver = ri.Cvar_Get("gl_driver", "libGL.so", CVAR_ARCHIVE, NULL);
#endif
	gl_texturemode = ri.Cvar_Get("gl_texturemode", "GL_LINEAR_MIPMAP_NEAREST", CVAR_ARCHIVE, NULL);
	gl_texturealphamode = ri.Cvar_Get("gl_texturealphamode", "default", CVAR_ARCHIVE, NULL);
	gl_texturesolidmode = ri.Cvar_Get("gl_texturesolidmode", "default", CVAR_ARCHIVE, NULL);
	gl_wire = ri.Cvar_Get("gl_wire", "0", CVAR_CHEAT, "Draw the scene in wireframe mode");
	gl_fog = ri.Cvar_Get("gl_fog", "1", CVAR_ARCHIVE, NULL);
	gl_showbox = ri.Cvar_Get("gl_showbox", "0", CVAR_ARCHIVE, "Shows model bounding box");

	gl_ext_swapinterval = ri.Cvar_Get("gl_ext_swapinterval", "1", CVAR_ARCHIVE, NULL);
	gl_ext_multitexture = ri.Cvar_Get("gl_ext_multitexture", "1", CVAR_ARCHIVE, NULL);
	gl_ext_combine = ri.Cvar_Get("gl_ext_combine", "1", CVAR_ARCHIVE, NULL);
	gl_ext_lockarrays = ri.Cvar_Get("gl_ext_lockarrays", "0", CVAR_ARCHIVE, NULL);
	gl_ext_texture_compression = ri.Cvar_Get("gl_ext_texture_compression", "0", CVAR_ARCHIVE, NULL);
	gl_ext_s3tc_compression = ri.Cvar_Get("gl_ext_s3tc_compression", "1", CVAR_ARCHIVE, NULL);

	gl_drawbuffer = ri.Cvar_Get("gl_drawbuffer", "GL_BACK", 0, NULL);
	gl_swapinterval = ri.Cvar_Get("gl_swapinterval", "1", CVAR_ARCHIVE, NULL);

	gl_saturatelighting = ri.Cvar_Get("gl_saturatelighting", "0", 0, NULL);

	gl_3dmapradius = ri.Cvar_Get("gl_3dmapradius", "8192.0", CVAR_NOSET, NULL);

	vid_fullscreen = ri.Cvar_Get("vid_fullscreen", "0", CVAR_ARCHIVE, NULL);
	vid_gamma = ri.Cvar_Get("vid_gamma", "1.0", CVAR_ARCHIVE, NULL);
#if defined(_WIN32)
	vid_ref = ri.Cvar_Get("vid_ref", "gl", CVAR_ARCHIVE, NULL);
#elif defined (__APPLE__) || defined (MACOSX)
	vid_ref = ri.Cvar_Get("vid_ref", "sdl", CVAR_ARCHIVE, NULL);
#else
	vid_ref = ri.Cvar_Get("vid_ref", "glx", CVAR_ARCHIVE, NULL);
#endif
	vid_grabmouse = ri.Cvar_Get("vid_grabmouse", "1", CVAR_ARCHIVE, NULL);
	vid_grabmouse->modified = qfalse;

	con_font = ri.Cvar_Get("con_font", "0", CVAR_ARCHIVE, NULL);
	con_fontWidth = ri.Cvar_Get("con_fontWidth", "8", CVAR_NOSET, NULL);
	con_fontHeight = ri.Cvar_Get("con_fontHeight", "16", CVAR_NOSET, NULL);
	con_fontShift = ri.Cvar_Get("con_fontShift", "3", CVAR_NOSET, NULL);

	for (commands = r_commands; commands->name; commands++)
		ri.Cmd_AddCommand(commands->name, commands->function, commands->description);
}

/**
 * @brief
 */
static qboolean R_SetMode (void)
{
	rserr_t err;
	qboolean fullscreen;

	fullscreen = vid_fullscreen->integer;

	vid_fullscreen->modified = qfalse;
	gl_mode->modified = qfalse;
	gl_ext_texture_compression->modified = qfalse;

	if ((err = GLimp_SetMode(&vid.width, &vid.height, gl_mode->integer, fullscreen)) == rserr_ok)
		gl_state.prev_mode = gl_mode->integer;
	else {
		if (err == rserr_invalid_fullscreen) {
			ri.Cvar_SetValue("vid_fullscreen", 0);
			vid_fullscreen->modified = qfalse;
			ri.Con_Printf(PRINT_ALL, "ref_gl::R_SetMode() - fullscreen unavailable in this mode\n");
			if ((err = GLimp_SetMode(&vid.width, &vid.height, gl_mode->integer, qfalse)) == rserr_ok)
				return qtrue;
		} else if (err == rserr_invalid_mode) {
			ri.Cvar_SetValue("gl_mode", gl_state.prev_mode);
			gl_mode->modified = qfalse;
			ri.Con_Printf(PRINT_ALL, "ref_gl::R_SetMode() - invalid mode\n");
		}

		/* try setting it back to something safe */
		if ((err = GLimp_SetMode(&vid.width, &vid.height, gl_state.prev_mode, qfalse)) != rserr_ok) {
			ri.Con_Printf(PRINT_ALL, "ref_gl::R_SetMode() - could not revert to safe mode\n");
			return qfalse;
		}
	}
	return qtrue;
}

/**
 * @brief
 */
static qboolean R_Init (HINSTANCE hinstance, WNDPROC wndproc)
{
	char renderer_buffer[1000];
	char vendor_buffer[1000];
	int j;
	extern float r_turbsin[256];
	int aniso_level, max_aniso;

	for (j = 0; j < 256; j++)
		r_turbsin[j] *= 0.5;

	ri.Con_Printf(PRINT_ALL, "ref_gl version: "REF_VERSION"\n");

	R_Register();

	/* initialize our QGL dynamic bindings */
	if (!QGL_Init(gl_driver->string)) {
		QGL_Shutdown();
		ri.Con_Printf(PRINT_ALL, "ref_gl::R_Init() - could not load \"%s\"\n", gl_driver->string);
		return qfalse;
	}

	/* initialize OS-specific parts of OpenGL */
	if (!GLimp_Init(hinstance, wndproc)) {
		QGL_Shutdown();
		return qfalse;
	}

	/* set our "safe" modes */
	gl_state.prev_mode = 3;

	/* create the window and set up the context */
	if (!R_SetMode()) {
		QGL_Shutdown();
		ri.Con_Printf(PRINT_ALL, "ref_gl::R_Init() - could not R_SetMode()\n");
		return qfalse;
	}

	/* get our various GL strings */
	gl_config.vendor_string = (const char *)qglGetString (GL_VENDOR);
	ri.Con_Printf(PRINT_ALL, "GL_VENDOR: %s\n", gl_config.vendor_string);
	gl_config.renderer_string = (const char *)qglGetString (GL_RENDERER);
	ri.Con_Printf(PRINT_ALL, "GL_RENDERER: %s\n", gl_config.renderer_string);
	gl_config.version_string = (const char *)qglGetString (GL_VERSION);
	ri.Con_Printf(PRINT_ALL, "GL_VERSION: %s\n", gl_config.version_string);
	gl_config.extensions_string = (const char *)qglGetString (GL_EXTENSIONS);
	ri.Con_Printf(PRINT_ALL, "GL_EXTENSIONS: %s\n", gl_config.extensions_string);

	Q_strncpyz(renderer_buffer, gl_config.renderer_string, sizeof(renderer_buffer));
	renderer_buffer[sizeof(renderer_buffer)-1] = 0;
	Q_strlwr(renderer_buffer);

	Q_strncpyz(vendor_buffer, gl_config.vendor_string, sizeof(vendor_buffer));
	vendor_buffer[sizeof(vendor_buffer)-1] = 0;
	Q_strlwr(vendor_buffer);

	if (strstr(renderer_buffer, "voodoo")) {
		if (!strstr(renderer_buffer, "rush"))
			gl_config.renderer = GL_RENDERER_VOODOO;
		else
			gl_config.renderer = GL_RENDERER_VOODOO_RUSH;
	} else if (strstr(vendor_buffer, "sgi"))
		gl_config.renderer = GL_RENDERER_SGI;
	else if (strstr(renderer_buffer, "permedia"))
		gl_config.renderer = GL_RENDERER_PERMEDIA2;
	else if (strstr(renderer_buffer, "glint"))
		gl_config.renderer = GL_RENDERER_GLINT_MX;
	else if (strstr(renderer_buffer, "glzicd"))
		gl_config.renderer = GL_RENDERER_REALIZM;
	else if (strstr(renderer_buffer, "gdi"))
		gl_config.renderer = GL_RENDERER_MCD;
	else if (strstr(renderer_buffer, "pcx2"))
		gl_config.renderer = GL_RENDERER_PCX2;
	else if (strstr(renderer_buffer, "verite"))
		gl_config.renderer = GL_RENDERER_RENDITION;
	else
		gl_config.renderer = GL_RENDERER_OTHER;

	if (toupper(gl_monolightmap->string[1]) != 'F') {
		if (gl_config.renderer == GL_RENDERER_PERMEDIA2) {
			ri.Cvar_Set("gl_monolightmap", "A");
			ri.Con_Printf(PRINT_ALL, "...using gl_monolightmap ' a '\n");
		} else
			ri.Cvar_Set("gl_monolightmap", "0");
	}

#if defined (__linux__) || defined (__FreeBSD__) || defined (__NetBSD__)
	ri.Cvar_SetValue("gl_finish", 1);
#else
	/* MCD has buffering issues */
	if (gl_config.renderer == GL_RENDERER_MCD)
		ri.Cvar_SetValue("gl_finish", 1);
#endif

	/* grab extensions */
	if (strstr(gl_config.extensions_string, "GL_EXT_compiled_vertex_array") || strstr(gl_config.extensions_string, "GL_SGI_compiled_vertex_array")) {
		if (gl_ext_lockarrays->integer) {
			ri.Con_Printf(PRINT_ALL, "...enabling GL_EXT_LockArrays\n");
			qglLockArraysEXT = (void (APIENTRY *) (int, int)) qwglGetProcAddress("glLockArraysEXT");
			qglUnlockArraysEXT = (void (APIENTRY *) (void)) qwglGetProcAddress("glUnlockArraysEXT");
		} else
			ri.Con_Printf(PRINT_ALL, "...ignoring GL_EXT_LockArrays\n");
	} else
		ri.Con_Printf(PRINT_ALL, "...GL_EXT_compiled_vertex_array not found\n");

#ifdef _WIN32
	if (strstr(gl_config.extensions_string, "WGL_EXT_swap_control")) {
		qwglSwapIntervalEXT = (BOOL(WINAPI *) (int)) qwglGetProcAddress("wglSwapIntervalEXT");
		ri.Con_Printf(PRINT_ALL, "...enabling WGL_EXT_swap_control\n");
	} else
		ri.Con_Printf(PRINT_ALL, "...WGL_EXT_swap_control not found\n");
#endif

	if (strstr(gl_config.extensions_string, "GL_ARB_multitexture")) {
		if (gl_ext_multitexture->integer) {
			ri.Con_Printf(PRINT_ALL, "...using GL_ARB_multitexture\n");
			qglMTexCoord2fSGIS = (void (APIENTRY *) (GLenum, GLfloat, GLfloat)) qwglGetProcAddress("glMultiTexCoord2fARB");
			qglActiveTextureARB = (void (APIENTRY *) (GLenum)) qwglGetProcAddress("glActiveTextureARB");
			qglClientActiveTextureARB = (void (APIENTRY *) (GLenum)) qwglGetProcAddress("glClientActiveTextureARB");
			gl_texture0 = GL_TEXTURE0_ARB;
			gl_texture1 = GL_TEXTURE1_ARB;
			gl_texture2 = GL_TEXTURE2_ARB;
			gl_texture3 = GL_TEXTURE3_ARB;
		} else
			ri.Con_Printf(PRINT_ALL, "...ignoring GL_ARB_multitexture\n");
	} else
		ri.Con_Printf(PRINT_ALL, "...GL_ARB_multitexture not found\n");

	if (strstr(gl_config.extensions_string, "GL_EXT_texture_env_combine") || strstr(gl_config.extensions_string, "GL_ARB_texture_env_combine")) {
		if (gl_ext_combine->integer) {
			ri.Con_Printf(PRINT_ALL, "...using GL_EXT_texture_env_combine\n");
			gl_combine = GL_COMBINE_EXT;
		} else {
			ri.Con_Printf(PRINT_ALL, "...ignoring EXT_texture_env_combine\n");
			gl_combine = 0;
		}
	} else {
		ri.Con_Printf(PRINT_ALL, "...GL_EXT_texture_env_combine not found\n");
		gl_combine = 0;
	}

	if (strstr(gl_config.extensions_string, "GL_SGIS_multitexture")) {
		if (qglActiveTextureARB)
			ri.Con_Printf(PRINT_ALL, "...GL_SGIS_multitexture deprecated in favor of ARB_multitexture\n");
		else if (gl_ext_multitexture->integer) {
			ri.Con_Printf(PRINT_ALL, "...using GL_SGIS_multitexture\n");
			qglMTexCoord2fSGIS = (void (APIENTRY *) (GLenum, GLfloat, GLfloat)) qwglGetProcAddress("glMTexCoord2fSGIS");
			qglSelectTextureSGIS = (void (APIENTRY *) (GLenum)) qwglGetProcAddress("glSelectTextureSGIS");
			gl_texture0 = GL_TEXTURE0_SGIS;
			gl_texture1 = GL_TEXTURE1_SGIS;
			gl_texture2 = GL_TEXTURE2_SGIS;
			gl_texture3 = GL_TEXTURE3_SGIS;
		} else
			ri.Con_Printf(PRINT_ALL, "...ignoring GL_SGIS_multitexture\n");
	} else
		ri.Con_Printf(PRINT_ALL, "...GL_SGIS_multitexture not found\n");

	if (strstr(gl_config.extensions_string, "GL_ARB_texture_compression")) {
		if (gl_ext_texture_compression->integer) {
			ri.Con_Printf(PRINT_ALL, "...using GL_ARB_texture_compression\n");
			if (gl_ext_s3tc_compression->integer && strstr(gl_config.extensions_string, "GL_EXT_texture_compression_s3tc")) {
/*				ri.Con_Printf(PRINT_ALL, "   with s3tc compression\n"); */
				gl_compressed_solid_format = GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
				gl_compressed_alpha_format = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
			} else {
/*				ri.Con_Printf(PRINT_ALL, "   without s3tc compression\n"); */
				gl_compressed_solid_format = GL_COMPRESSED_RGB_ARB;
				gl_compressed_alpha_format = GL_COMPRESSED_RGBA_ARB;
			}
		} else {
			ri.Con_Printf(PRINT_ALL, "...ignoring GL_ARB_texture_compression\n");
			gl_compressed_solid_format = 0;
			gl_compressed_alpha_format = 0;
		}
	} else {
		ri.Con_Printf(PRINT_ALL, "...GL_ARB_texture_compression not found\n");
		gl_compressed_solid_format = 0;
		gl_compressed_alpha_format = 0;
	}

	/* anisotropy */
	gl_state.anisotropic = qfalse;

	qglGetIntegerv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &max_aniso);
	ri.Cvar_SetValue("r_ext_max_anisotropy", max_aniso);
	if (r_anisotropic->integer > r_ext_max_anisotropy->integer) {
		ri.Con_Printf(PRINT_ALL, "...max GL_EXT_texture_filter_anisotropic value is %i\n", max_aniso);
		ri.Cvar_SetValue("r_anisotropic", r_ext_max_anisotropy->integer);
	}

	aniso_level = r_anisotropic->integer;
	if (strstr(gl_config.extensions_string, "GL_EXT_texture_filter_anisotropic")) {
		if (!r_anisotropic->integer)
			ri.Con_Printf(PRINT_ALL, "...ignoring GL_EXT_texture_filter_anisotropic\n");
		else {
			ri.Con_Printf(PRINT_ALL, "...using GL_EXT_texture_filter_anisotropic [%2i max] [%2i selected]\n", max_aniso, aniso_level);
			gl_state.anisotropic = qtrue;
		}
	} else {
		ri.Con_Printf(PRINT_ALL, "...GL_EXT_texture_filter_anisotropic not found\n");
		ri.Cvar_Set("r_anisotropic", "0");
		ri.Cvar_Set("r_ext_max_anisotropy", "0");
	}

	if (strstr(gl_config.extensions_string, "GL_EXT_texture_lod_bias")) {
		ri.Con_Printf(PRINT_ALL, "...using GL_EXT_texture_lod_bias\n");
		gl_state.lod_bias = qtrue;
	} else {
		ri.Con_Printf(PRINT_ALL, "...GL_EXT_texture_lod_bias not found\n");
		gl_state.lod_bias = qfalse;
	}

#if 0
	if (strstr(gl_config.extensions_string, "GL_SGIS_generate_mipmap")) {
		ri.Con_Printf(PRINT_ALL, "...using GL_SGIS_generate_mipmap\n");
		gl_state.sgis_mipmap = qtrue;
	} else {
		ri.Con_Printf(PRINT_ALL, "...GL_SGIS_generate_mipmap not found\n");
		ri.Sys_Error(ERR_FATAL, "GL_SGIS_generate_mipmap not found!");
		gl_state.sgis_mipmap = qfalse;
	}
#endif

	if (strstr(gl_config.extensions_string, "GL_EXT_stencil_wrap")) {
		ri.Con_Printf(PRINT_ALL, "...using GL_EXT_stencil_wrap\n");
		gl_state.stencil_wrap = qtrue;
	} else {
		ri.Con_Printf(PRINT_ALL, "...GL_EXT_stencil_wrap not found\n");
		gl_state.stencil_wrap = qfalse;
	}

	if (strstr(gl_config.extensions_string, "GL_EXT_fog_coord")) {
		ri.Con_Printf(PRINT_ALL, "...using GL_EXT_fog_coord\n");
		gl_state.fog_coord = qtrue;
	} else {
		ri.Con_Printf(PRINT_ALL, "...GL_EXT_fog_coord not found\n");
		gl_state.fog_coord = qfalse;
	}

	gl_state.glsl_program = qfalse;
	gl_state.arb_fragment_program = qfalse;
#ifdef HAVE_SHADERS
	if (strstr(gl_config.extensions_string, "GL_ARB_fragment_program")) {
		ri.Con_Printf(PRINT_ALL, "...using GL_ARB_fragment_program\n");
		gl_state.arb_fragment_program = qtrue;

		qglProgramStringARB = (void *) qwglGetProcAddress("glProgramStringARB");
		qglBindProgramARB = (void *) qwglGetProcAddress("glBindProgramARB");
		qglDeleteProgramsARB = (void *) qwglGetProcAddress("glDeleteProgramsARB");
		qglGenProgramsARB = (void *) qwglGetProcAddress("glGenProgramsARB");
		qglProgramEnvParameter4dARB = (void *) qwglGetProcAddress("glProgramEnvParameter4dARB");
		qglProgramEnvParameter4dvARB = (void *) qwglGetProcAddress("glProgramEnvParameter4dvARB");
		qglProgramEnvParameter4fARB = (void *) qwglGetProcAddress("glProgramEnvParameter4fARB");
		qglProgramEnvParameter4fvARB = (void *) qwglGetProcAddress("glProgramEnvParameter4fvARB");
		qglProgramLocalParameter4dARB = (void *) qwglGetProcAddress("glProgramLocalParameter4dARB");
		qglProgramLocalParameter4dvARB = (void *) qwglGetProcAddress("glProgramLocalParameter4dvARB");
		qglProgramLocalParameter4fARB = (void *) qwglGetProcAddress("glProgramLocalParameter4fARB");
		qglProgramLocalParameter4fvARB = (void *) qwglGetProcAddress("glProgramLocalParameter4fvARB");
		qglGetProgramEnvParameterdvARB = (void *) qwglGetProcAddress("glGetProgramEnvParameterdvARB");
		qglGetProgramEnvParameterfvARB = (void *) qwglGetProcAddress("glGetProgramEnvParameterfvARB");
		qglGetProgramLocalParameterdvARB = (void *) qwglGetProcAddress("glGetProgramLocalParameterdvARB");
		qglGetProgramLocalParameterfvARB = (void *) qwglGetProcAddress("glGetProgramLocalParameterfvARB");
		qglGetProgramivARB = (void *) qwglGetProcAddress("glGetProgramivARB");
		qglGetProgramStringARB = (void *) qwglGetProcAddress("glGetProgramStringARB");
		qglIsProgramARB = (void *) qwglGetProcAddress("glIsProgramARB");
	} else {
		ri.Con_Printf(PRINT_ALL, "...GL_ARB_fragment_program not found\n");
		gl_state.arb_fragment_program = qfalse;
	}

	/* FIXME: Is this the right extension to check for? */
	if (strstr(gl_config.extensions_string, "GL_ARB_shading_language_100")) {
		ri.Con_Printf(PRINT_ALL, "...using GL_ARB_shading_language_100\n");
		qglCreateShader  = (void *) qwglGetProcAddress("glCreateShaderObjectARB");
		qglShaderSource  = (void *) qwglGetProcAddress("glShaderSourceARB");
		qglCompileShader = (void *) qwglGetProcAddress("glCompileShaderARB");
		qglCreateProgram = (void *) qwglGetProcAddress("glCreateProgramObjectARB");
		qglAttachShader  = (void *) qwglGetProcAddress("glAttachObjectARB");
		qglLinkProgram   = (void *) qwglGetProcAddress("glLinkProgramARB");
		qglUseProgram    = (void *) qwglGetProcAddress("glUseProgramObjectARB");
		qglDeleteShader  = (void *) qwglGetProcAddress("glDeleteObjectARB");
		qglDeleteProgram = (void *) qwglGetProcAddress("glDeleteObjectARB");
		if (!qglCreateShader)
			Sys_Error("Could not load all needed GLSL functions\n");
		gl_state.glsl_program = qtrue;
	} else {
		ri.Con_Printf(PRINT_ALL, "...GL_ARB_shading_language_100 not found\n");
		gl_state.glsl_program = qfalse;
	}
#endif							/* HAVE_SHADERS */

	gl_state.ati_separate_stencil = qfalse;
	if (strstr(gl_config.extensions_string, "GL_ATI_separate_stencil")) {
		if (!gl_ati_separate_stencil->integer) {
			ri.Con_Printf(PRINT_ALL, "...ignoring GL_ATI_separate_stencil\n");
			gl_state.ati_separate_stencil = qfalse;
		} else {
			ri.Con_Printf(PRINT_ALL, "...using GL_ATI_separate_stencil\n");
			gl_state.ati_separate_stencil = qtrue;
			qglStencilOpSeparateATI = (void (APIENTRY *) (GLenum, GLenum, GLenum, GLenum)) qwglGetProcAddress("glStencilOpSeparateATI");
			qglStencilFuncSeparateATI = (void (APIENTRY *) (GLenum, GLenum, GLint, GLuint)) qwglGetProcAddress("glStencilFuncSeparateATI");
		}
	} else {
		ri.Con_Printf(PRINT_ALL, "...GL_ATI_separate_stencil not found\n");
		gl_state.ati_separate_stencil = qfalse;
		ri.Cvar_Set("gl_ati_separate_stencil", "0");
	}

	gl_state.stencil_two_side = qfalse;
	if (strstr(gl_config.extensions_string, "GL_EXT_stencil_two_side")) {
		if (!gl_stencil_two_side->integer) {
			ri.Con_Printf(PRINT_ALL, "...ignoring GL_EXT_stencil_two_side\n");
			gl_state.stencil_two_side = qfalse;
		} else {
			ri.Con_Printf(PRINT_ALL, "...using GL_EXT_stencil_two_side\n");
			gl_state.stencil_two_side = qtrue;
			qglActiveStencilFaceEXT = (void (APIENTRY *) (GLenum)) qwglGetProcAddress("glActiveStencilFaceEXT");
		}
	} else {
		ri.Con_Printf(PRINT_ALL, "...GL_EXT_stencil_two_side not found\n");
		gl_state.stencil_two_side = qfalse;
		ri.Cvar_Set("gl_stencil_two_side", "0");
	}

	{
		int size;
		GLenum err;

		ri.Con_Printf(PRINT_ALL, "...max texture size:\n");

		qglGetIntegerv(GL_MAX_TEXTURE_SIZE, &size);
		gl_config.maxTextureSize = size;

		/* stubbed or broken drivers may have reported 0 */
		if (gl_config.maxTextureSize <= 0)
			gl_config.maxTextureSize = 0;

		if ((err = qglGetError()) != GL_NO_ERROR) {
			ri.Con_Printf(PRINT_ALL, "......cannot detect !\n");
		} else {
			ri.Con_Printf(PRINT_ALL, "......detected %d\n", size);
			if (gl_maxtexres->integer > size) {
				ri.Con_Printf(PRINT_ALL, "......downgrading from %i\n", gl_maxtexres->integer);
				ri.Cvar_SetValue("gl_maxtexres", size);
			} else if (gl_maxtexres->integer < size) {
				ri.Con_Printf(PRINT_ALL, "......but using %i as requested\n", gl_maxtexres->integer);
			}
		}
	}

	GL_SetDefaultState();

	GL_InitImages();
	R_InitMiscTexture();
	Draw_InitLocal();
	GL_InitBloom();

	GL_CHECK_ERROR

	return qtrue;
}

/**
 * @brief
 * @sa R_Init
 */
static void R_Shutdown (void)
{
	const cmdList_t *commands;

	for (commands = r_commands; commands->name; commands++)
		ri.Cmd_RemoveCommand(commands->name);

	qglDeleteLists(spherelist, 1);
	spherelist = -1;

	GL_ShutdownModels();
	GL_ShutdownImages();
#ifdef HAVE_SHADERS
	GL_ShutdownShaders();
#endif
	Font_Shutdown();

	/* shut down OS specific OpenGL stuff like contexts, etc. */
	GLimp_Shutdown();

	/* shutdown our QGL subsystem */
	QGL_Shutdown();
}

/**
 * @brief
 * @sa R_RenderFrame
 * @sa GLimp_BeginFrame
 */
static void R_BeginFrame (float camera_separation)
{
	gl_state.camera_separation = camera_separation;

	/* change modes if necessary */
	/* FIXME: only restart if CDS is required */
	if (gl_mode->modified || vid_fullscreen->modified || gl_ext_texture_compression->modified) {
		vid_ref->modified = qtrue;
	}

	if (gl_log->modified) {
		GLimp_EnableLogging(gl_log->integer);
		gl_log->modified = qfalse;
	}

	if (r_anisotropic->modified) {
		if (r_anisotropic->integer > r_ext_max_anisotropy->integer) {
			ri.Con_Printf(PRINT_ALL, "...max GL_EXT_texture_filter_anisotropic value is %i\n", r_ext_max_anisotropy->integer);
			ri.Cvar_SetValue("r_anisotropic", r_ext_max_anisotropy->value);
		}
		/*GL_UpdateAnisotropy();*/
		r_anisotropic->modified = qfalse;
	}

	if (con_font->modified) {
		if (con_font->integer == 0) {
			ri.Cvar_ForceSet("con_fontWidth", "8");
			ri.Cvar_ForceSet("con_fontHeight", "16");
			ri.Cvar_ForceSet("con_fontShift", "3");
			con_font->modified = qfalse;
		} else if (con_font->integer == 1 && draw_chars[1]) {
			ri.Cvar_ForceSet("con_fontWidth", "8");
			ri.Cvar_ForceSet("con_fontHeight", "8");
			ri.Cvar_ForceSet("con_fontShift", "3");
			con_font->modified = qfalse;
		} else
			ri.Cvar_ForceSet("con_font", "1");
	}

	if (gl_log->integer)
		GLimp_LogNewFrame();

	if (vid_gamma->modified) {
		vid_gamma->modified = qfalse;
		if (gl_state.hwgamma)
			GLimp_SetGamma();
	}

	GLimp_BeginFrame(camera_separation);

	/* go into 2D mode */
	R_SetGL2D();

	/* draw buffer stuff */
	if (gl_drawbuffer->modified) {
		gl_drawbuffer->modified = qfalse;

		if (gl_state.camera_separation == 0 || !gl_state.stereo_enabled) {
			if (Q_stricmp(gl_drawbuffer->string, "GL_FRONT") == 0)
				qglDrawBuffer(GL_FRONT);
			else
				qglDrawBuffer(GL_BACK);
		}
	}

	/* texturemode stuff */
	/* Realtime set level of anisotropy filtering and change texture lod bias */
	if (gl_texturemode->modified) {
		GL_TextureMode(gl_texturemode->string);
		gl_texturemode->modified = qfalse;
	}

	if (gl_texturealphamode->modified) {
		GL_TextureAlphaMode(gl_texturealphamode->string);
		gl_texturealphamode->modified = qfalse;
	}

	if (gl_texturesolidmode->modified) {
		GL_TextureSolidMode(gl_texturesolidmode->string);
		gl_texturesolidmode->modified = qfalse;
	}

	/* swapinterval stuff */
	GL_UpdateSwapInterval();

	/* clear screen if desired */
	R_Clear();
}

/**
 * @brief
 */
static void R_TakeVideoFrame (int w, int h, byte * captureBuffer, byte * encodeBuffer, qboolean motionJpeg)
{
	size_t frameSize;
	int i;

	qglReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, captureBuffer);

	if (motionJpeg) {
		frameSize = SaveJPGToBuffer(encodeBuffer, 90, w, h, captureBuffer);
		ri.CL_WriteAVIVideoFrame(encodeBuffer, frameSize);
	} else {
		frameSize = w * h;

		for (i = 0; i < frameSize; i++) {   /* Pack to 24bpp and swap R and B */
			encodeBuffer[ i*3 ]     = captureBuffer[ i*4 + 2 ];
			encodeBuffer[ i*3 + 1 ] = captureBuffer[ i*4 + 1 ];
			encodeBuffer[ i*3 + 2 ] = captureBuffer[ i*4 ];
		}
		ri.CL_WriteAVIVideoFrame(encodeBuffer, frameSize * 3);
	}
}

/**
 * @brief
 */
refexport_t GetRefAPI (refimport_t rimp)
{
	refexport_t re;

	ri = rimp;

	re.api_version = API_VERSION;

	re.BeginLoading = GL_BeginLoading;
	re.RegisterModel = R_RegisterModelShort;
	re.RegisterPic = Draw_FindPic;
	re.EndLoading = GL_EndLoading;

	re.RenderFrame = R_RenderFrame;
	re.SetRefDef = R_SetRefreshDefinition;
	re.DrawPtls = R_DrawPtls;

	re.DrawModelDirect = R_DrawModelDirect;
	re.DrawGetPicSize = Draw_GetPicSize;
	re.DrawPic = Draw_Pic;
	re.DrawNormPic = Draw_NormPic;
	re.DrawChar = Draw_Char;
	re.FontDrawString = Font_DrawString;
	re.FontLength = Font_Length;
	re.FontRegister = Font_Register;
	re.DrawTileClear = Draw_TileClear;
	re.DrawFill = Draw_Fill;
	re.DrawColor = Draw_Color;
	re.DrawFadeScreen = Draw_FadeScreen;
	re.DrawDayAndNight = Draw_DayAndNight;
	re.DrawLineStrip = Draw_LineStrip;
	re.DrawLineLoop = Draw_LineLoop;
	re.DrawPolygon = Draw_Polygon;
	re.DrawCircle = Draw_Circle;
	re.Draw3DGlobe = Draw_3DGlobe;
	re.Draw3DMapMarkers = Draw_3DMapMarkers;
	re.DrawImagePixelData = GL_DrawImagePixelData;
	re.AnimAppend = Anim_Append;
	re.AnimChange = Anim_Change;
	re.AnimRun = Anim_Run;
	re.AnimGetName = Anim_GetName;

	re.LoadTGA = LoadTGA;

	re.Init = R_Init;
	re.Shutdown = R_Shutdown;

	re.BeginFrame = R_BeginFrame;
	re.EndFrame = GLimp_EndFrame;

	re.AppActivate = GLimp_AppActivate;
	re.TakeVideoFrame = R_TakeVideoFrame;
	Swap_Init();

	return re;
}


#ifndef REF_HARD_LINKED
/* this is only here so the functions in q_shared.c and q_shwin.c can link */
/**
 * @brief
 */
void Sys_Error (const char *error, ...)
{
	va_list argptr;
	char text[1024];

	va_start(argptr, error);
	Q_vsnprintf(text, sizeof(text), error, argptr);
	va_end(argptr);

	text[sizeof(text)-1] = 0;

	ri.Sys_Error(ERR_FATAL, "%s", text);
}

/**
 * @brief
 */
void Com_Printf (const char *fmt, ...)
{
	va_list argptr;
	char text[1024];

	va_start(argptr, fmt);
	Q_vsnprintf(text, sizeof(text), fmt, argptr);
	va_end(argptr);

	text[sizeof(text)-1] = 0;

	ri.Con_Printf(PRINT_ALL, "%s", text);
}

/**
 * @brief
 */
void Com_DPrintf (const char *fmt, ...)
{
	va_list argptr;
	char text[1024];

	va_start(argptr, fmt);
	Q_vsnprintf(text, sizeof(text), fmt, argptr);
	va_end(argptr);

	text[sizeof(text)-1] = 0;

	ri.Con_Printf(PRINT_DEVELOPER, "%s", text);
}
#endif /* REF_HARD_LINKED */
