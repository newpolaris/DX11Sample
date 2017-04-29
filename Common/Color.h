#pragma once

class Color
{
public:
	Color() : r(0.f), g(0.f), b(0.f), a(0.f) {}
	Color(float _r, float _g, float _b, float _a) : r(_r), g(_g), b(_b), a(_a) {}
	union {
        struct
        {
			float r, g, b, a;
        };
		float c[4];
	};
};