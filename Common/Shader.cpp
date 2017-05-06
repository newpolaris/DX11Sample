#include "Shader.h"

#include "d3dApp.h"
#include "ShaderFactoryDX11.h"

using Microsoft::WRL::ComPtr;

ShaderDX11::ShaderDX11(ShaderType Type, std::string Name, D3D_SHADER_MACRO * Defines, const std::wstring & Filename, const std::string & Function, const std::string & Model)
{
	m_ShaderType = Type;
	m_Name = Name;

	m_CompiledShader = ShaderFactoryDX11::CompileShader(Filename, Defines, Function, Model);

	switch (Type) {
		case VertexShader:
		{
			ComPtr<ID3D11VertexShader> Shader;
			HR(g_Device->CreateVertexShader(
				m_CompiledShader->GetBufferPointer(),
				m_CompiledShader->GetBufferSize(),
				nullptr,
				Shader.GetAddressOf()));
			m_Shader.Swap(Shader);
			break;
		}
		case PixelShader:
		{
			ComPtr<ID3D11PixelShader> Shader;
			HR(g_Device->CreatePixelShader(
				m_CompiledShader->GetBufferPointer(),
				m_CompiledShader->GetBufferSize(),
				nullptr,
				Shader.GetAddressOf()));
			m_Shader.Swap(Shader);
			break;
		}
		case GeometryShader:
		{
			ComPtr<ID3D11GeometryShader> Shader;
			HR(g_Device->CreateGeometryShader(
				m_CompiledShader->GetBufferPointer(),
				m_CompiledShader->GetBufferSize(),
				nullptr,
				Shader.GetAddressOf()));
			m_Shader.Swap(Shader);
			break;
		}
		case ComputeShader:
		{
			ComPtr<ID3D11ComputeShader> Shader;
			HR(g_Device->CreateComputeShader(
				m_CompiledShader->GetBufferPointer(),
				m_CompiledShader->GetBufferSize(),
				nullptr,
				Shader.GetAddressOf()));
			m_Shader.Swap(Shader);
			break;
		}
	};
	m_Shader->SetPrivateData(WKPDID_D3DDebugObjectName, Name.size(), Name.c_str());

	FillReflection();
}

void ShaderDX11::FillReflection()
{
	ComPtr<ID3D11ShaderReflection> pReflector; 
	HR(D3DReflect( m_CompiledShader->GetBufferPointer(), m_CompiledShader->GetBufferSize(),
		IID_ID3D11ShaderReflection, reinterpret_cast<void**>( pReflector.GetAddressOf() )));

	// Get the top level shader information, including the number of constant buffers,
	// as well as the number bound resources (constant buffers + other objects), the
	// number of input elements, and the number of output elements for the shader.

	D3D11_SHADER_DESC desc;
	pReflector->GetDesc( &desc );

	// Get the input and output signature description arrays.  These can be used later
	// for verification of linking shader stages together.
	for ( UINT i = 0; i < desc.InputParameters; i++ )
	{
		D3D11_SIGNATURE_PARAMETER_DESC input_desc;
		pReflector->GetInputParameterDesc( i, &input_desc );
		m_InputSignatureParameters.push_back( input_desc );
	}
	for ( UINT i = 0; i < desc.OutputParameters; i++ )
	{
		D3D11_SIGNATURE_PARAMETER_DESC output_desc;
		pReflector->GetOutputParameterDesc( i, &output_desc );
		m_OutputSignatureParameters.push_back( output_desc );
	}

	// Get the constant buffer information, which will be used for setting parameters
	// for use by this shader at rendering time.
	for ( UINT i = 0; i < desc.ConstantBuffers; i++ )
	{
		ID3D11ShaderReflectionConstantBuffer* pConstBuffer = pReflector->GetConstantBufferByIndex( i );
		ConstantBufferLayout BufferLayout;

		D3D11_SHADER_BUFFER_DESC bufferDesc;
		pConstBuffer->GetDesc( &bufferDesc );
		BufferLayout.Description = bufferDesc;

		// Load the description of each variable for use later on when binding a buffer
		for (UINT j = 0; j < BufferLayout.Description.Variables; j++)
		{
			// Get the variable description and store it
			ID3D11ShaderReflectionVariable* pVariable = pConstBuffer->GetVariableByIndex(j);
			D3D11_SHADER_VARIABLE_DESC var_desc;
			pVariable->GetDesc(&var_desc);

			BufferLayout.Variables.push_back(var_desc);

			// Get the variable type description and store it
			ID3D11ShaderReflectionType* pType = pVariable->GetType();
			D3D11_SHADER_TYPE_DESC type_desc;
			pType->GetDesc(&type_desc);

			BufferLayout.Types.push_back(type_desc);
		}
		m_BufferDescription.push_back(BufferLayout);
	}

	for ( UINT i = 0; i < desc.BoundResources; i++ )
	{
		D3D11_SHADER_INPUT_BIND_DESC resource_desc;
		pReflector->GetResourceBindingDesc( i, &resource_desc );
		m_ResourceDescrition.push_back({ resource_desc });
	}
}

bool ShaderDX11::ShaderCheckResource(ShaderType Type, D3D_SHADER_INPUT_TYPE InputType, UINT Slot, std::string Name)
{
	for (UINT i = 0; i < m_ResourceDescrition.size(); i++) {
		if (m_ResourceDescrition[i].Type == Type && m_ResourceDescrition[i].BindPoint == Slot) {
			assert(m_ResourceDescrition[i].Name == Name);
			if (m_ResourceDescrition[i].Name == Name) 
				return true;
		}
	}
	return false;
}
