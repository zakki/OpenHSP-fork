#ifndef gamehsp2d_H_
#define gamehsp2d_H_

#include "gameplay.h"
#include "gpmat.h"
#include "../texmes.h"

using namespace gameplay;

//#define USE_GPBFONT

/**
 * Main game class.
 */
#define GPPRM_FPS 0
#define GPPRM_VSYNC 1
#define GPPRM_MAX 2

#define GPOBJ_FLAG_NONE (0)
#define GPOBJ_FLAG_ENTRY (1)

#define GPOBJ_MODE_HIDE (1)
#define GPOBJ_MODE_CLIP (2)
#define GPOBJ_MODE_XFRONT (4)
#define GPOBJ_MODE_WIRE (8)
#define GPOBJ_MODE_MOVE (32)
#define GPOBJ_MODE_FLIP (64)
#define GPOBJ_MODE_BORDER (0x80)
#define GPOBJ_MODE_2D (0x100)
#define GPOBJ_MODE_TIMER (0x200)
#define GPOBJ_MODE_SORT (0x400)
#define GPOBJ_MODE_LATE (0x4000)

#define GPOBJ_MODE_BHIDE (0x8000)

#define GPOBJ_ID_MATFLAG  (0x200000)
#define GPOBJ_ID_FLAGBIT (0xff00000)
#define GPOBJ_ID_FLAGMASK (0x0fffff)

#define GPOBJ_ID_EXFLAG (0x100000)
#define GPOBJ_ID_SCENE  (0x100001)
#define GPOBJ_ID_CAMERA (0x100002)
#define GPOBJ_ID_LIGHT  (0x100003)
#define GPOBJ_ID_TOUCHNODE  (0x100004)

#define GPOBJ_SHAPE_NONE (-1)
#define GPOBJ_SHAPE_MODEL (0)
#define GPOBJ_SHAPE_BOX (1)
#define GPOBJ_SHAPE_FLOOR (2)
#define GPOBJ_SHAPE_PLATE (3)
#define GPOBJ_SHAPE_MESH (4)
#define GPOBJ_SHAPE_SPRITE (16)

enum {
MOC_POS = 0,
MOC_QUAT,
MOC_SCALE,
MOC_DIR,
MOC_EFX,
MOC_COLOR,
MOC_WORK,
MOC_WORK2,
MOC_AXANG,
MOC_ANGX,
MOC_ANGY,
MOC_ANGZ,
MOC_FORWARD,
MOC_MAX
};

enum {
GPOBJ_USERVEC_DIR = 0,
GPOBJ_USERVEC_COLOR,
GPOBJ_USERVEC_WORK,
GPOBJ_USERVEC_WORK2,
GPOBJ_USERVEC_MAX
};

enum {
GPPSET_ENABLE = 0,
GPPSET_FRICTION,
GPPSET_DAMPING,
GPPSET_KINEMATIC,
GPPSET_ANISOTROPIC_FRICTION,
GPPSET_GRAVITY,
GPPSET_LINEAR_FACTOR,
GPPSET_ANGULAR_FACTOR,
GPPSET_ANGULAR_VELOCITY,
GPPSET_LINEAR_VELOCITY,
GPPSET_MASS_CENTER,
GPPSET_MAX
};

#define GPOBJ_PRM_ID_NONE (0)
#define GPOBJ_PRM_ID_SPR (0x100)
#define GPOBJ_PRM_ID_CLOG (0x200)

#define GPOBJ_MARK_UPDATE_POS (0x100)
#define GPOBJ_MARK_UPDATE_ANG (0x200)
#define GPOBJ_MARK_UPDATE_SCALE (0x400)

#define GPOBJ_MATOPT_NOLIGHT (1)
#define GPOBJ_MATOPT_NOMIPMAP (2)
#define GPOBJ_MATOPT_NOCULL (4)
#define GPOBJ_MATOPT_NOZTEST (8)
#define GPOBJ_MATOPT_NOZWRITE (16)
#define GPOBJ_MATOPT_BLENDADD (32)
#define GPOBJ_MATOPT_SPECULAR (64)
#define GPOBJ_MATOPT_USERSHADER (128)
#define GPOBJ_MATOPT_USERBUFFER (256)
#define GPOBJ_MATOPT_MIRROR (512)
#define GPOBJ_MATOPT_CUBEMAP (1024)
#define GPOBJ_MATOPT_NODISCARD (2048)

#define GPDRAW_OPT_OBJUPDATE (1)
#define GPDRAW_OPT_DRAWSCENE (2)
#define GPDRAW_OPT_DRAW2D (4)
#define GPDRAW_OPT_DRAWSCENE_LATE (8)
#define GPDRAW_OPT_DRAW2D_LATE (16)

