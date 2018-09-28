#include <stdafx.h>
#include <chrono>
#include "clSpatialFilterRenderer.h"

CLSpatialFilterRenderer::CLSpatialFilterRenderer(boost::shared_ptr<SequenceRenderer> sequenceRenderer, ShaderManager::P shaderManager, KernelManager::P _kernelManager, SpatialFilter::P _spatialFilter, unsigned int width, unsigned int height, FFTChannelMode channelMode)
	: SpatialFilterRenderer(sequenceRenderer, shaderManager, _kernelManager, _spatialFilter)
	, fft(width, height, channelMode)
	, fft_prepare(width, height, channelMode)
	, width(width)
	, height(height)
{
	this->channelMode = channelMode;
	copyShader = shaderManager->loadShader(R"GLSLC0D3(
			#version 150 compatibility
	    	#extension GL_ARB_texture_rectangle : enable
			precision highp float;
			uniform ivec2 offset;
			uniform ivec2 fftSize;
			uniform vec2 pixelRatio;
			uniform float pixelArea;
			uniform sampler2DRect srcrg;
			uniform sampler2DRect srcba;
			uniform bool clFFT;
			in vec2 fTexCoord;
			out vec4 outcolor;
			void main() {
				vec2 uv = gl_FragCoord.xy * pixelRatio;
				uv = mod(uv + offset, vec2(fftSize)) + vec2(1, 1);
				outcolor = texture2DRect(srcrg, uv) * pixelArea;
			}
		)GLSLC0D3"
	);
	current_fft = &fft;
	other_fft = &fft_prepare;
}

void CLSpatialFilterRenderer::multiply(OPENCLFFT* fft)
{
	cl_mem filterr;
	cl_mem imager;

	if(kernelManager->getKernelChannels(spatialFilter, filterr))
	{
		fft->get_channels(imager);
		size_t size[1] = {(width / 2 + 1) * height};
		if(channelMode == FFTChannelMode::Monochrome)
			OpenCLCore::Get()->MultiplyMonoFFT(fft->getQueue(), imager, filterr, size);
		else
			OpenCLCore::Get()->MultiplyFFT(fft->getQueue(), imager, filterr, size); // Filter is monochrome, stored in red
	}
}

void CLSpatialFilterRenderer::fftConvolution()
{
	auto start = std::chrono::system_clock::now();
	// Fourier transformation

	if(other_fft->HasImageObject())
		return;

	if(firstFrame)
	{
		firstFrame = false;
		if(!spatialFilter->stimulusGivenInFrequencyDomain)
			current_fft->do_fft();

		multiply(current_fft);

		if(!spatialFilter->showFft)
		{
			current_fft->do_inverse_fft();
		}
	}

	auto end = std::chrono::system_clock::now();
	std::chrono::duration<double> drawelapsedSeconds = end - start;
	

	start = std::chrono::system_clock::now();

	if(!spatialFilter->stimulusGivenInFrequencyDomain)
		other_fft->do_fft();

	end = std::chrono::system_clock::now();
	std::chrono::duration<double> fftelapsedSeconds = end - start;

	start = std::chrono::system_clock::now();

	// Multiply in frequency domain
	multiply(other_fft);

	end = std::chrono::system_clock::now();
	std::chrono::duration<double> melapsedSeconds = end - start;

	start = std::chrono::system_clock::now();

	// Do inverse fft
	if(!spatialFilter->showFft)
	{
		if(other_fft->HasImageObject())
			other_fft->do_inverse_fft();
		else
		{
			std::cout << "****************************************************************" << std::endl;
			std::cout << "Error other_fft hasn't got image object, so there was no do_fft!" << std::endl;
			std::cout << "****************************************************************" << std::endl;
		}
		if(current_fft->HasImageObject())
		{
			current_fft->finishConv();
		}
	}

	end = std::chrono::system_clock::now();
	std::chrono::duration<double> ielapsedSeconds = end - start;
	/*std::cout << "   Length of draw for clfft: " << drawelapsedSeconds.count() * 1000 << "ms." << std::endl;
	std::cout << "   Length of fft for clfft: " << fftelapsedSeconds.count() * 1000 << "ms." << std::endl;
	std::cout << "   Length of multiply for clfft: " << melapsedSeconds.count() * 1000 << "ms." << std::endl;
	std::cout << "   Length of inverse fft for clfft: " << ielapsedSeconds.count() * 1000 << "ms." << std::endl;*/

}

void CLSpatialFilterRenderer::bindTexture(Shader* shader)
{
	copyShader->bindUniformTextureRect("srcrg", current_fft->get_fullTex(), 0);
	
}

void CLSpatialFilterRenderer::prepareNext()
{
	current_fft->set_input(renderStim);
	current_fft->redraw_input();
	std::swap(current_fft, other_fft);
}

void CLSpatialFilterRenderer::initFirstFrames(std::function<void(int)> stim)
{
	firstFrame = true;
	current_fft->set_input(stim);
	current_fft->redraw_input();

	other_fft->set_input([&stim](int) { stim(2); });
	other_fft->redraw_input();

	/*if(!spatialFilter->stimulusGivenInFrequencyDomain)
		current_fft->do_fft();

	multiply(current_fft);

	if(!spatialFilter->showFft)
	{
		current_fft->do_inverse_fft();
	}*/
}