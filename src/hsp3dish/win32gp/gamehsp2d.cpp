#include "gamehsp2d.h"

#include "../../hsp3/hsp3config.h"
#include "../supio.h"
#include "../sysreq.h"
#include "../hspwnd.h"

#include "shader_sprite.h"

// Default sprite shaders
//#define SPRITE_VSH "res/shaders/sprite.vert"
//#define SPRITE_FSH "res/shaders/sprite.frag"
#define SPRITECOL_VSH "res/shaders/spritecol.vert"
#define SPRITECOL_FSH "res/shaders/spritecol.frag"

enum {
CLSMODE_NONE = 0,
CLSMODE_SOLID,
CLSMODE_TEXTURE,
CLSMODE_BLUR,
CLSMODE_MAX,
};

// material defines for load model
static std::string model_defines;
static std::string model_defines_shade;

extern bool hasParameter( Material* material, const char* name );


static void QuaternionToEulerAngles(Quaternion q, double& roll, double& pitch, double& yaw)
{
	// roll (x-axis rotation)
	double sinr_cosp = +2.0 * (q.w * q.x + q.y * q.z);
	double cosr_cosp = +1.0 - 2.0 * (q.x * q.x + q.y * q.y);
	roll = atan2(sinr_cosp, cosr_cosp);

	// pitch (y-axis rotation)
	double sinp = +2.0 * (q.w * q.y - q.z * q.x);
	if (fabs(sinp) >= 1)
		pitch = copysign(MATH_PI / 2, sinp); // use 90 degrees if out of range
	else
		pitch = asin(sinp);

	// yaw (z-axis rotation)
	double siny_cosp = +2.0 * (q.w * q.z + q.x * q.y);
	double cosy_cosp = +1.0 - 2.0 * (q.y * q.y + q.z * q.z);
	yaw = atan2(siny_cosp, cosy_cosp);
}

/*------------------------------------------------------------*/
/*
		gameplay game class
*/
/*------------------------------------------------------------*/

gamehsp::gamehsp()
{
	// コンストラクタ

	_gpmat = NULL;
	_colrate = 1.0f / 255.0f;
	_meshBatch = NULL;
	_meshBatch_line = NULL;
	_meshBatch_font = NULL;
	_spriteEffect = NULL;
	_spritecolEffect = NULL;
}

void gamehsp::initialize()
{
	resetScreen();
}

void gamehsp::finalize()
{
	// release
	//
	deleteAll();
}


void gamehsp::deleteAll( void )
{
	// release
	//
	tmes.texmesTerm();

	if (_gpmat) {
		int i;
		for (i = 0; i<_maxmat; i++) { deleteMat(i); }
		delete[] _gpmat;
		_gpmat = NULL;
	}

	if (_meshBatch) {
		delete _meshBatch;
		_meshBatch = NULL;
	}
	if (_meshBatch_line) {
		delete _meshBatch_line;
		_meshBatch_line = NULL;
	}
	if (_meshBatch_font) {
		delete _meshBatch_font;
		_meshBatch_font = NULL;
	}

	SAFE_RELEASE(_spriteEffect);
	SAFE_RELEASE(_spritecolEffect);
}


void gamehsp::render(float elapsedTime)
{
	// 描画先をリセット
	resumeFrameBuffer();

	// ビューポート初期化
	updateViewport(_viewx1, _viewy1, _viewx2, _viewy2);

	// プロジェクションの初期化
	update2DRenderProjectionSystem(&_projectionMatrix2Dpreset);

	// 画面クリア
	clearFrameBuffer();

	// 使用するマトリクスをコピー
	_projectionMatrix2D = _projectionMatrix2Dpreset;
	_projectionMatrix2D.invert(&_projectionMatrix2Dinv);

}


/*------------------------------------------------------------*/
/*
		HSP Script Service
*/
/*------------------------------------------------------------*/

void gamehsp::hookSetSysReq( int reqid, int value )
{
	//	HGIMG4用のSetSysReq
	//
	switch( reqid ) {
	case SYSREQ_VSYNC:
		setVsync( value!=0 );
		break;
	default:
		break;
	}
}


