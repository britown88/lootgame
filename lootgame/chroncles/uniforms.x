//	  | name,         | enumName,     | Type,     | Default
//---------------------------------------------------------------------
UNIFORM(uTexMatrix,		TextureMatrix,	Matrix,		Matrix::identity())
UNIFORM(uModelMatrix,	ModelMatrix,	Matrix,		Matrix::identity())
UNIFORM(uViewMatrix,	   ViewMatrix,		Matrix,		Matrix::identity())

UNIFORM(uColor,			Color,			ColorRGBAf, {1,1,1,1})
UNIFORM(uAlpha,			Alpha,			Float,		   1)

UNIFORM(uDiffuse,		   DiffuseTexture,TextureSlot, 0)

UNIFORM(uColorOnly,		ColorOnly,		Bool,		   false)