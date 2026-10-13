#include "pch.h"

#include <string>
#include "dxgraputils.h"
#include "../[Lib]__MfcEx/Sources/RANPARAM.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#endif

namespace DxGrapUtils
{
	std::string strCapturePath;

	BOOL ValidDirectory ( LPCTSTR szPath )
	{
		//	Note : 종전 활성 디렉토리 경로 백업.
		DWORD dwOldLenth = 0;
		char szOldPath[MAX_PATH] = "";
		dwOldLenth = ::GetCurrentDirectory ( MAX_PATH, szOldPath );

		//	Note : 경로 유효성 검사.
		BOOL bValid = ::SetCurrentDirectory ( szPath );
		
		//	Note : 활성 디렉토리 경로 되돌림.
		if ( dwOldLenth )	::SetCurrentDirectory ( szOldPath );

		return bValid;
	}

	void SetPath( LPCTSTR szPath )
	{
		//if( RANPARAM::emSERVICE_TYPE == EMSERVICE_CHINA )
		//{
		//	if ( ValidDirectory(szPath3) )		strCapturePath = szPath3;
		//	else								strCapturePath = "c:\\";
		//}
		//else
		//{
		//	if ( ValidDirectory(szPath1) )			strCapturePath = szPath1;
		//	else if ( ValidDirectory(szPath2) )		strCapturePath = szPath2;
		//	else if ( ValidDirectory(szPath3) )		strCapturePath = szPath3;
		//	else									strCapturePath = "c:\\";
		//}

		if( ValidDirectory( szPath ) )
			strCapturePath = szPath;
		else
			strCapturePath = _T("c:\\");
	}

	HRESULT CaptureScreen ( LPDIRECT3DDEVICEQ pDev )
	{
		HRESULT hr;

		CString StrPathName;

		// Use time structure to build a customized time string.
		time_t ltime;
		struct tm today;

		time(&ltime);
		localtime_s(&today, &ltime);
		
		char szTime[MAX_PATH] = "";
		// Use strftime to build a customized time string.
		strftime ( szTime, MAX_PATH, "%Y%m%d[%H%M]", &today );

		CFileFind FFind;
		for ( int i=0; i<999; i++ )
		{
			if( RANPARAM::emSERVICE_TYPE == EMSERVICE_CHINA )
			{
				StrPathName.Format ( "%s%s%s%03d.jpg", strCapturePath.c_str(), "직槿빻蹈", szTime, i );
			}
			else if ( RANPARAM::emSERVICE_TYPE == EMSERVICE_FEYA )
			{
				StrPathName.Format ( "%s%s%s%03d.jpg", strCapturePath.c_str(), "쳃 online", szTime, i );
			}
			else
			{
				StrPathName.Format ( "%s%s%s%03d.jpg", strCapturePath.c_str(), "ran", szTime, i );
			}

			if ( FFind.FindFile ( StrPathName ) )	continue;
			else									break;
		}

		hr = ScreenGrab( pDev, StrPathName.GetString(), D3DXIFF_JPG );

		return hr;
	}


	//int jpeg_convertor ()
	//{
	//	// open input and output files
	//	FILE* input_file;
	//	FILE* output_file;
	//	
	//	char* input_filename = "input.bmp";
	//	char* output_filename = "output.jpg";

	//	input_file = fopen ( input_filename, "rb" );
	//	output_file = fopen ( output_filename, "wb" );

	//	// create the JPEG compression structure. jpeg_compress_struct cinfo;
	//	// Initialize the JPEG compression object with default error
	//	// handling.
	//	jpeg_error_mgr jerr;
	//	cinfo.err = jpeg_std_error(&jerr);
	//	jpeg_create_compress(&cinfo);

	//	// set the default compression parameters. IJG's defaults produce
	//	// a "standard" JFIF image with 4:1:1 subsampling in a YUV color
	//	// space at average quality.
	//	cinfo.in_color_space = JCS_RGB;
	//	jpeg_set_defaults(&cinfo);