void gamehsp::hookGetSysReq( int reqid )
{
	//	HGIMG4用のGetSysReq
	//
	switch( reqid ) {
	case SYSREQ_FPS:
		SetSysReq( reqid, (int)getFrameRate() );
		break;
	default:
		break;
	}
}


void gamehsp::resetScreen( int opt )
{
	// 画面の初期化
	deleteAll();

	// VSYNCの設定
	setVsync( GetSysReq( SYSREQ_VSYNC )!=0 );

	// gpmat作成
	_maxmat = GetSysReq( SYSREQ_MAXMATERIAL );
	_gpmat = new gpmat[ _maxmat ];

	// シェーダー定義文字列を生成
	setupDefines();

	// 2D初期化
	init2DRender();

	// ビューポート初期化
	updateViewport( 0, 0, getWidth(), getHeight() );

	// texmat作成
	tmes.texmesInit(GetSysReq(SYSREQ_MESCACHE_MAX));

	int sx = 32;
	int sy = 32;

	Texture* texture;
	texture = Texture::create(Texture::Format::RGBA, sx, sy, NULL, false, Texture::TEXTURE_2D);
	//texture = Texture::create(id, sx, sy, Texture::Format::RGBA);
	//texture = Texture::create("chr.png");
	if (texture == NULL) {
		Alertf("ERR");
		return;
	}
	//texture->setData( (const unsigned char *)data );
	Material* _fontMaterial;
	_fontMaterial = makeMaterialTex2D(texture, GPOBJ_MATOPT_NOMIPMAP);
	if (_fontMaterial == NULL) {
		Alertf("ERR");
		return;
	}

	VertexFormat::Element elements[] =
	{
		VertexFormat::Element(VertexFormat::POSITION, 3),
		VertexFormat::Element(VertexFormat::TEXCOORD0, 2),
		VertexFormat::Element(VertexFormat::COLOR, 4)
	};

	unsigned int elementCount = sizeof(elements) / sizeof(VertexFormat::Element);
	_meshBatch_font = MeshBatch::create(VertexFormat(elements, elementCount), Mesh::TRIANGLE_STRIP, _fontMaterial, true, 16, 256);

	SAFE_RELEASE(texture);
	SAFE_RELEASE(_fontMaterial);
}


void gamehsp::updateViewport( int x, int y, int w, int h )
{
	Rectangle viewport;
	_viewx1 = x; _viewy1 = y;
	_viewx2 = w; _viewy2 = h;
	viewport.set((float)x, (float)y, (float)w, (float)h);
	setViewport( viewport );
}


void gamehsp::selectFrameBuffer(gameplay::FrameBuffer *fb, int sx, int sy)
{
	Rectangle viewport;
	_previousFrameBuffer = fb->bind();
	viewport.set(0, 0, (float)sx, (float)sy);
	setViewport(viewport);
	clearFrameBuffer();
}


void gamehsp::resumeFrameBuffer(void)
{
	if (_previousFrameBuffer) {
		_previousFrameBuffer->bind();
		_previousFrameBuffer = NULL;
	}
}


void gamehsp::clearFrameBuffer(void)
{
	Vector4 clscolor;
	int icolor;
	int clsmode;
	icolor = GetSysReq(SYSREQ_CLSCOLOR);
	clsmode = GetSysReq(SYSREQ_CLSMODE);

	if (clsmode == CLSMODE_NONE) {
		clear(CLEAR_DEPTH, Vector4::zero(), 1.0f, 0);
		return;
	}
	clscolor.set(((icolor >> 16) & 0xff)*_colrate, ((icolor >> 8) & 0xff)*_colrate, (icolor & 0xff)*_colrate, 1.0f);
	clear(CLEAR_COLOR_DEPTH, clscolor, 1.0f, 0);
}


