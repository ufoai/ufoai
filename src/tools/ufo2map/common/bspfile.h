#ifndef _BSPFILE
#define _BSPFILE

#include "qfiles.h"


extern	int			nummodels;
extern	dmodel_t	dmodels[MAX_MAP_MODELS];

extern	int			routedatasize;
extern	byte		droutedata[MAX_MAP_ROUTING];

extern	int			lightdatasize;
extern	byte		dlightdata[MAX_MAP_LIGHTING];

extern	int			numleafs;
extern	dleaf_t		dleafs[MAX_MAP_LEAFS];

extern	int			numplanes;
extern	dplane_t	dplanes[MAX_MAP_PLANES];

extern	int			numvertexes;
extern	dvertex_t	dvertexes[MAX_MAP_VERTS];

extern	int			numnodes;
extern	dnode_t		dnodes[MAX_MAP_NODES];

extern	int			numtexinfo;
extern	texinfo_t	texinfo[MAX_MAP_TEXINFO];

extern	int			numfaces;
extern	dface_t		dfaces[MAX_MAP_FACES];

extern	int			numedges;
extern	dedge_t		dedges[MAX_MAP_EDGES];

extern	int			numleaffaces;
extern	unsigned short	dleaffaces[MAX_MAP_LEAFFACES];

extern	int			numleafbrushes;
extern	unsigned short	dleafbrushes[MAX_MAP_LEAFBRUSHES];

extern	int			numsurfedges;
extern	int			dsurfedges[MAX_MAP_SURFEDGES];

extern	int			numbrushes;
extern	dbrush_t	dbrushes[MAX_MAP_BRUSHES];

extern	int			numbrushsides;
extern	dbrushside_t	dbrushsides[MAX_MAP_BRUSHSIDES];

void LoadBSPFile(char *filename);
void LoadBSPFileTexinfo(char *filename);	/* just for qdata */
void WriteBSPFile(const char *filename);
void PrintBSPFileSizes(void);

/*=============== */


typedef struct epair_s
{
	struct epair_s	*next;
	char	*key;
	char	*value;
} epair_t;

typedef struct
{
	vec3_t		origin;
	int			firstbrush;
	int			numbrushes;
	epair_t		*epairs;

	/* only valid for func_areaportals */
	int			areaportalnum;
	int			portalareas[2];
} entity_t;

extern	int			num_entities;
extern	entity_t	entities[MAX_MAP_ENTITIES];

void	ParseEntities(void);
void	UnparseEntities(void);

void 	SetKeyValue(entity_t *ent, char *key, char *value);
char 	*ValueForKey(entity_t *ent, char *key);
/* will return "" if not present */

vec_t	FloatForKey(entity_t *ent, char *key);
void 	GetVectorForKey(entity_t *ent, char *key, vec3_t vec);

epair_t *ParseEpair(void);

byte *CompressRouting(byte *dataStart, byte *destStart, int l);
/*int DeCompressRouting(byte **source, byte *dataStart );*/

#endif /* _BSP_FILE */
