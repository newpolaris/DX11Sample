//***************************************************************************************
// BlurFilter.cpp by Frank Luna (C) 2011 All Rights Reserved.
//***************************************************************************************

#include "BlurFilter.h"
#include "Effects.h"
#include <algorithm>
#include <iterator>
#include <string.h>
#include <numeric>

BlurFilter::BlurFilter()
  : mBlurredOutputTexSRV(0), mBlurredOutputTexUAV(0), mBlurredOutputTexRTV(0)
{
}

BlurFilter::~BlurFilter()
{
	ReleaseCOM(mBlurredOutputTexSRV);
	ReleaseCOM(mBlurredOutputTexUAV);
	ReleaseCOM(mBlurredOutputTexRTV);
}

ID3D11ShaderResourceView* BlurFilter::GetBlurredOutput()
{
	return mBlurredOutputTexSRV;
}

void BlurFilter::SetGaussianWeights(float sigma)
{
	float d = 2.0f*sigma*sigma;

	float weights[9];
	float sum = 0.0f;
	for(int i = 0; i < 8; ++i)
	{
		float x = (float)i;
		weights[i] = expf(-x*x/d);

		sum += weights[i];
	}

	// Divide by the sum so all the weights add up to 1.0.
	for(int i = 0; i < 8; ++i)
	{
		weights[i] /= sum;
	}
}

void BlurFilter::SetWeights(const float weights[9])
{
}

void BlurFilter::Init(ID3D11Device* device, UINT width, UINT height, DXGI_FORMAT format)
{
	// Start fresh.
	ReleaseCOM(mBlurredOutputTexSRV);
	ReleaseCOM(mBlurredOutputTexUAV);

	mWidth = width;
	mHeight = height;
	mFormat = format;

	// Note, compressed formats cannot be used for UAV.  We get error like:
	// ERROR: ID3D11Device::CreateTexture2D: The format (0x4d, BC3_UNORM) 
	// cannot be bound as an UnorderedAccessView, or cast to a format that
	// could be bound as an UnorderedAccessView.  Therefore this format 
	// does not support D3D11_BIND_UNORDERED_ACCESS.

	D3D11_TEXTURE2D_DESC blurredTexDesc;
	blurredTexDesc.Width     = width;
	blurredTexDesc.Height    = height;
    blurredTexDesc.MipLevels = 1;
    blurredTexDesc.ArraySize = 1;
	blurredTexDesc.Format    = format;
	blurredTexDesc.SampleDesc.Count   = 1;
	blurredTexDesc.SampleDesc.Quality = 0;
    blurredTexDesc.Usage     = D3D11_USAGE_DEFAULT;
    blurredTexDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_RENDER_TARGET;
    blurredTexDesc.CPUAccessFlags = 0;
    blurredTexDesc.MiscFlags      = 0;

	ID3D11Texture2D* blurredTex = 0;
	HR(device->CreateTexture2D(&blurredTexDesc, 0, &blurredTex));

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
	srvDesc.Format = format;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;
	HR(device->CreateShaderResourceView(blurredTex, &srvDesc, &mBlurredOutputTexSRV));

	D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc;
	uavDesc.Format = format;
	uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
	uavDesc.Texture2D.MipSlice = 0;
	HR(device->CreateUnorderedAccessView(blurredTex, &uavDesc, &mBlurredOutputTexUAV));

	HR(device->CreateRenderTargetView(blurredTex, nullptr, &mBlurredOutputTexRTV));

	// Views save a reference to the texture so we can release our reference.
	ReleaseCOM(blurredTex);

	BuildWeight(5);
}