bool gamehsp::init2DRender( void )
{
	// 2D用の初期化
	//
	proj2Dcode = -2;

	// 2D用のプロジェクション
	make2DRenderProjection(&_projectionMatrix2Dpreset,getWidth(),getHeight());

	// スプライト用のshader
	_spriteEffect = Effect::createFromSource(intshd_sprite_vert, intshd_sprite_frag);
	if ( _spriteEffect == NULL ) {
        GP_ERROR("2D shader initalize failed.");
        return false;
	}

	_spritecolEffect = Effect::createFromSource(intshd_spritecol_vert, intshd_spritecol_frag);
	if (_spritecolEffect == NULL) {
		GP_ERROR("2D shader initalize failed.");
		return false;
	}

	// MeshBatch for FlatPolygon Draw
    VertexFormat::Element elements[] =
    {
        VertexFormat::Element(VertexFormat::POSITION, 3),
        VertexFormat::Element(VertexFormat::COLOR, 4)
    };
	unsigned int elementCount = sizeof(elements) / sizeof(VertexFormat::Element);
	Material* mesh_material = make2DMaterialForMesh();
	_meshBatch = MeshBatch::create(VertexFormat(elements, elementCount), Mesh::TRIANGLE_STRIP, mesh_material, true, 16, 16 );
	SAFE_RELEASE(mesh_material);

	mesh_material = make2DMaterialForMesh();
	_meshBatch_line = MeshBatch::create(VertexFormat(elements, elementCount), Mesh::LINES, mesh_material, true, 4, 4 );
	SAFE_RELEASE(mesh_material);

	int i;
	for(i=0;i<BUFSIZE_POLYCOLOR;i++) {
		_bufPolyColor[i] = 0.0f;
	}
	for(i=0;i<BUFSIZE_POLYTEX;i++) {
		_bufPolyTex[i] = 0.0f;
	}

	return true;
}


void gamehsp::make2DRenderProjection(Matrix *mat,int sx, int sy)
{
	//	2Dシステム用のプロジェクションを作成する
	Matrix::createOrthographicOffCenter(0.0f, (float)sx, (float)sy, 0.0f, -1.0f, 1.0f, mat);
	//mat->translate(0.5f, 0.0f, 0.0f);						// 座標誤差修正のため0.5ドットずらす
}


void gamehsp::update2DRenderProjection(Material* material, Matrix *mat)
{
	//	マテリアルのプロジェクションを再設定する
	MaterialParameter *prm = material->getParameter("u_projectionMatrix");
	if (prm) {
		prm->setValue(*mat);
	}
}


void gamehsp::update2DRenderProjectionSystem(Matrix *mat)
{
	//	2Dシステム用のプロジェクションを再設定する
	if (_meshBatch) update2DRenderProjection(_meshBatch->getMaterial(), mat);
	if (_meshBatch_line) update2DRenderProjection(_meshBatch_line->getMaterial(), mat);
	if (_meshBatch_font) update2DRenderProjection(_meshBatch_font->getMaterial(), mat);

}


void gamehsp::setUser2DRenderProjectionSystem(Matrix* mat, bool updateinv)
{
	//	ユーザー用のプロジェクションを設定する
	_projectionMatrix2D = *mat;
	update2DRenderProjectionSystem(&_projectionMatrix2D);
	if (updateinv) {
		proj2Dcode--;
		_projectionMatrix2D.invert(&_projectionMatrix2Dinv);
	}
}


void gamehsp::convert2DRenderProjection(Vector4& pos)
{
	//	ユーザー用のプロジェクションを逆変換する
	Vector4 result;
	_projectionMatrix2Dinv.transformVector(pos,&result);
	pos.x = result.x;
	pos.y = result.y;
	pos.z = result.z;
}


int gamehsp::getCurrentFilterMode(void)
{
	//	現在のフィルターモードを返す
	return _filtermode;
}


void gamehsp::setCurrentFilterMode(int mode)
{
	//	フィルターモードを設定する
	_filtermode = mode;
}

/*------------------------------------------------------------*/
/*
		Text message support
*/
/*------------------------------------------------------------*/