#define GPANIM_OPT_START_FRAME (0)
#define GPANIM_OPT_END_FRAME (1)
#define GPANIM_OPT_DURATION (2)
#define GPANIM_OPT_ELAPSED (3)
#define GPANIM_OPT_BLEND (4)
#define GPANIM_OPT_PLAYING (5)
#define GPANIM_OPT_SPEED (6)

#define GPOBJ_MULTIEVENT_MAX 4

#define GPNODEINFO_NODE (0)
#define GPNODEINFO_MODEL (1)
#define GPNODEINFO_NAME (0x100)
#define GPNODEINFO_CHILD (0x101)
#define GPNODEINFO_SIBLING (0x102)
#define GPNODEINFO_SKINROOT (0x103)


#define BUFSIZE_POLYCOLOR 32
#define BUFSIZE_POLYTEX 64
#define BUFSIZE_MULTILIGHT 10

#define DEFAULT_ANIM_CLIPNAME "_idle"

#define PARAMNAME_LIGHT_MAXSIZE 32
#define PARAMNAME_LIGHT_DIRECTION "u_directionalLightDirection[0]"
#define PARAMNAME_LIGHT_COLOR "u_directionalLightColor[0]"
#define PARAMNAME_LIGHT_AMBIENT "u_ambientColor"
#define PARAMNAME_LIGHT_POINTCOLOR "u_pointLightColor[0]"
#define PARAMNAME_LIGHT_POINTPOSITION "u_pointLightPosition[0]"
#define PARAMNAME_LIGHT_POINTRANGE "u_pointLightRangeInverse[0]"

#define PARAMNAME_LIGHT_SPOTCOLOR "u_spotLightColor[0]"
#define PARAMNAME_LIGHT_SPOTPOSITION "u_spotLightPosition[0]"
#define PARAMNAME_LIGHT_SPOTDIRECTION "u_spotLightDirection[0]"
#define PARAMNAME_LIGHT_SPOTRANGE "u_spotLightRangeInverse[0]"
#define PARAMNAME_LIGHT_SPOTINNER "u_spotLightInnerAngleCos[0]"
#define PARAMNAME_LIGHT_SPOTOUTER "u_spotLightOuterAngleCos[0]"

#define LIGHT_OPT_NORMAL (0)
#define LIGHT_OPT_POINT (1)
#define LIGHT_OPT_SPOT (2)

#define DEFAULT_GPB_FILEEXT ".gpb"
#define DEFAULT_MATERIAL_FILEEXT ".material"
#define DEFAULT_PHYSISCS_FILEEXT ".physics"

//	gamehsp Object
class gamehsp: public Game
{
public:

	/**
	 * Constructor.
	 */
	gamehsp();

	/*
		HSP Support Functions
	*/
	void resetScreen( int opt=0 );
	void deleteAll( void );

	void hookSetSysReq( int reqid, int value );
	void hookGetSysReq( int reqid );

	void updateViewport( int x, int y, int w, int h );

	gpmat *getMat( int id );
	int deleteMat( int id );
	gpmat *addMat( void );
	Material *getMaterial( int matid );

	int makeNewMat(Material* material, int mode, int color, int matopt );
	int makeNewMat2D( char *fname, int matopt );
	int makeNewMatFromFB(gameplay::FrameBuffer *fb, int matopt);

	Material *makeMaterialColor( int color, int lighting );
	Material *makeMaterialTexture( char *fname, int matopt, Texture *opttex = NULL);
	Material *makeMaterialFromShader( char *vshd, char *fshd, char *defs );
	void setMaterialDefaultBinding(Material* material);
	void setMaterialDefaultBinding(Material* material, int icolor, int matopt);
	float setMaterialBlend( Material* material, int gmode, int gfrate );
	Material *makeMaterialTex2D(Texture *texture, int matopt);
	int getTextureWidth( void );
	int getTextureHeight( void );

	gameplay::FrameBuffer *makeFremeBuffer(char *name, int sx, int sy);
	void deleteFrameBuffer(gameplay::FrameBuffer *fb);
	void selectFrameBuffer(gameplay::FrameBuffer *fb, int sx, int sy);
	void resumeFrameBuffer(void);
	void clearFrameBuffer(void);

	void drawTest( int matid );
	void setFont(char *fontname, int size, int style);
	//int drawFont(void *bmscr, int x, int y, char* text, int* out_ysize);