void BlurFilter::BuildWeight(int radius)
{
	const int max_coeff = 50;
	int kernel_n = (radius)*2+1;
	// 좌우 2개씩 뺀다. 좀더 가중치 높은 것들만 남기기 위해
	int selected_row = kernel_n + 3; 
	assert(max_coeff > kernel_n);
	int coeff[max_coeff][max_coeff];
	memset(coeff, 0, sizeof(coeff));
	auto fill_coeff = [&](int n) {
		coeff[n][0] = coeff[n][n] = 1;
		for (int i = 1; i < n; i++)
			coeff[n][i] = coeff[n-1][i-1] + coeff[n-1][i];
	};
	for (int i = 0; i <= selected_row; i++)
		fill_coeff(i);

	std::vector<int> coeffients;
	int* st = coeff[selected_row] + 2;
	int* en = st + kernel_n;
	std::copy(st, en, std::back_inserter(coeffients));
	int sum = std::accumulate(coeffients.begin(), coeffients.end(), 0);
	assert(sum != 0);

	// Half sized kernel
	int half_size = 1 + radius;
	std::vector<float> offset(half_size), weight(half_size);
	for (int i = 0; i < half_size; i++) {
		offset[i] = static_cast<float>(i);
		weight[i] = static_cast<float>(coeffients[i+radius])/sum;
	}
	weight = { 0.2f, 0.1f, 0.1f, 0.1f, 0.05f, 0.05f };

	// Quarter sized kernel
	// even number 는 0 번을 빼고 나머지를 따로 하면 된다
	// odd number 는 (-3,-2), (-1, 0),... (3) 으로 묶으면 된다
	if (radius % 2 == 1) {
		std::vector<float> qoffset, qweight;
		for (int i = -radius; i < radius; i+=2) {
			auto l = abs(i), r = abs(i+1);
			qweight.push_back(weight[l] + weight[r]);
			qoffset.push_back(i*weight[l] + (i+1)*weight[r]);
			qoffset.back() /= qweight.back();
		}
		qoffset.push_back(radius);
		qweight.push_back(weight[radius]);
	} else {
		int quarter_size = radius/2 + 1;
		std::vector<float> qoffset(quarter_size), qweight(quarter_size);
		qoffset[0] = 0.f;
		qweight[0] = weight[0];
		for (int i = 0; i < quarter_size-1; i++) {
			qweight[1+i] = weight[1+2*i] + weight[1+2*i+1];
			qoffset[1+i] = offset[1+2*i]*weight[1+2*i] + offset[1+2*i+1]*weight[1+2*i+1];
			qoffset[1+i] /= qweight[1+i];
		}
	}
}

void BlurFilter::BlurInPlace(ID3D11DeviceContext* dc, 
							 ID3D11ShaderResourceView* inputSRV, 
	                         ID3D11RenderTargetView* inputRTV,
							 ID3D11DepthStencilView* depthSten,
							 int blurCount)
{
	//
	// Run the compute shader to blur the offscreen texture.
	// 

	for(int i = 0; i < blurCount; ++i)
	{
		dc->ClearDepthStencilView(depthSten, D3D11_CLEAR_DEPTH|D3D11_CLEAR_STENCIL, 1.0f, 0);

		ID3D11RenderTargetView* nullRT[1] = { nullptr };
		dc->OMSetRenderTargets(1, nullRT, depthSten);

		// HORIZONTAL blur pass.
		std::vector<ID3D11ShaderResourceView*> SRVSlot = { inputSRV };
		dc->PSSetShaderResources(0, 1, SRVSlot.data());

		ID3D11RenderTargetView* rt[1] = { mBlurredOutputTexRTV };
		dc->OMSetRenderTargets(1, rt, depthSten);
		dc->VSSetShader(reinterpret_cast<ID3D11VertexShader*>(mShader[0]), nullptr, 0);
		dc->PSSetShader(reinterpret_cast<ID3D11PixelShader*>(mShader[1]), nullptr, 0);
		dc->DrawIndexed(6, 0, 0);
	
		dc->ClearDepthStencilView(depthSten, D3D11_CLEAR_DEPTH|D3D11_CLEAR_STENCIL, 1.0f, 0);

		// Unbind the input texture from the CS for good housekeeping.
		ID3D11ShaderResourceView* nullSRV[1] = { 0 };
		dc->PSSetShaderResources( 0, 1, nullSRV );

		rt[0] = inputRTV;
		dc->OMSetRenderTargets(1, rt, depthSten);

		// VERTICAL blur pass.
		SRVSlot[0] = { mBlurredOutputTexSRV };
		dc->PSSetShaderResources(0, 1, SRVSlot.data());

		dc->VSSetShader(reinterpret_cast<ID3D11VertexShader*>(mShader[0]), nullptr, 0);
		dc->PSSetShader(reinterpret_cast<ID3D11PixelShader*>(mShader[2]), nullptr, 0);
		dc->DrawIndexed(6, 0, 0);

		dc->PSSetShaderResources( 0, 1, nullSRV );
	}
}

void BlurFilter::SetShader(std::vector<ID3D11DeviceChild*> shader)
{
	mShader = shader;	
}
