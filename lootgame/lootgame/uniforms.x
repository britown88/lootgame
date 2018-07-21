//	  | name,         | enumName,     | Type,     | Default
//---------------------------------------------------------------------
UNIFORM(uTexMatrix,		TextureMatrix,	Matrix,		Matrix::identity())
UNIFORM(uModelMatrix,	ModelMatrix,	Matrix,		Matrix::identity())
UNIFORM(uViewMatrix,	ViewMatrix,		Matrix,		Matrix::identity())
UNIFORM(uDiffuse,		TextureSlot,	TextureSlot, 0)
UNIFORM(uColor,			Color,			ColorRGBAf, {1,1,1,1})
UNIFORM(uAlpha,			Alpha,			f32,		1)
UNIFORM(uColorOnly,		ColorOnly,		Bool,		false)
UNIFORM(uPointLight,	PointLight,		Bool,		false)
UNIFORM(uPointLightPos,	PointLightPos,  Float2,     {0,0})