	// utility function
	void colorVector3( int color, Vector4 &vec );
	void colorVector4( int color, Vector4 &vec );
	Material*make2DMaterialForMesh( void );
	int getCurrentFilterMode(void);
	void setCurrentFilterMode(int mode);

	// 2D draw function
	float *startPolyTex2D( gpmat *mat, int material_id );
	void drawPolyTex2D( gpmat *mat );
	void addPolyTex2D( gpmat *mat );
	void finishPolyTex2D( gpmat *mat );
	void setPolyDiffuseTex2D( float r, float g, float b, float a );

	float *startPolyColor2D( void );
	void drawPolyColor2D( void );
	void addPolyColor2D( int num );
	void finishPolyColor2D( void );
	float setPolyColorBlend( int gmode, int gfrate );
	void setPolyDiffuse2D( float r, float g, float b, float a );
	float *startLineColor2D( void );
	void drawLineColor2D( void );
	void addLineColor2D( int num );
	void finishLineColor2D( void );

	// global light function
	void setupDefines(void);
	char *getLightDefines(void) { return (char *)light_defines.c_str(); }
	char *getNoLightDefines(void) { return (char *)nolight_defines.c_str(); }
	char *getSpecularLightDefines(void) { return (char *)splight_defines.c_str(); }

	/**
	* 2D projection parameter
	*/
	void make2DRenderProjection(Matrix *mat, int sx, int sy);
	void update2DRenderProjection(Material* material, Matrix *mat);
	void update2DRenderProjectionSystem(Matrix* mat);
	void setUser2DRenderProjectionSystem(Matrix* mat, bool updateinv);
	void convert2DRenderProjection(Vector4 &pos);

	/**
	* Message texture service
	*/
	void texmesProc(void);
	void texmesDrawClip(void *bmscr, int x, int y, int psx, int psy, texmes *tex, int basex, int basey);
	texmesManager *getTexmesManager(void) { return &tmes; };


protected:
	/**
	 * Internal use
	 */

	/**
	 * @see Game::initialize
	 */
	void initialize();

	/**
	 * @see Game::finalize
	 */
	void finalize();

	/**
	 * @see Game::render
	 */
	void render(float elapsedTime);

private:

	/**
	 * Draws the scene each frame.
	 */
	bool init2DRender( void );

	float _colrate;
	int _tex_width;
	int _tex_height;

	// gpmat
	int _maxmat;
	gpmat *_gpmat;

	// default scene
	int _viewx1, _viewy1, _viewx2, _viewy2;
	FrameBuffer* _previousFrameBuffer;
	int _filtermode;

	// preset flat mesh
	int proj2Dcode;								// 2D projection Matrix ID code
	Matrix _projectionMatrix2Dpreset;			// 2D projection Matrix (画面全体用)
	Matrix _projectionMatrix2D;					// 2D projection Matrix (システム用)
	Matrix _projectionMatrix2Dinv;				// 2D projection Matrix (逆変換用)
	MeshBatch* _meshBatch;						// MeshBatch for Polygon
	MeshBatch* _meshBatch_line;					// MeshBatch for Line
	MeshBatch* _meshBatch_font;					// MeshBatch for Font

	Effect *_spriteEffect;
	Effect *_spritecolEffect;
	float _bufPolyColor[BUFSIZE_POLYCOLOR];
	float _bufPolyTex[BUFSIZE_POLYTEX];

	// texmes
	texmesManager tmes;

	// preset light defines
	std::string	light_defines;
	std::string	nolight_defines;
	std::string	splight_defines;

	char lightname_ambient[PARAMNAME_LIGHT_MAXSIZE];
	char lightname_color[PARAMNAME_LIGHT_MAXSIZE];
	char lightname_direction[PARAMNAME_LIGHT_MAXSIZE];

	char lightname_pointcolor[PARAMNAME_LIGHT_MAXSIZE];
	char lightname_pointposition[PARAMNAME_LIGHT_MAXSIZE];
	char lightname_pointrange[PARAMNAME_LIGHT_MAXSIZE];

	char lightname_spotcolor[PARAMNAME_LIGHT_MAXSIZE];
	char lightname_spotposition[PARAMNAME_LIGHT_MAXSIZE];
	char lightname_spotdirection[PARAMNAME_LIGHT_MAXSIZE];
	char lightname_spotrange[PARAMNAME_LIGHT_MAXSIZE];
	char lightname_spotinner[PARAMNAME_LIGHT_MAXSIZE];
	char lightname_spotouter[PARAMNAME_LIGHT_MAXSIZE];

	// preset user defines
	std::string	user_vsh;
	std::string	user_fsh;
	std::string	user_defines;

};

#endif
