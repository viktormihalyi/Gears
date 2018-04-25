#include "stdafx.h"
#include "openCLCore.h"

const char* multiplyFFTMonoProgram =
"__kernel void multiplyFFTMono(\n"
"__global float2* lhs,\n"
"__global float2* rhs)\n"
"{\n"
"int i = get_global_id( 0 );\n"
"lhs[i] = (float2)(lhs[i].x * rhs[i].x - lhs[i].y * rhs[i].y, lhs[i].x * rhs[i].y + lhs[i].y * rhs[i].x);\n"
"}\n";

const char* multiplyFFTProgram = R"(
__kernel void multiplyFFT(
__global float2* lhsr,
__global float2* lhsg,
__global float2* lhsb,
__global float2* rhsr,
__global float2* rhsg,
__global float2* rhsb)
{
	int i = get_global_id(0); 
	lhsr[i] = (float2)(lhsr[i].x * rhsr[i].x - lhsr[i].y * rhsr[i].y, lhsr[i].x * rhsr[i].y + lhsr[i].y * rhsr[i].x);
	lhsg[i] = (float2)(lhsg[i].x * rhsg[i].x - lhsg[i].y * rhsg[i].y, lhsg[i].x * rhsg[i].y + lhsg[i].y * rhsg[i].x);
	lhsb[i] = (float2)(lhsb[i].x * rhsb[i].x - lhsb[i].y * rhsb[i].y, lhsb[i].x * rhsb[i].y + lhsb[i].y * rhsb[i].x);
})";

OpenCLCore::OpenCLCore()
{
	using OpenCLHelper::clPrintError;

	/* Setup OpenCL environment.  */
	cl_int err;
	cl_platform_id platform = 0;
	char platform_name[128];
	char device_name[128];
	cl_context_properties props[7];

	err = clGetPlatformIDs( 1, &platform, NULL );
	clPrintError( err );

	size_t ret_param_size = 0;
	err = clGetPlatformInfo( platform, CL_PLATFORM_NAME,
		sizeof( platform_name ), platform_name,
		&ret_param_size );
	clPrintError( err );
	//std::cout << "Platform found: " << platform_name << std::endl;

	err = clGetDeviceIDs( platform, CL_DEVICE_TYPE_DEFAULT, 1, &device, NULL );
	clPrintError( err );

	err = clGetDeviceInfo( device, CL_DEVICE_NAME,
		sizeof( device_name ), device_name,
		&ret_param_size );
	clPrintError( err );
	//std::cout << "Device found on the above platform: " << device_name << std::endl;

	char device_extensions[1024];
	err = clGetDeviceInfo( device, CL_DEVICE_EXTENSIONS,
		sizeof( device_extensions ), device_extensions,
		&ret_param_size );
	clPrintError( err );
	// std::cout << ret_param_size << "  " << device_extensions << std::endl;

	props[0] = CL_CONTEXT_PLATFORM;
	props[1] = (cl_context_properties) platform;
	props[2] = CL_GL_CONTEXT_KHR;
	props[3] = (cl_context_properties) wglGetCurrentContext();
	props[4] = CL_WGL_HDC_KHR;
	props[5] = (cl_context_properties) GetDC( GetActiveWindow() );
	props[6] = 0;

	ctx = clCreateContext( props, 1, &device, NULL, NULL, &err );
	clPrintError( err );

	queue = clCreateCommandQueue( ctx, device, 0, &err );
	clPrintError( err );

	/* Setup clFFT. */
	clfftSetupData fftSetup;
	err = clfftInitSetupData( &fftSetup );
	clPrintError( err );
	err = clfftSetup( &fftSetup );
	clPrintError( err );
}

OpenCLCore* OpenCLCore::Get()
{
	if ( !_instance )
	{
		_instance = new OpenCLCore();
		RegistKernel( "multiplyFFTMono", multiplyFFTMonoProgram );
		RegistKernel( "multiplyFFT", multiplyFFTProgram );
	}
	return _instance;
}

cl_kernel OpenCLCore::GetKernel( std::string name )
{
	auto it = _instance->kernels.find( name );
	if ( it == _instance->kernels.end() )
		return nullptr;

	if ( !it->second.kernel )
	{
		it->second.kernel = CompileKernel( it->first.c_str(), it->second.source, it->second.sourceSize );
		if ( !it->second.kernel )
			return nullptr;
	}
	return it->second.kernel;
}

void OpenCLCore::RegistKernel( std::string name, const char* source, size_t sourceSize, bool compile )
{
	auto it = _instance->kernels.find( name );
	if ( it != _instance->kernels.end() )
		return;
	OpenCLKernelEntry entry;
	entry.source = source;
	entry.sourceSize = sourceSize == 0 ? strlen( source ) : sourceSize;
	if ( compile )
	{
		entry.kernel = CompileKernel( name.c_str(), source, sourceSize );
	}
	_instance->kernels[name] = entry;
}

