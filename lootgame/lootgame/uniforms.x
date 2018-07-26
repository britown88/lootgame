//	  | name,         | enumName,     | Type,     | Default
//---------------------------------------------------------------------
UNIFORM(uTexMatrix,		TextureMatrix,	Matrix,		Matrix::identity())
UNIFORM(uModelMatrix,	ModelMatrix,	Matrix,		Matrix::identity())
UNIFORM(uViewMatrix,	   ViewMatrix,		Matrix,		Matrix::identity())

UNIFORM(uColor,			Color,			ColorRGBAf, {1,1,1,1})
UNIFORM(uAlpha,			Alpha,			f32,		   1)
UNIFORM(uHeight,			Height,			f32,		   0)

UNIFORM(uDiffuse,		   DiffuseTexture,TextureSlot, 0)
UNIFORM(uNormals,		   NormalsTexture,TextureSlot, 1)

UNIFORM(uColorOnly,		ColorOnly,		Bool,		   false)
UNIFORM(uOutlineOnly,   OutlineOnly,	Bool,		   false)
UNIFORM(uOutputNormals,	OutputNormals, Bool,		   false)
UNIFORM(uPointLight,	   PointLight,		Bool,		   false)
UNIFORM(uNormalLighting,NormalLighting,Bool,		   false)
UNIFORM(uDiscardAlpha,  DiscardAlpha,  Bool,		   false)

UNIFORM(uLightIntensity,LightIntensity,f32,        1.0f)