	//	// Set the input format for our source manager as a Windows
	//	// Bitmap file
	//	cjpeg_source_ptr src_mgr = jinit_read_bmp(&cinfo);
	//	src_mgr->input_file = input_file;

	//	// Read the input file header to obtain file size & colorspace.
	//	(*src_mgr->start_input) (&cinfo, src_mgr);

	//	// Now that we know input colorspace, fix colorspace-dependent
	//	// defaults
	//	jpeg_default_colorspace(&cinfo);

	//	// Specify data destination for compression
	//	jpeg_stdio_dest(&cinfo, output_file);

	//	// Start compressor
	//	jpeg_start_compress(&cinfo, TRUE);

	//	// Process data
	//	while (cinfo.next_scanline < cinfo.image_height)
	//	{
	//		// read data from the source into the source buffer
	//		// num_scanlines will be read.
	//		int num_scanlines = (*src_mgr->get_pixel_rows) ( &cinfo, src_mgr );

	//		// write num_scanlines scanlines of data from the source
	//		// manager input buffer (src_mgr->buffer) to the JPEG file.
	//		jpeg_write_scanlines ( &cinfo, src_mgr->buffer, num_scanlines );
	//	}

	//	// Finish reading data from the Bitmap and writing it to
	//	// the JPEG image.
	//	(*src_mgr->finish_input) (&cinfo, src_mgr);
	//	jpeg_finish_compress(&cinfo);
	//	
	//	// Clean up the JPEG Compressor structure.
	//	jpeg_destroy_compress(&cinfo);

	//	// close the input and output files fclose(input_file);
	//	fclose(output_file);
	//	
	//	return 0;
	//}

	HRESULT ScreenGrab ( LPDIRECT3DDEVICEQ pDev, LPCTSTR fileName, D3DXIMAGE_FILEFORMAT emFormat )
	{ 
		HRESULT hr;

		// get display dimensions 
		// this will be the dimensions of the front buffer 
		D3DDISPLAYMODE mode;
		hr = pDev->GetDisplayMode( 0, &mode );
		if ( FAILED(hr) )
		{
			CDebugSet::ToLogFile ( "ERROR : DxGrapUtils::ScreenGrab()-GetDisplayMode() %s", DXGetErrorString9(hr) );
			return hr;
		}

		// create the image surface to store the front buffer image 
		// note that call to GetFrontBuffer will always convert format to A8R8G8B8 
		LPDIRECT3DSURFACEQ surf; 
		hr = pDev->CreateOffscreenPlainSurface( mode.Width, mode.Height, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &surf, NULL );
		if ( FAILED(hr) )
		{
			CDebugSet::ToLogFile ( "ERROR : DxGrapUtils::ScreenGrab()-CreateImageSurface() %s", DXGetErrorString9(hr) );
			return hr;
		}

		// read the front buffer into the image surface 
		hr = pDev->GetFrontBufferData( 0, surf );
		//pDev->GetBackBuffer ( 0, D3DBACKBUFFER_TYPE_MONO, &surf );
		if ( FAILED(hr) )
		{ 
			CDebugSet::ToLogFile ( "ERROR : DxGrapUtils::ScreenGrab()-GetFrontBuffer() %s", DXGetErrorString9(hr) );
			surf->Release();
			return hr; 
		} 

		// write the entire surface to the requested file 
		hr = D3DXSaveSurfaceToFile(fileName,emFormat,surf,NULL,NULL); 
		if ( FAILED(hr) )
		{ 
			CDebugSet::ToLogFile ( "ERROR : DxGrapUtils::ScreenGrab()-SaveFile() %s, %s", fileName, DXGetErrorString9(hr) );
			surf->Release();
			return hr; 
		} 

		// release the image surface 
		surf->Release(); 

		// return status of save operation to caller 
		return hr; 
	} 
};