void gamehsp::texmesProc(void)
{
	tmes.texmesProc();
}

void gamehsp::texmesDrawClip(void *bmscr, int x, int y, int psx, int psy, texmes *tex, int basex, int basey)
{
	//		画像コピー
	//		texid内の(xx,yy)-(xx+srcsx,yy+srcsy)を現在の画面に(psx,psy)サイズでコピー
	//		カレントポジション、描画モードはBMSCRから取得
	//
	//	float psx, psy;
	float x1, y1, x2, y2, sx, sy;
	float tx0, ty0, tx1, ty1;

	//		meshのTextureを差し替える
	Uniform* samplerUniform = NULL;
	for (unsigned int i = 0, count = _spriteEffect->getUniformCount(); i < count; ++i)
	{
		Uniform* uniform = _spriteEffect->getUniform(i);
		if (uniform && uniform->getType() == GL_SAMPLER_2D)
		{
			samplerUniform = uniform;
			break;
		}
	}

	MaterialParameter* mp = _meshBatch_font->getMaterial()->getParameter(samplerUniform->getName());
	mp->setValue(tex->_texture);

	tx0 = ((float)(basex));
	tx1 = ((float)(basex+psx));
	ty0 = ((float)(basey));
	ty1 = ((float)(basey+psy));

	x1 = ((float)x);
	y1 = ((float)y);
	x2 = x1 + psx;
	y2 = y1 + psy;

	sx = tex->ratex;
	sy = tex->ratey;

	tx0 *= sx;
	tx1 *= sx;
	ty0 *= sy;
	ty1 *= sy;

	float* v = _bufPolyTex;

	BMSCR *bm = (BMSCR *)bmscr;
	float r_val = bm->colorvalue[0];
	float g_val = bm->colorvalue[1];
	float b_val = bm->colorvalue[2];
	float a_val = setPolyColorBlend(bm->gmode, bm->gfrate);

	*v++ = x1; *v++ = y2; v++;
	*v++ = tx0; *v++ = ty1;
	*v++ = r_val; *v++ = g_val; *v++ = b_val; *v++ = a_val;
	*v++ = x1; *v++ = y1; v++;
	*v++ = tx0; *v++ = ty0;
	*v++ = r_val; *v++ = g_val; *v++ = b_val; *v++ = a_val;
	*v++ = x2; *v++ = y2; v++;
	*v++ = tx1; *v++ = ty1;
	*v++ = r_val; *v++ = g_val; *v++ = b_val; *v++ = a_val;
	*v++ = x2; *v++ = y1; v++;
	*v++ = tx1; *v++ = ty0;
	*v++ = r_val; *v++ = g_val; *v++ = b_val; *v++ = a_val;

	static unsigned short indices[] = { 0, 1, 2, 3 };

	_meshBatch_font->start();
	_meshBatch_font->add(_bufPolyTex, 4, indices, 4);
	_meshBatch_font->finish();
	_meshBatch_font->draw();

	mp->setValue(0);
}


void gamehsp::setFont(char* fontname, int size, int style)
{
	// フォント設定
	tmes.setFont(fontname,size,style);
}


/*------------------------------------------------------------*/
/*
		Utils
*/
/*------------------------------------------------------------*/

void gamehsp::colorVector3( int icolor, Vector4 &vec )
{
	vec.set( ( (icolor>>16)&0xff )*_colrate, ( (icolor>>8)&0xff )*_colrate, ( icolor&0xff )*_colrate, 1.0f );
}


void gamehsp::colorVector4( int icolor, Vector4 &vec )
{
	vec.set( ( (icolor>>16)&0xff )*_colrate, ( (icolor>>8)&0xff )*_colrate, ( icolor&0xff )*_colrate, ( (icolor>>24)&0xff )*_colrate );
}


float *gamehsp::startPolyColor2D( void )
{
    _meshBatch->start();
	return _bufPolyColor;
}


