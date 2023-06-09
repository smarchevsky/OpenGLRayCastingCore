#pragma once
namespace Utils
{
	constexpr char resourceDir[] = "/home/staseg/Projects/Downloaded/OpenGLRayCastingCore/";
	
	inline uint32_t powerOfTwo(uint32_t value)
	{
		value--;
		value |= value >> 1;
		value |= value >> 2;
		value |= value >> 4;
		value |= value >> 8;
		value |= value >> 16;
		value++;
		return value;
	}
};