cl_kernel OpenCLCore::CompileKernel( const char* name, const char* source, size_t sourceSize )
{
	using OpenCLHelper::clPrintError;

	cl_kernel kernel;

	// Build opencl program
	cl_int err;
	char *program_log;
	size_t log_size;

	cl_program program = clCreateProgramWithSource(
		_instance->ctx, 1, (const char**) &source, &sourceSize, &err );
	if ( err < 0 )
	{
		std::cout << "Error::clCreateProgramWithSource" << err << std::endl;
		exit( 0 );
	}

	err = clBuildProgram( program, 0, NULL, NULL, NULL, NULL );
	if ( err )
	{
		clGetProgramBuildInfo(
			program,
			_instance->device,
			CL_PROGRAM_BUILD_LOG,
			0,
			NULL,
			&log_size
		);

		program_log = (char*) malloc( log_size + 1 );

		program_log[log_size] = '\0';

		clGetProgramBuildInfo(
			program,
			_instance->device,
			CL_PROGRAM_BUILD_LOG,
			log_size + 1,
			program_log,
			NULL
		);

		clPrintError( err );
		std::cout << program_log << std::endl;

		free( program_log );
		return nullptr;
	}

	kernel = clCreateKernel( program, name, &err );
	if ( err )
	{
		clPrintError( err );
		return nullptr;
	}

	return kernel;
}

void OpenCLCore::MultiplyFFT( cl_mem lhs, cl_mem rhs, size_t* imageSize, size_t* localWorkSize )
{
	using OpenCLHelper::clPrintError;
	cl_kernel multiplyFFTKernel = OpenCLCore::Get()->GetKernel( "multiplyFFTMono" );

	if ( !multiplyFFTKernel )
		return;
	cl_int err;
	err = clSetKernelArg( multiplyFFTKernel, 0, sizeof( cl_mem ), &lhs );
	clPrintError( err );
	err = clSetKernelArg( multiplyFFTKernel, 1, sizeof( cl_mem ), &rhs );
	clPrintError( err );

	size_t defaultLocal[1] = { 64 };
	size_t global[1] = { imageSize[0] / 2 };
	size_t* local;

	if ( localWorkSize )
	{
		local = localWorkSize;
	}
	else
	{
		local = defaultLocal;
	}

	err = clEnqueueNDRangeKernel( _instance->queue, multiplyFFTKernel, 1, 0, global, local, 0, NULL, NULL );
	clPrintError( err );
}

void OpenCLCore::MultiplyFFT( cl_mem lhsr, cl_mem lhsg, cl_mem lhsb, cl_mem rhsr, cl_mem rhsg, cl_mem rhsb, size_t* imageSize, size_t* localWorkSize )
{
	using OpenCLHelper::clPrintError;
	cl_kernel multiplyFFTKernel = OpenCLCore::Get()->GetKernel( "multiplyFFT" );

	if ( !multiplyFFTKernel )
		return;
	cl_int err;
	err = clSetKernelArg( multiplyFFTKernel, 0, sizeof( cl_mem ), &lhsr );
	clPrintError( err );
	err = clSetKernelArg( multiplyFFTKernel, 1, sizeof( cl_mem ), &lhsg );
	clPrintError( err );
	err = clSetKernelArg( multiplyFFTKernel, 2, sizeof( cl_mem ), &lhsb );
	clPrintError( err );
	err = clSetKernelArg( multiplyFFTKernel, 3, sizeof( cl_mem ), &rhsr );
	clPrintError( err );
	err = clSetKernelArg( multiplyFFTKernel, 4, sizeof( cl_mem ), &rhsg );
	clPrintError( err );
	err = clSetKernelArg( multiplyFFTKernel, 5, sizeof( cl_mem ), &rhsb );
	clPrintError( err );

	size_t defaultLocal[1] = { 64 };
	size_t global[1] = { imageSize[0] };
	size_t* local;

	if ( localWorkSize )
	{
		local = localWorkSize;
	}
	else
	{
		local = defaultLocal;
	}

	err = clEnqueueNDRangeKernel( _instance->queue, multiplyFFTKernel, 1, 0, global, local, 0, NULL, NULL );
	clPrintError( err );
}

void OpenCLCore::Destroy()
{
	if ( _instance )
	{
		/* Release clFFT library. */
		clfftTeardown();

		clReleaseContext( _instance->ctx );
		clReleaseDevice( _instance->device );
		for ( auto k : _instance->kernels )
		{
			if ( k.second.kernel )
				clReleaseKernel( k.second.kernel );
		}
		_instance->kernels.clear();

		delete _instance;
		_instance = nullptr;
	}
}

void OpenCLCore::getClData(cl_mem mem, float* data, unsigned size)
{
	cl_int err = clEnqueueReadBuffer(queue, mem, CL_TRUE, 0, size * sizeof(float), data, 0, NULL, NULL);
}

OpenCLCore* OpenCLCore::_instance = nullptr;