void gamehsp::drawPolyColor2D( void )
{
    static unsigned short indices[] = { 0, 1, 2, 3 };
	_meshBatch->add( _bufPolyColor, 4, indices, 4 );
    _meshBatch->finish();
    _meshBatch->draw();
}


void gamehsp::addPolyColor2D( int num )
{
    static unsigned short indices[] = { 0, 1, 2, 3 };
	_meshBatch->add( _bufPolyColor, num, indices, num );
}


void gamehsp::finishPolyColor2D( void )
{
    _meshBatch->finish();
    _meshBatch->draw();
}


void gamehsp::setPolyDiffuse2D( float r, float g, float b, float a )
{
	//	Vertexのカラーコードのみを設定する
	//
	int i;
	float *v = _bufPolyColor;
	for(i=0;i<4;i++) {
		v += 3;						// Posを飛ばす
		*v++ = r;
		*v++ = g;
		*v++ = b;
		*v++ = a;
	}
}


float *gamehsp::startLineColor2D( void )
{
    _meshBatch_line->start();
	return _bufPolyColor;
}


void gamehsp::drawLineColor2D( void )
{
    static unsigned short indices[] = { 0, 1, 2, 3 };
	_meshBatch_line->add( _bufPolyColor, 2, indices, 2 );
    _meshBatch_line->finish();
    _meshBatch_line->draw();
}


void gamehsp::addLineColor2D( int num )
{
    static unsigned short indices[] = { 0, 1, 2, 3 };
	_meshBatch_line->add( _bufPolyColor, num, indices, num );
}


void gamehsp::finishLineColor2D( void )
{
    _meshBatch_line->finish();
    _meshBatch_line->draw();
}


float *gamehsp::startPolyTex2D(gpmat *mat, int material_id )
{
	//	テクスチャポリゴン描画開始
	//		mat : コピー元のマテリアル
	//		material_id : 描画先のマテリアルID
	//
	MeshBatch *mesh = mat->_mesh;
	if ( mesh == NULL ) {
        GP_ERROR("Bad Material.");
        return NULL;
	}

	gpmat *targetmat = getMat(material_id);
	if (targetmat) {
		if (mat->_target_material_id != material_id) {
			//	同一マテリアルIDの場合はプロジェクションを設定しない(高速化のため)
			update2DRenderProjection(mat->_material, &targetmat->_projectionMatrix2D);
			mat->_target_material_id = material_id;
		}
	}
	else {
		//	メイン画面用の2Dプロジェクション
		if (mat->_target_material_id != proj2Dcode ) {
			update2DRenderProjection(mat->_material, &_projectionMatrix2D);
			mat->_target_material_id = proj2Dcode;
		}
	}

	mesh->start();
	return _bufPolyTex;
}


void gamehsp::drawPolyTex2D( gpmat *mat )
{
    static unsigned short indices[] = { 0, 1, 2, 3 };

	MeshBatch *mesh = mat->_mesh;
	if ( mesh == NULL ) {
        GP_ERROR("Bad Material.");
        return;
	}
	mat->applyFilterMode(_filtermode);
	mesh->add( _bufPolyTex, 4, indices, 4 );
    mesh->finish();
    mesh->draw();
}


void gamehsp::addPolyTex2D( gpmat *mat )
{
    static unsigned short indices[] = { 0, 1, 2, 3 };

	MeshBatch *mesh = mat->_mesh;
	if ( mesh == NULL ) {
        GP_ERROR("Bad Material.");
        return;
	}
	mesh->add( _bufPolyTex, 4, indices, 4 );
}


void gamehsp::finishPolyTex2D( gpmat *mat )
{
	MeshBatch *mesh = mat->_mesh;
	if ( mesh == NULL ) {
        GP_ERROR("Bad Material.");
        return;
	}
	mat->applyFilterMode(_filtermode);
	mesh->finish();
    mesh->draw();
}


