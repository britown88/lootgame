#include "defs.h"
#include "math.h"

const ColorRGBAf Cleared = sRgbToLinear(ColorRGBAf{ 0.0f,  0.0f,  0.0f, 0.0f });
const ColorRGBAf White =   sRgbToLinear(ColorRGBAf{ 1.0f,  1.0f,  1.0f, 1.0f });
const ColorRGBAf Gray =    sRgbToLinear(ColorRGBAf{ 0.5f,  0.5f,  0.5f, 1.0f });
const ColorRGBAf DkGray =  sRgbToLinear(ColorRGBAf{ 0.25f, 0.25f, 0.25f,1.0f });
const ColorRGBAf LtGray =  sRgbToLinear(ColorRGBAf{ 0.75f, 0.75f, 0.75f,1.0f });
const ColorRGBAf Black =   sRgbToLinear(ColorRGBAf{ 0.0f,  0.0f,  0.0f, 1.0f });
const ColorRGBAf Red =     sRgbToLinear(ColorRGBAf{ 1.0f,  0.0f,  0.0f, 1.0f });
const ColorRGBAf DkRed =   sRgbToLinear(ColorRGBAf{ 0.5f,  0.0f,  0.0f, 1.0f });
const ColorRGBAf Green =   sRgbToLinear(ColorRGBAf{ 0.0f,  1.0f,  0.0f, 1.0f });
const ColorRGBAf DkGreen = sRgbToLinear(ColorRGBAf{ 0.0f,  0.5f,  0.0f, 1.0f });
const ColorRGBAf Blue =    sRgbToLinear(ColorRGBAf{ 0.0f,  0.0f,  1.0f, 1.0f });
const ColorRGBAf DkBlue =  sRgbToLinear(ColorRGBAf{ 0.0f,  0.0f,  0.5f, 1.0f });
const ColorRGBAf Cyan =    sRgbToLinear(ColorRGBAf{ 0.0f,  1.0f,  1.0f, 1.0f });
const ColorRGBAf Yellow =  sRgbToLinear(ColorRGBAf{ 1.0f,  1.0f,  0.0f, 1.0f });
const ColorRGBAf Magenta = sRgbToLinear(ColorRGBAf{ 1.0f,  0.0f,  1.0f, 1.0f });