#include "PipelineStateObject.h"

PipelineStateDesc::PipelineStateDesc(std::string il, std::string vs, std::string ps, std::string gs)
	: IL(il), VS(vs), PS(ps), GS(gs) 
{}

void PipelineStateDesc::BindSampler(int ShaderFlags, const std::vector<std::string>& SamplerList)
{
	for (int i = 0; i < NumShader; i++) 
		if (ShaderFlags & i) 
			std::copy(SamplerList.begin(), SamplerList.end(), bindings[i].Sampler.begin());
}