void gamehsp::setPolyDiffuseTex2D( float r, float g, float b, float a )
{
	//	Vertexのカラーコードのみを設定する
	//
	int i;
	float *v = _bufPolyTex;
	for(i=0;i<4;i++) {
		v += 3 + 2;					// Pos,UVを飛ばす
		*v++ = r;
		*v++ = g;
		*v++ = b;
		*v++ = a;
	}
}


float gamehsp::setPolyColorBlend( int gmode, int gfrate )
{
	//	2Dカラー描画設定
	//	(戻り値=alpha値(0.0～1.0))
	//
	Material *material;
	material = _meshBatch->getMaterial();
	return setMaterialBlend( material, gmode, gfrate );
}


void gamehsp::drawTest( int matid )
{

	float points[] ={
	        0,100,0, 1,1,1,1,
	        0,0,0, 1,1,1,1,
	        100,100,0, 1,1,0,1,
	        100,0,0, 1,1,0,1,

	        0,300,0, 1,1,1,0,
	        0,200,0, 1,1,1,0,
	        100,300,0, 1,0,1,1,
	        100,200,0, 1,0,1,1,
	};

    //SPRITE_ADD_VERTEX(v[0], downLeft.x, downLeft.y, z, u1, v1, color.x, color.y, color.z, color.w);
    //SPRITE_ADD_VERTEX(v[1], upLeft.x, upLeft.y, z, u1, v2, color.x, color.y, color.z, color.w);
    //SPRITE_ADD_VERTEX(v[2], downRight.x, downRight.y, z, u2, v1, color.x, color.y, color.z, color.w);
    //SPRITE_ADD_VERTEX(v[3], upRight.x, upRight.y, z, u2, v2, color.x, color.y, color.z, color.w);

//	material->getParameter("u_projectionMatrix")->bindValue(this, &gamehsp::getProjectionMatrix);
//	material->getParameter("u_projectionMatrix")->setValue( _projectionMatrix2D );
//	material->getParameter("u_worldViewProjectionMatrix")->setValue( _camera->getWorldViewProjectionMatrix() );
//	material->getParameter("u_inverseTransposeWorldViewMatrix")->setValue( _camera->getInverseTransposeWorldViewMatrix() );
//	material->getParameter("u_cameraPosition")->setValue( _camera->getTranslation() );

    static unsigned short indices[] = { 0, 1, 2, 3, 3,4,   4,5,6,7, };

    _meshBatch->start();
    _meshBatch->add( points, 8, indices, 10 );
    _meshBatch->finish();
    _meshBatch->draw();
	return;
}


Material* gamehsp::make2DMaterialForMesh( void )
{
	RenderState::StateBlock *state;
	Material* mesh_material = NULL;

	if (_spritecolEffect) {
		mesh_material = Material::create(_spritecolEffect);
	}

	//Material* mesh_material = Material::create(SPRITECOL_VSH, SPRITECOL_FSH, NULL);
	if ( mesh_material == NULL ) {
        GP_ERROR("2D initalize failed.");
        return NULL;
	}
	update2DRenderProjection(mesh_material, &_projectionMatrix2Dpreset);

	state = mesh_material->getStateBlock();
	state->setCullFace(false);
	state->setDepthTest(false);
	state->setDepthWrite(false);
	state->setBlend(true);
	state->setBlendSrc(RenderState::BLEND_SRC_ALPHA);
	state->setBlendDst(RenderState::BLEND_ONE_MINUS_SRC_ALPHA);
	return mesh_material;
}


gameplay::FrameBuffer *gamehsp::makeFremeBuffer(char *name, int sx, int sy)
{
	gameplay::FrameBuffer *frameBuffer;
	frameBuffer = FrameBuffer::create(name, sx, sy);
	DepthStencilTarget* dst = DepthStencilTarget::create(name, DepthStencilTarget::DEPTH_STENCIL, sx, sy);
	frameBuffer->setDepthStencilTarget(dst);
	SAFE_RELEASE(dst);
	return frameBuffer;
}

void gamehsp::deleteFrameBuffer(gameplay::FrameBuffer *fb)
{
	SAFE_RELEASE(fb);
}
