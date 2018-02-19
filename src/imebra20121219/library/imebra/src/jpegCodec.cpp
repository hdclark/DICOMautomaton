/*

Imebra 2011 build 2012-12-19_20-05-13

Imebra: a C++ Dicom library

Copyright (c) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010 by Paolo Brandoli
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
 are permitted provided that the following conditions are met:
- Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE FREEBSD PROJECT ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE FREEBSD PROJECT OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

The views and conclusions contained in the software and documentation are those of the authors
 and should not be interpreted as representing official policies, either expressed or implied,
 of the Imebra Project.

Imebra is available at http://imebra.com



*/

/*! \file jpegCodec.cpp
	\brief Implementation of the class jpegCodec.

*/

#include "../../base/include/exception.h"
#include "../../base/include/streamReader.h"
#include "../../base/include/streamWriter.h"
#include "../../base/include/huffmanTable.h"
#include "../include/jpegCodec.h"
#include "../include/dataSet.h"
#include "../include/image.h"
#include "../include/dataHandlerNumeric.h"
#include <vector>
#include <cstdlib>
#include <cstring>

namespace puntoexe
{

namespace imebra
{

namespace codecs
{

static registerCodec m_registerCodec(ptr<codec>(new jpegCodec));

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
//
// Default luminance or RGB quantization table
//
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
static const imbxUint32 JpegStdLuminanceQuantTbl[] =
{
	16,  11,  10,  16,  24,  40,  51,  61,
		12,  12,  14,  19,  26,  58,  60,  55,
		14,  13,  16,  24,  40,  57,  69,  56,
		14,  17,  22,  29,  51,  87,  80,  62,
		18,  22,  37,  56,  68, 109, 103,  77,
		24,  35,  55,  64,  81, 104, 113,  92,
		49,  64,  78,  87, 103, 121, 120, 101,
		72,  92,  95,  98, 112, 100, 103,  99
};


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
//
// Default chrominance quantization table
//
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
static const imbxUint32 JpegStdChrominanceQuantTbl[] =
{
	17,  18,  24,  47,  99,  99,  99,  99,
		18,  21,  26,  66,  99,  99,  99,  99,
		24,  26,  56,  99,  99,  99,  99,  99,
		47,  66,  99,  99,  99,  99,  99,  99,
		99,  99,  99,  99,  99,  99,  99,  99,
		99,  99,  99,  99,  99,  99,  99,  99,
		99,  99,  99,  99,  99,  99,  99,  99,
		99,  99,  99,  99,  99,  99,  99,  99
};


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
//
// Default scale factors for FDCT/IDCT calculation
//
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
static const float JpegDctScaleFactor[]=
{
	(float)1.0,
		(float)1.387039845,
		(float)1.306562965,
		(float)1.175875602,
		(float)1.0,
		(float)0.785694958,
		(float)0.541196100,
		(float)0.275899379
};


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
//
// Default Huffman table for DC values of luminance channel
// (Values per length)
//
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
static const int JpegBitsDcLuminance[]=
{ 0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0};


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
//
// Default Huffman table for DC values of luminance channel
// (Values to code)
//
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
static const imbxUint32 JpegValDcLuminance[]=
{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
//
// Default Huffman table for DC values of chrominance
//  channel (Values per length)
//
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
static const int JpegBitsDcChrominance[]=
{ 0, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0 };


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
//
// Default Huffman table for DC values of chrominance
//  channel (Values to code)
//
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
static const imbxUint32 JpegValDcChrominance[]=
{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
//
// Default Huffman table for AC values of luminance channel
// (Values per length)
//
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
static const int JpegBitsAcLuminance[]=
{ 0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 0x7d };


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
//
// Default Huffman table for AC values of luminance channel
// (Values to code)
//
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
static const imbxUint32 JpegValAcLuminance[]=
{
	0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12,
		0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07,
		0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xa1, 0x08,
		0x23, 0x42, 0xb1, 0xc1, 0x15, 0x52, 0xd1, 0xf0,
		0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0a, 0x16,
		0x17, 0x18, 0x19, 0x1a, 0x25, 0x26, 0x27, 0x28,
		0x29, 0x2a, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
		0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
		0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
		0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
		0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,
		0x7a, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
		0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98,
		0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
		0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6,
		0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5,
		0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4,
		0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xe1, 0xe2,
		0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea,
		0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
		0xf9, 0xfa
};


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
//
// Default Huffman table for AC values of chrominance
//  channel (Values per length)
//
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
static const int JpegBitsAcChrominance[] =
{ 0, 2, 1, 2, 4, 4, 3, 4, 7, 5, 4, 4, 0, 1, 2, 0x77 };


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Default Huffman table for AC values of chrominance
//  channel (Values to code)
//
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
static const imbxUint32 JpegValAcChrominance[] =
{
	0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21,
		0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71,
		0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91,
		0xa1, 0xb1, 0xc1, 0x09, 0x23, 0x33, 0x52, 0xf0,
		0x15, 0x62, 0x72, 0xd1, 0x0a, 0x16, 0x24, 0x34,
		0xe1, 0x25, 0xf1, 0x17, 0x18, 0x19, 0x1a, 0x26,
		0x27, 0x28, 0x29, 0x2a, 0x35, 0x36, 0x37, 0x38,
		0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
		0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
		0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
		0x69, 0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
		0x79, 0x7a, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
		0x88, 0x89, 0x8a, 0x92, 0x93, 0x94, 0x95, 0x96,
		0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5,
		0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4,
		0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3,
		0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2,
		0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda,
		0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9,
		0xea, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
		0xf9, 0xfa
};


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
//
// Translate zig-zag order in 8x8 blocks to raw order
//
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
static const imbxUint32 JpegDeZigZagOrder[]=
{
	0, 1, 8, 16, 9, 2, 3, 10,
		17, 24, 32, 25, 18, 11,  4,  5,
		12, 19, 26, 33, 40, 48, 41, 34,
		27, 20, 13,  6,  7, 14, 21, 28,
		35, 42, 49, 56, 57, 50, 43, 36,
		29, 22, 15, 23, 30, 37, 44, 51,
		58, 59, 52, 45, 38, 31, 39, 46,
		53, 60, 61, 54, 47, 55, 62, 63
};

#define JPEG_DECOMPRESSION_BITS_PRECISION 14

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
//
// jpegCodec
//
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Constructor
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
jpegCodec::jpegCodec()
{
	PUNTOEXE_FUNCTION_START(L"jpegCodec::jpegCodec");

	// Resets the channels list
	///////////////////////////////////////////////////////////
	memset(m_channelsList, 0, sizeof(m_channelsList));

	// Allocate the huffman tables
	///////////////////////////////////////////////////////////
	for(int resetHuffmanTables = 0; resetHuffmanTables<16; ++resetHuffmanTables)
	{
		ptr<huffmanTable> huffmanDC(new huffmanTable(9));
		m_pHuffmanTableDC[resetHuffmanTables]=huffmanDC;

		ptr<huffmanTable> huffmanAC(new huffmanTable(9));
		m_pHuffmanTableAC[resetHuffmanTables]=huffmanAC;
	}

	// Register all the tag classes
	///////////////////////////////////////////////////////////

	// Unknown tag must be registered
	///////////////////////////////////////////////////////////
	registerTag(unknown, ptr<jpeg::tag>(new jpeg::tagUnknown));

	// Register SOF
	///////////////////////////////////////////////////////////
	registerTag(sof0, ptr<jpeg::tag>(new jpeg::tagSOF));
	registerTag(sof1, ptr<jpeg::tag>(new jpeg::tagSOF));
	registerTag(sof2, ptr<jpeg::tag>(new jpeg::tagSOF));
	registerTag(sof3, ptr<jpeg::tag>(new jpeg::tagSOF));
	registerTag(sof5, ptr<jpeg::tag>(new jpeg::tagSOF));
	registerTag(sof6, ptr<jpeg::tag>(new jpeg::tagSOF));
	registerTag(sof7, ptr<jpeg::tag>(new jpeg::tagSOF));
	registerTag(sof9, ptr<jpeg::tag>(new jpeg::tagSOF));
	registerTag(sofA, ptr<jpeg::tag>(new jpeg::tagSOF));
	registerTag(sofB, ptr<jpeg::tag>(new jpeg::tagSOF));
	registerTag(sofD, ptr<jpeg::tag>(new jpeg::tagSOF));
	registerTag(sofE, ptr<jpeg::tag>(new jpeg::tagSOF));
	registerTag(sofF, ptr<jpeg::tag>(new jpeg::tagSOF));

	// Register DHT
	///////////////////////////////////////////////////////////
	registerTag(dht, ptr<jpeg::tag>(new jpeg::tagDHT));

	// Register DQT
	///////////////////////////////////////////////////////////
	registerTag(dqt, ptr<jpeg::tag>(new jpeg::tagDQT));

	// Register SOS
	///////////////////////////////////////////////////////////
	registerTag(sos, ptr<jpeg::tag>(new jpeg::tagSOS));

	// Register EOI
	///////////////////////////////////////////////////////////
	registerTag(eoi, ptr<jpeg::tag>(new jpeg::tagEOI));

	// Register RST
	///////////////////////////////////////////////////////////
	registerTag(rst0, ptr<jpeg::tag>(new jpeg::tagRST));
	registerTag(rst1, ptr<jpeg::tag>(new jpeg::tagRST));
	registerTag(rst2, ptr<jpeg::tag>(new jpeg::tagRST));
	registerTag(rst3, ptr<jpeg::tag>(new jpeg::tagRST));
	registerTag(rst4, ptr<jpeg::tag>(new jpeg::tagRST));
	registerTag(rst5, ptr<jpeg::tag>(new jpeg::tagRST));
	registerTag(rst6, ptr<jpeg::tag>(new jpeg::tagRST));
	registerTag(rst7, ptr<jpeg::tag>(new jpeg::tagRST));

	// Register DRI
	///////////////////////////////////////////////////////////
	registerTag(dri, ptr<jpeg::tag>(new jpeg::tagDRI));

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Destructor
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
jpegCodec::~jpegCodec()
{
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Create another JPEG codec
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
ptr<codec> jpegCodec::createCodec()
{
	PUNTOEXE_FUNCTION_START(L"jpegCodec::createCodec");

	return ptr<codec>(new jpegCodec);

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Register a tag
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void jpegCodec::registerTag(tTagId tagId, ptr<jpeg::tag> pTag)
{
	PUNTOEXE_FUNCTION_START(L"jpegCodec::registerTag");

	ptr<jpeg::tag> test = pTag;
	m_tagsMap[(imbxUint8)tagId]=pTag;

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Write a jpeg stream
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void jpegCodec::writeStream(ptr<streamWriter> pStream, ptr<dataSet> pDataSet)
{
	PUNTOEXE_FUNCTION_START(L"jpegCodec::writeStream");

	lockObject lockDataSet(pDataSet.get());

	// Retrieve the transfer syntax
	///////////////////////////////////////////////////////////
	std::wstring transferSyntax = pDataSet->getUnicodeString(0x0002, 0x0, 0x0010, 0x0);

	// The buffer can be written as it is
	///////////////////////////////////////////////////////////
	if(canHandleTransferSyntax(transferSyntax))
	{
		ptr<data> imageData = pDataSet->getTag(0x7fe0, 0, 0x0010, false);
		if(imageData == nullptr || !imageData->bufferExists(0))
		{
			PUNTOEXE_THROW(dataSetImageDoesntExist, "The requested image doesn't exist");
		}
		imbxUint32 firstBufferId(0);
		imbxUint32 endBufferId(1);
		if(imageData->bufferExists(1))
		{
			pDataSet->getFrameBufferIds(0, &firstBufferId, &endBufferId);
		}
		for(imbxUint32 scanBuffers = firstBufferId; scanBuffers != endBufferId; ++scanBuffers)
		{
			ptr<handlers::dataHandlerRaw> readHandler = imageData->getDataHandlerRaw(scanBuffers, false, "");
			imbxUint8* readBuffer = readHandler->getMemoryBuffer();
			pStream->write(readBuffer, readHandler->getSize());
		}
		return;

	}

	// Get the image then write it
	///////////////////////////////////////////////////////////
	ptr<image> decodedImage = pDataSet->getImage(0);
	std::wstring defaultTransferSyntax(L"1.2.840.10008.1.2.4.50"); // baseline (8 bits lossy)
	setImage(pStream, decodedImage, defaultTransferSyntax, codecs::codec::high, "OB", 8, true, true, false, false);

	PUNTOEXE_FUNCTION_END();

}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Erase all the allocated channels
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void jpegCodec::eraseChannels()
{
	PUNTOEXE_FUNCTION_START(L"jpegCodec::eraseChannels");

	m_channelsMap.clear();
	memset(m_channelsList, 0, sizeof(m_channelsList));

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Reset all the internal variables
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void jpegCodec::resetInternal(bool bCompression, quality compQuality)
{
	PUNTOEXE_FUNCTION_START(L"jpegCodec::resetInternal");

	// Factor used to calculate the quantization tables used
	//  for the compression
	///////////////////////////////////////////////////////////
	float compQuantization = (float)compQuality / (float)medium;

	eraseChannels();

	m_imageSizeX = m_imageSizeY = 0;

	m_precision = 8;
	m_valuesMask = ((imbxInt32)1 << m_precision)-1;

	m_process = 0;

	m_mcuPerRestartInterval = 0;

	m_mcuLastRestart = 0;

	m_spectralIndexStart = 0;
	m_spectralIndexEnd = 63;
	m_bitHigh = 0;
	m_bitLow = 0;

	m_bLossless = false;

	// The number of MCUs (horizontal, vertical, total)
	///////////////////////////////////////////////////////////
	m_mcuNumberX = 0;
	m_mcuNumberY = 0;
	m_mcuNumberTotal = 0;

	m_maxSamplingFactorX = 0;
	m_maxSamplingFactorY = 0;

	m_mcuProcessed = 0;
	m_mcuProcessedX = 0;
	m_mcuProcessedY = 0;
	m_eobRun = 0;

	m_jpegImageSizeX = 0;
	m_jpegImageSizeY = 0;

	// Reset the QT tables
	///////////////////////////////////////////////////////////
	for(int resetQT = 0; resetQT<16; ++resetQT)
	{
		const imbxUint32* pSourceTable = (resetQT == 0) ? JpegStdLuminanceQuantTbl : JpegStdChrominanceQuantTbl;

		imbxUint8 tableIndex = 0;
		for(imbxUint8 row = 0; row < 8; ++row)
		{
			for(imbxUint8 col = 0; col < 8; ++col)
			{
				if(bCompression)
				{
					imbxUint32 quant = (imbxUint32) ((float)(pSourceTable[tableIndex]) * compQuantization);
					if(quant < 1)
					{
						quant = 1;
					}
					if(quant > 255)
					{
						quant = 255;
					}
					m_quantizationTable[resetQT][tableIndex++] = quant;
					continue;
				}
								m_quantizationTable[resetQT][tableIndex] = pSourceTable[tableIndex];
								++tableIndex;
			}
		}
		recalculateQuantizationTables(resetQT);
	}

	// Reset the huffman tables
	///////////////////////////////////////////////////////////
	for(int DcAc = 0; DcAc < 2; ++DcAc)
	{
		for(int resetHT=0; resetHT < 16; ++resetHT)
		{
			ptr<huffmanTable> pHuffman;
			const int* pLengthTable;
			const imbxUint32* pValuesTable;
			if(DcAc == 0)
			{
				pHuffman=m_pHuffmanTableDC[resetHT];
				if(resetHT == 0)
				{
					pLengthTable=JpegBitsDcLuminance;
					pValuesTable=JpegValDcLuminance;
				}
				else
				{
					pLengthTable=JpegBitsDcChrominance;
					pValuesTable=JpegValDcChrominance;
				}
			}
			else
			{
				pHuffman=m_pHuffmanTableAC[resetHT];
				if(resetHT == 0)
				{
					pLengthTable=JpegBitsAcLuminance;
					pValuesTable=JpegValAcLuminance;
				}
				else
				{
					pLengthTable=JpegBitsAcChrominance;
					pValuesTable=JpegValAcChrominance;
				}
			}

			pHuffman->reset();
			if(bCompression)
			{
				continue;
			}

			// Read the number of codes per length
			/////////////////////////////////////////////////////////////////
			imbxUint32 valueIndex = 0;
			for(int scanLength = 0; scanLength<16; ++scanLength)
			{
				pHuffman->m_valuesPerLength[scanLength+1]=(imbxUint32)pLengthTable[scanLength];
				for(imbxUint32 scanValues = 0; scanValues<pHuffman->m_valuesPerLength[scanLength+1]; ++scanValues)
				{
					pHuffman->m_orderedValues[valueIndex]=pValuesTable[valueIndex];
					++valueIndex;
				}
			}
			pHuffman->calcHuffmanTables();
		}
	}

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Allocate the channels. This function is called when a
//  SOF tag is found
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void jpegCodec::allocChannels()
{
	PUNTOEXE_FUNCTION_START(L"jpegCodec::allocChannels");

	m_maxSamplingFactorX=1L;
	m_maxSamplingFactorY=1L;

	m_valuesMask = ((imbxInt32)1 << m_precision)-1;

	// Find the maximum sampling factor
	///////////////////////////////////////////////////////////
	for(auto & channelsIterator0 : m_channelsMap)
	{
		ptr<jpeg::jpegChannel> pChannel=channelsIterator0.second;

		if(pChannel->m_samplingFactorX>m_maxSamplingFactorX)
			m_maxSamplingFactorX=pChannel->m_samplingFactorX;
		if(pChannel->m_samplingFactorY>m_maxSamplingFactorY)
			m_maxSamplingFactorY=pChannel->m_samplingFactorY;
	}

	if(m_bLossless)
	{
		m_jpegImageSizeX=(m_imageSizeX+(m_maxSamplingFactorX-1))/m_maxSamplingFactorX;
		m_jpegImageSizeX*=m_maxSamplingFactorX;
		m_jpegImageSizeY=(m_imageSizeY+(m_maxSamplingFactorY-1))/m_maxSamplingFactorY;
		m_jpegImageSizeY*=m_maxSamplingFactorY;
	}
	else
	{
		m_jpegImageSizeX=(m_imageSizeX+((m_maxSamplingFactorX<<3)-1))/(m_maxSamplingFactorX<<3);
		m_jpegImageSizeX*=(m_maxSamplingFactorX<<3);
		m_jpegImageSizeY=(m_imageSizeY+((m_maxSamplingFactorY<<3)-1))/(m_maxSamplingFactorY<<3);
		m_jpegImageSizeY*=(m_maxSamplingFactorY<<3);
	}

	// Allocate the channels' buffers
	///////////////////////////////////////////////////////////
	for(auto & channelsIterator1 : m_channelsMap)
	{
		ptr<jpeg::jpegChannel> pChannel=channelsIterator1.second;
		pChannel->m_defaultDCValue = m_bLossless ? ((imbxInt32)1<<(m_precision - 1)) : 0;
		pChannel->m_lastDCValue = pChannel->m_defaultDCValue;

		pChannel->allocate(
			m_jpegImageSizeX*(imbxUint32)pChannel->m_samplingFactorX/m_maxSamplingFactorX,
			m_jpegImageSizeY*(imbxUint32)pChannel->m_samplingFactorY/m_maxSamplingFactorY);
		pChannel->m_valuesMask = m_valuesMask;
	}

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Find the MCU's size
// This function is called when a SOS tag is found
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void jpegCodec::findMcuSize()
{
	PUNTOEXE_FUNCTION_START(L"jpegCodec::findMcuSize");

	// Find the maximum sampling factor for all the channels
	///////////////////////////////////////////////////////////
	imbxUint32 maxSamplingFactorChannelsX=1;
	imbxUint32 maxSamplingFactorChannelsY=1;
	for(auto & allChannelsIterator : m_channelsMap)
	{
		ptr<jpeg::jpegChannel> pChannel = allChannelsIterator.second;

		if(pChannel->m_samplingFactorX > maxSamplingFactorChannelsX)
			maxSamplingFactorChannelsX = pChannel->m_samplingFactorX;
		if(pChannel->m_samplingFactorY > maxSamplingFactorChannelsY)
			maxSamplingFactorChannelsY = pChannel->m_samplingFactorY;
	}


	// Find the minimum and maximum sampling factor in the scan
	///////////////////////////////////////////////////////////
	imbxUint32 maxSamplingFactorX=1;
	imbxUint32 maxSamplingFactorY=1;
	imbxUint32 minSamplingFactorX=256;
	imbxUint32 minSamplingFactorY=256;

	jpeg::jpegChannel* pChannel; // Used in the lòops
	for(jpeg::jpegChannel** channelsIterator = m_channelsList; *channelsIterator != nullptr; ++channelsIterator)
	{
		pChannel = *channelsIterator;

		if(pChannel->m_samplingFactorX > maxSamplingFactorX)
			maxSamplingFactorX = pChannel->m_samplingFactorX;
		if(pChannel->m_samplingFactorY > maxSamplingFactorY)
			maxSamplingFactorY = pChannel->m_samplingFactorY;
		if(pChannel->m_samplingFactorX < minSamplingFactorX)
			minSamplingFactorX = pChannel->m_samplingFactorX;
		if(pChannel->m_samplingFactorY < minSamplingFactorY)
			minSamplingFactorY = pChannel->m_samplingFactorY;
	}

	// Find the number of blocks per MCU per channel
	///////////////////////////////////////////////////////////
	for(jpeg::jpegChannel** channelsIterator = m_channelsList; *channelsIterator != nullptr; ++channelsIterator)
	{
		pChannel=*channelsIterator;

		pChannel->m_blockMcuX=pChannel->m_samplingFactorX/minSamplingFactorX;
		pChannel->m_blockMcuY=pChannel->m_samplingFactorY/minSamplingFactorY;
		pChannel->m_blockMcuXY = pChannel->m_blockMcuX * pChannel->m_blockMcuY;
		pChannel->m_losslessPositionX = 0;
		pChannel->m_losslessPositionY = 0;
		pChannel->m_unprocessedAmplitudesCount = 0;
		pChannel->m_unprocessedAmplitudesPredictor = 0;
		pChannel->m_lastDCValue = pChannel->m_defaultDCValue;
	}

	// Find the MCU size, in image's pixels
	///////////////////////////////////////////////////////////
	if(m_bLossless)
	{
		m_mcuNumberX = m_jpegImageSizeX * minSamplingFactorX / maxSamplingFactorChannelsX;
		m_mcuNumberY = m_jpegImageSizeY * minSamplingFactorY / maxSamplingFactorChannelsY;
	}
	else
	{
		imbxUint32 xBoundary = 8 * maxSamplingFactorChannelsX / minSamplingFactorX;
		imbxUint32 yBoundary = 8 * maxSamplingFactorChannelsY / minSamplingFactorY;

		m_mcuNumberX = (m_imageSizeX + xBoundary - 1) / xBoundary;
		m_mcuNumberY = (m_imageSizeY + yBoundary - 1) / yBoundary;
	}
	m_mcuNumberTotal = m_mcuNumberX*m_mcuNumberY;
	m_mcuProcessed = 0;
	m_mcuProcessedX = 0;
	m_mcuProcessedY = 0;


	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Build a DICOM dataset from a jpeg file
//
//
/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
void jpegCodec::readStream(ptr<streamReader> pSourceStream, ptr<dataSet> pDataSet, imbxUint32 /* maxSizeBufferLoad = 0xffffffff */)
{
	PUNTOEXE_FUNCTION_START(L"jpegCodec::readStream");

	streamReader* pStream = pSourceStream.get();

	// Reset the internal variables
	///////////////////////////////////////////////////////////
	resetInternal(false, medium);

	// Store the stream's position.
	// This will be used later, in order to reread all the
	//  stream's content and store it into the dataset
	///////////////////////////////////////////////////////////
	imbxUint32 startPosition=pStream->position();

	try
	{
		// Read the Jpeg signature
		///////////////////////////////////////////////////////////
		imbxUint8 jpegSignature[2];
		try
		{
			pStream->read(jpegSignature, 2);
		}
		catch(streamExceptionEOF&)
		{
			PUNTOEXE_THROW(codecExceptionWrongFormat, "readStream detected a wrong format");
		}

		// If the jpeg signature is wrong, then return an error
		//  condition
		///////////////////////////////////////////////////////////
		imbxUint8 checkSignature[2]={(imbxUint8)0xff, (imbxUint8)0xd8};
		if(::memcmp(jpegSignature, checkSignature, 2) != 0)
		{
			PUNTOEXE_THROW(codecExceptionWrongFormat, "detected a wrong format");
		}
	}
	catch(streamExceptionEOF&)
	{
		PUNTOEXE_THROW(codecExceptionWrongFormat, "detected a wrong format");
	}

	// Used to read discharged chars
	///////////////////////////////////////////////////////////
	imbxUint8 entryByte;

	// Read all the tags in the stream
	///////////////////////////////////////////////////////////
	for(m_bEndOfImage=false; !m_bEndOfImage; /* empty */)
	{
		// If a tag has been found, then parse it
		///////////////////////////////////////////////////////////
		pStream->read(&entryByte, 1);
		if(entryByte != 0xff)
		{
			continue;
		}
		do
		{
			pStream->read(&entryByte, 1);
		} while(entryByte == 0xff);

		if(entryByte != 0)
		{
			ptr<jpeg::tag> pTag;
			tTagsMap::iterator findTag = m_tagsMap.find(entryByte);
			if(findTag != m_tagsMap.end())
				pTag = findTag->second;
			else
				pTag=m_tagsMap[0xff];

			// Parse the tag
			///////////////////////////////////////////////////////////
			pTag->readTag(pStream, this, entryByte);
		}
	}

	//
	// Build the dataset
	//
	///////////////////////////////////////////////////////////

	// Color space
	///////////////////////////////////////////////////////////
	if(m_channelsMap.size()==1)
		pDataSet->setUnicodeString(0x0028, 0, 0x0004, 0, L"MONOCHROME2");
	else
		pDataSet->setUnicodeString(0x0028, 0, 0x0004, 0, L"YBR_FULL");

	// Transfer syntax
	///////////////////////////////////////////////////////////
	switch(m_process)
	{
	case 0x00:
		pDataSet->setUnicodeString(0x0002, 0, 0x0010, 0, L"1.2.840.10008.1.2.4.50");
		break;
	case 0x01:
		pDataSet->setUnicodeString(0x0002, 0, 0x0010, 0, L"1.2.840.10008.1.2.4.51");
		break;
	case 0x03:
		pDataSet->setUnicodeString(0x0002, 0, 0x0010, 0, L"1.2.840.10008.1.2.4.57");
		break;
	case 0x07:
		pDataSet->setUnicodeString(0x0002, 0, 0x0010, 0, L"1.2.840.10008.1.2.4.57");
		break;
	default:
		throw jpegCodecCannotHandleSyntax("Jpeg SOF not supported");
	}

	// Number of planes
	///////////////////////////////////////////////////////////
	pDataSet->setUnsignedLong(0x0028, 0, 0x0002, 0, (imbxUint32)m_channelsMap.size());

	// Image's width
	/////////////////////////////////////////////////////////////////
	pDataSet->setUnsignedLong(0x0028, 0, 0x0011, 0, m_imageSizeX);

	// Image's height
	/////////////////////////////////////////////////////////////////
	pDataSet->setUnsignedLong(0x0028, 0, 0x0010, 0, m_imageSizeY);

	// Number of frames
	/////////////////////////////////////////////////////////////////
	pDataSet->setUnsignedLong(0x0028, 0, 0x0008, 0, 1);

	// Pixel representation
	/////////////////////////////////////////////////////////////////
	pDataSet->setUnsignedLong(0x0028, 0x0, 0x0103, 0x0, 0);

	// Allocated, stored bits and high bit
	/////////////////////////////////////////////////////////////////
	pDataSet->setUnsignedLong(0x0028, 0x0, 0x0100, 0x0, m_precision);
	pDataSet->setUnsignedLong(0x0028, 0x0, 0x0101, 0x0, m_precision);
	pDataSet->setUnsignedLong(0x0028, 0x0, 0x0102, 0x0, m_precision - 1);

	// Interleaved (more than 1 channel in the channels list)
	/////////////////////////////////////////////////////////////////
	pDataSet->setUnsignedLong(0x0028, 0x0, 0x0006, 0x0, m_channelsList[0] != nullptr && m_channelsList[1] != nullptr);

	// Insert the basic offset table
	////////////////////////////////////////////////////////////////
	ptr<handlers::dataHandlerRaw> offsetHandler=pDataSet->getDataHandlerRaw(0x7fe0, 0, 0x0010, 0, true, "OB");
	offsetHandler->setSize(4);
	::memset(offsetHandler->getMemoryBuffer(), 0, offsetHandler->getSize());

	// Reread all the stream's content and write it into the dataset
	////////////////////////////////////////////////////////////////
	imbxUint32 finalPosition=pStream->position();
	imbxUint32 streamLength=(imbxUint32)(finalPosition-startPosition);
	pStream->seek((imbxInt32)startPosition);

	ptr<handlers::dataHandlerRaw> imageHandler=pDataSet->getDataHandlerRaw(0x7fe0, 0, 0x0010, 1, true, "OB");
	if(imageHandler != nullptr && streamLength != 0)
	{
		imageHandler->setSize(streamLength);
		pStream->read(imageHandler->getMemoryBuffer(), streamLength);
	}

	PUNTOEXE_FUNCTION_END();
}


////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
//
//
// Returns true if the codec can handle the specified transfer
//  syntax
//
//
/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
bool jpegCodec::canHandleTransferSyntax(std::wstring transferSyntax)
{
	PUNTOEXE_FUNCTION_START(L"jpegCodec::canHandleTransferSyntax");

	return (
		transferSyntax == L"1.2.840.10008.1.2.4.50" ||  // baseline (8 bits lossy)
		transferSyntax == L"1.2.840.10008.1.2.4.51" ||  // extended (12 bits lossy)
		transferSyntax == L"1.2.840.10008.1.2.4.57" ||  // lossless NH
		transferSyntax == L"1.2.840.10008.1.2.4.70");   // lossless NH first order prediction

	PUNTOEXE_FUNCTION_END();
}


////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
//
//
// Returns true if the transfer syntax has to be
//  encapsulated
//
//
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
bool jpegCodec::encapsulated(std::wstring transferSyntax)
{
	PUNTOEXE_FUNCTION_START(L"jpegCodec::canHandleTransferSyntax");

	if(!canHandleTransferSyntax(transferSyntax))
	{
		PUNTOEXE_THROW(codecExceptionWrongTransferSyntax, "Cannot handle the transfer syntax");
	}
	return true;

	PUNTOEXE_FUNCTION_END();
}


/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
//
//
// Return the highest bit that the transfer syntax can
//  handle
//
//
/////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
imbxUint32 jpegCodec::getMaxHighBit(std::string transferSyntax)
{
	PUNTOEXE_FUNCTION_START(L"jpegCodec::getMaxHighBit");

	if(transferSyntax == "1.2.840.10008.1.2.4.50")
	{
		return 7;
	}
	if(transferSyntax == "1.2.840.10008.1.2.4.51")
	{
		return 11;
	}
	return 15;

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Return the suggested allocated bits
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
imbxUint32 jpegCodec::suggestAllocatedBits(std::wstring transferSyntax, imbxUint32 highBit)
{
	PUNTOEXE_FUNCTION_START(L"jpegCodec::suggestAllocatedBits");

	if(transferSyntax == L"1.2.840.10008.1.2.4.50")
	{
		return 8;
	}
	if(transferSyntax == L"1.2.840.10008.1.2.4.51")
	{
		return 12;
	}
	return (highBit + 8) & 0xfffffff8;

	PUNTOEXE_FUNCTION_END();
}


////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
//
//
// Get a jpeg image from a Dicom dataset
//
//
/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
ptr<image> jpegCodec::getImage(ptr<dataSet> sourceDataSet, ptr<streamReader> pStream, std::string /* dataType not used */)
{
	PUNTOEXE_FUNCTION_START(L"jpegCodec::getImage");

	streamReader* pSourceStream = pStream.get();

	// Reset the internal variables
	////////////////////////////////////////////////////////////////
	resetInternal(false, medium);

	// Activate the tags in the stream
	///////////////////////////////////////////////////////////
	pSourceStream->m_bJpegTags = true;

	// Read the Jpeg signature
	///////////////////////////////////////////////////////////
	imbxUint8 jpegSignature[2];

	try
	{
		pSourceStream->read(jpegSignature, 2);
	}
	catch(streamExceptionEOF&)
	{
		PUNTOEXE_THROW(codecExceptionWrongFormat, "Jpeg signature not present");
	}

	// If the jpeg signature is wrong, then return an error
	//  condition
	///////////////////////////////////////////////////////////
	static imbxUint8 checkSignature[2]={(imbxUint8)0xff, (imbxUint8)0xd8};
	if(::memcmp(jpegSignature, checkSignature, 2) != 0)
	{
		PUNTOEXE_THROW(codecExceptionWrongFormat, "Jpeg signature not valid");
	}

	//
	// Preallocate the variables used in the loop
	//
	///////////////////////////////////////////////////////////

	int scanBlock;              // scan lossless blocks
	int scanBlockX, scanBlockY; // scan lossy blocks
	imbxUint32 amplitudeLength; // lossless amplitude's length
	imbxInt32 amplitude;        // lossless amplitude

	// Used to read the channels' content
	///////////////////////////////////////////////////////////
	imbxUint32 bufferPointer = 0;

	// Read until the end of the image is reached
	///////////////////////////////////////////////////////////
	for(m_bEndOfImage=false; !m_bEndOfImage; pSourceStream->resetInBitsBuffer())
	{
		imbxUint32 nextMcuStop = m_mcuNumberTotal;
		if(m_mcuPerRestartInterval != 0)
		{
			nextMcuStop = m_mcuLastRestart + m_mcuPerRestartInterval;
			if(nextMcuStop > m_mcuNumberTotal)
			{
				nextMcuStop = m_mcuNumberTotal;
			}
		}

		if(nextMcuStop <= m_mcuProcessed)
		{
			// Look for a tag. Skip all the FF bytes
			imbxUint8 tagId(0xff);

			try
			{
				pSourceStream->read(&tagId, 1);
				if(tagId != 0xff)
				{
					continue;
				}

				while(tagId == 0xff)
				{
					pSourceStream->read(&tagId, 1);
				}


				// An entry has been found. Process it
				///////////////////////////////////////////////////////////
				ptr<jpeg::tag> pTag;
				if(m_tagsMap.find(tagId)!=m_tagsMap.end())
					pTag=m_tagsMap[tagId];
				else
					pTag=m_tagsMap[0xff];

				pTag->readTag(pSourceStream, this, tagId);
			}
			catch(const streamExceptionEOF& e)
			{
				if(m_mcuProcessed == m_mcuNumberTotal)
				{
					m_bEndOfImage = true;
				}
				else
				{
					throw;
				}
			}
			continue;

		}

		jpeg::jpegChannel* pChannel; // Used in the loops
		while(m_mcuProcessed < nextMcuStop && !pSourceStream->endReached())
		{
			// Read an MCU
			///////////////////////////////////////////////////////////

			// Scan all components
			///////////////////////////////////////////////////////////
			for(jpeg::jpegChannel** channelsIterator = m_channelsList; *channelsIterator != nullptr; ++channelsIterator)
			{
				pChannel = *channelsIterator;

				// Read a lossless pixel
				///////////////////////////////////////////////////////////
				if(m_bLossless)
				{
					for(
						scanBlock = 0;
						scanBlock != pChannel->m_blockMcuXY;
						++scanBlock)
					{
												amplitudeLength = pChannel->m_pActiveHuffmanTableDC->readHuffmanCode(pSourceStream);
												if(amplitudeLength)
						{
							amplitude = pSourceStream->readBits(amplitudeLength);
							if(amplitude < ((imbxInt32)1<<(amplitudeLength-1)))
							{
								amplitude -= ((imbxInt32)1<<amplitudeLength)-1;
							}
						}
						else
						{
							amplitude = 0;
						}

						pChannel->addUnprocessedAmplitude(amplitude, m_spectralIndexStart, m_mcuLastRestart == m_mcuProcessed && scanBlock == 0);
					}

					continue;
				}

				// Read a lossy MCU
				///////////////////////////////////////////////////////////
				bufferPointer = (m_mcuProcessedY * pChannel->m_blockMcuY * ((m_jpegImageSizeX * pChannel->m_samplingFactorX / m_maxSamplingFactorX) >> 3) + m_mcuProcessedX * pChannel->m_blockMcuX) * 64;
				for(scanBlockY = pChannel->m_blockMcuY; (scanBlockY != 0); --scanBlockY)
				{
					for(scanBlockX = pChannel->m_blockMcuX; scanBlockX != 0; --scanBlockX)
					{
						readBlock(pSourceStream, &(pChannel->m_pBuffer[bufferPointer]), pChannel);

						if(m_spectralIndexEnd>=63 && m_bitLow==0)
						{
							IDCT(
								&(pChannel->m_pBuffer[bufferPointer]),
								m_decompressionQuantizationTable[pChannel->m_quantTable]
								);
						}
						bufferPointer += 64;
					}
					bufferPointer += (m_mcuNumberX -1) * pChannel->m_blockMcuX * 64;
				}
			}

			++m_mcuProcessed;
			if(++m_mcuProcessedX == m_mcuNumberX)
			{
				m_mcuProcessedX = 0;
				++m_mcuProcessedY;
			}
		}
	}

	// Process unprocessed lossless amplitudes
	///////////////////////////////////////////////////////////
	for(auto & processLosslessIterator : m_channelsMap)
	{
		processLosslessIterator.second->processUnprocessedAmplitudes();
	}


	// Check for 2's complement
	///////////////////////////////////////////////////////////
	bool b2complement = sourceDataSet->getUnsignedLong(0x0028, 0, 0x0103, 0) != 0;
	std::wstring colorSpace = sourceDataSet->getUnicodeString(0x0028, 0, 0x0004, 0);

	// If the compression is jpeg baseline or jpeg extended
	//  then the color space cannot be "RGB"
	///////////////////////////////////////////////////////////
	if(colorSpace == L"RGB")
	{
		std::wstring transferSyntax(sourceDataSet->getUnicodeString(0x0002, 0, 0x0010, 0));
		if(transferSyntax == L"1.2.840.10008.1.2.4.50" ||  // baseline (8 bits lossy)
		   transferSyntax == L"1.2.840.10008.1.2.4.51")    // extended (12 bits lossy)
		{
			colorSpace = L"YBR_FULL";
		}
	}

	ptr<image> returnImage(new image);
	copyJpegChannelsToImage(returnImage, b2complement, colorSpace);

	return returnImage;

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Copy the loaded image into a class image
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void jpegCodec::copyJpegChannelsToImage(ptr<image> destImage, bool b2complement, std::wstring colorSpace)
{
	PUNTOEXE_FUNCTION_START(L"jpegCodec::copyJpegChannelsToImage");

	image::bitDepth depth;
	if(b2complement)
		depth = (m_precision==8) ? image::depthS8 : image::depthS16;
	else
		depth = (m_precision==8) ? image::depthU8 : image::depthU16;

	ptr<handlers::dataHandlerNumericBase> handler = destImage->create(m_imageSizeX, m_imageSizeY, depth, colorSpace, (imbxUint8)(m_precision-1));

	imbxInt32 offsetValue=(imbxInt32)1<<(m_precision-1);
	imbxInt32 maxClipValue=((imbxInt32)1<<m_precision)-1;
	imbxInt32 minClipValue = 0;
	if(b2complement)
	{
		maxClipValue-=offsetValue;
		minClipValue-=offsetValue;
	}

	if(handler == nullptr)
	{
		return;
	}

	// Copy the jpeg channels into the new image
	///////////////////////////////////////////////////////////
	imbxUint32 destChannelNumber = 0;
	for(auto & copyChannelsIterator : m_channelsMap)
	{
		ptr<jpeg::jpegChannel> pChannel = copyChannelsIterator.second;

		// Adjust 2complement
		///////////////////////////////////////////////////////////
		imbxInt32* pChannelBuffer = pChannel->m_pBuffer;
		if(!m_bLossless && !b2complement)
		{
			for(imbxUint32 adjust2complement = pChannel->m_bufferSize; adjust2complement != 0; --adjust2complement, ++pChannelBuffer)
			{
				*pChannelBuffer += offsetValue;
				if(*pChannelBuffer < minClipValue)
				{
					*pChannelBuffer = minClipValue;
				}
				else if(*pChannelBuffer > maxClipValue)
				{
					*pChannelBuffer = maxClipValue;
				}
			}
		}
		else if(m_bLossless && b2complement)
		{
			for(imbxUint32 adjust2complement = pChannel->m_bufferSize; adjust2complement; --adjust2complement)
			{
				if(*pChannelBuffer & offsetValue)
				{
					*pChannelBuffer |= ((imbxInt32)-1) << m_precision;
				}
				if(*pChannelBuffer < minClipValue)
				{
					*pChannelBuffer = minClipValue;
				}
				else if(*pChannelBuffer > maxClipValue)
				{
					*pChannelBuffer = maxClipValue;
				}
				++pChannelBuffer;
			}
		}

		// If only one channel is present, then use the fast copy
		///////////////////////////////////////////////////////////
		if(m_bLossless && m_channelsMap.size() == 1)
		{
			handler->copyFrom(pChannel->m_pBuffer, pChannel->m_bufferSize);
			return;
		}

		// Lossless interleaved
		///////////////////////////////////////////////////////////
		imbxUint32 runX = m_maxSamplingFactorX / pChannel->m_samplingFactorX;
		imbxUint32 runY = m_maxSamplingFactorY / pChannel->m_samplingFactorY;
		if(m_bLossless)
		{
			handler->copyFromInt32Interleaved(
				pChannel->m_pBuffer,
				runX, runY,
				0, 0, pChannel->m_sizeX * runX, pChannel->m_sizeY * runY,
				destChannelNumber++,
				m_imageSizeX, m_imageSizeY,
				(imbxUint32)m_channelsMap.size());

			continue;
		}

		// Lossy interleaved
		///////////////////////////////////////////////////////////
		imbxUint32 totalBlocksY(pChannel->m_sizeY >> 3);
		imbxUint32 totalBlocksX(pChannel->m_sizeX >> 3);

		imbxInt32* pSourceBuffer(pChannel->m_pBuffer);

		imbxUint32 startRow(0);
		for(imbxUint32 scanBlockY = 0; scanBlockY < totalBlocksY; ++scanBlockY)
		{
			imbxUint32 startCol(0);
			imbxUint32 endRow(startRow + (runY << 3));

			for(imbxUint32 scanBlockX = 0; scanBlockX < totalBlocksX; ++scanBlockX)
			{
				imbxUint32 endCol = startCol + (runX << 3);
				handler->copyFromInt32Interleaved(
							pSourceBuffer,
							runX, runY,
							startCol,
							startRow,
							endCol,
							endRow,
							destChannelNumber,
							m_imageSizeX, m_imageSizeY,
							(imbxUint32)m_channelsMap.size());

				pSourceBuffer += 64;
				startCol = endCol;
			}
			startRow = endRow;
		}
		++destChannelNumber;
	}

	PUNTOEXE_FUNCTION_END();
}


////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
//
//
// Copy an image into the internal channels
//
//
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
void jpegCodec::copyImageToJpegChannels(
	ptr<image> sourceImage,
	bool b2complement,
	imbxUint8 allocatedBits,
	bool bSubSampledX,
	bool bSubSampledY)
{
	PUNTOEXE_FUNCTION_START(L"jpegCodec::copyImageToJpegChannels");

	std::wstring colorSpace = sourceImage->getColorSpace();
	sourceImage->getSize(&m_imageSizeX, &m_imageSizeY);
	m_precision = allocatedBits;

	// Create the channels
	////////////////////////////////////////////////////////////////
	imbxUint32 rowSize, channelSize, channelsNumber;
	ptr<handlers::dataHandlerNumericBase>imageDataHandler = sourceImage->getDataHandler(false, &rowSize, &channelSize, &channelsNumber);

	for(imbxUint8 channelId = 0; channelId < (imbxUint8)channelsNumber; ++channelId)
	{
		ptr<jpeg::jpegChannel> pChannel(new jpeg::jpegChannel);
		m_channelsMap[channelId] = pChannel;

		pChannel->m_huffmanTableAC = 0;
		pChannel->m_pActiveHuffmanTableAC = m_pHuffmanTableAC[0].get();
		pChannel->m_huffmanTableDC = 0;
		pChannel->m_pActiveHuffmanTableDC = m_pHuffmanTableDC[0].get();

		if(channelId == 0)
		{
			if(bSubSampledX)
			{
				++(pChannel->m_samplingFactorX);
			}
			if(bSubSampledY)
			{
				++(pChannel->m_samplingFactorY);
			}
			continue;
		}
		if(colorSpace != L"YBR_FULL" && colorSpace != L"YBR_PARTIAL")
		{
			continue;
		}
		pChannel->m_quantTable = 1;
		pChannel->m_huffmanTableDC = 1;
		pChannel->m_huffmanTableAC = 1;
		pChannel->m_pActiveHuffmanTableAC = m_pHuffmanTableAC[1].get();
		pChannel->m_pActiveHuffmanTableDC = m_pHuffmanTableDC[1].get();
	}
	allocChannels();

	imbxInt32 offsetValue=(imbxInt32)1<<(m_precision-1);
	imbxInt32 maxClipValue=((imbxInt32)1<<m_precision)-1;
	imbxInt32 minClipValue = 0;
	if(b2complement)
	{
		maxClipValue-=offsetValue;
		minClipValue-=offsetValue;
	}

	// Copy the image into the jpeg channels
	///////////////////////////////////////////////////////////
	imbxUint32 sourceChannelNumber = 0;
	for(auto & copyChannelsIterator : m_channelsMap)
	{
		ptr<jpeg::jpegChannel> pChannel = copyChannelsIterator.second;

		// If only one channel is present, then use the fast copy
		///////////////////////////////////////////////////////////
		if(m_bLossless && m_channelsMap.size() == 1)
		{
			imageDataHandler->copyTo(pChannel->m_pBuffer, pChannel->m_bufferSize);
			continue;
		}

		// Lossless interleaved
		///////////////////////////////////////////////////////////
		imbxUint32 runX = m_maxSamplingFactorX / pChannel->m_samplingFactorX;
		imbxUint32 runY = m_maxSamplingFactorY / pChannel->m_samplingFactorY;
		if(m_bLossless)
		{
			imageDataHandler->copyToInt32Interleaved(
				pChannel->m_pBuffer,
				runX, runY,
				0, 0, pChannel->m_sizeX * runX, pChannel->m_sizeY * runY,
				sourceChannelNumber++,
				m_imageSizeX, m_imageSizeY,
				(imbxUint32)m_channelsMap.size());

			continue;
		}

		// Lossy interleaved
		///////////////////////////////////////////////////////////
		imbxUint32 totalBlocksY = (pChannel->m_sizeY >> 3);
		imbxUint32 totalBlocksX = (pChannel->m_sizeX >> 3);

		imbxInt32* pDestBuffer = pChannel->m_pBuffer;

		imbxUint32 startRow = 0;
		for(imbxUint32 scanBlockY = 0; scanBlockY < totalBlocksY; ++scanBlockY)
		{
			imbxUint32 startCol = 0;
			imbxUint32 endRow = startRow + (runY << 3);

			for(imbxUint32 scanBlockX = 0; scanBlockX < totalBlocksX; ++scanBlockX)
			{
				imbxUint32 endCol = startCol + (runX << 3);
				imageDataHandler->copyToInt32Interleaved(
					pDestBuffer,
					runX, runY,
					startCol,
					startRow,
					endCol,
					endRow,
					sourceChannelNumber,
					m_imageSizeX, m_imageSizeY,
					(imbxUint32)m_channelsMap.size());

				pDestBuffer += 64;
				startCol = endCol;
			}
			startRow = endRow;
		}
		++sourceChannelNumber;
	}


	for(auto & clipChannelsIterator : m_channelsMap)
	{
		ptr<jpeg::jpegChannel> pChannel = clipChannelsIterator.second;

		// Clip the values
		///////////////////////////////////////////////////////////
		imbxInt32* pChannelBuffer = pChannel->m_pBuffer;
		for(imbxUint32 clipValues = pChannel->m_bufferSize; clipValues; --clipValues)
		{
			if(*pChannelBuffer < minClipValue)
			{
				*pChannelBuffer = minClipValue;
			}
			if(*pChannelBuffer > maxClipValue)
			{
				*pChannelBuffer = maxClipValue;
			}
			++pChannelBuffer;
		}

		// Adjust 2complement
		///////////////////////////////////////////////////////////
		if(!m_bLossless && !b2complement)
		{
			pChannelBuffer = pChannel->m_pBuffer;
			for(imbxUint32 adjust2complement = pChannel->m_bufferSize; adjust2complement; --adjust2complement)
			{
				*(pChannelBuffer++) -= offsetValue;
			}
		}

		pChannelBuffer = pChannel->m_pBuffer;
		imbxInt32 orValue   = ((imbxInt32) - 1) << m_precision;
		for(imbxUint32 adjustHighBits = pChannel->m_bufferSize; adjustHighBits != 0; --adjustHighBits)
		{
			if((*pChannelBuffer & offsetValue) != 0)
			{
				*pChannelBuffer |= orValue;
			}
			++pChannelBuffer;
		}
	}

	PUNTOEXE_FUNCTION_END();
}


/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
//
//
// Write an image into the dataset
//
//
/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
void jpegCodec::setImage(
		ptr<streamWriter> pDestStream,
		ptr<image> pImage,
		std::wstring transferSyntax,
		quality imageQuality,
				std::string /* dataType */,
		imbxUint8 allocatedBits,
		bool bSubSampledX,
		bool bSubSampledY,
		bool bInterleaved,
		bool b2Complement)
{
	PUNTOEXE_FUNCTION_START(L"jpegCodec::setImage");

	streamWriter* pDestinationStream = pDestStream.get();

	// Activate the tags in the stream
	///////////////////////////////////////////////////////////
	pDestinationStream->m_bJpegTags = true;

	// Reset the internal variables
	////////////////////////////////////////////////////////////////
	resetInternal(true, imageQuality);

	m_bLossless = transferSyntax == L"1.2.840.10008.1.2.4.57" ||  // lossless NH
				  transferSyntax == L"1.2.840.10008.1.2.4.70";    // lossless NH first order prediction

	copyImageToJpegChannels(pImage, b2Complement, allocatedBits, bSubSampledX, bSubSampledY);

	// Now write the jpeg stream
	////////////////////////////////////////////////////////////////
	static imbxUint8 checkSignature[2]={(imbxUint8)0xff, (imbxUint8)0xd8};
	pDestinationStream->write(checkSignature, 2);

	// Write the SOF tag
	////////////////////////////////////////////////////////////////
	writeTag(pDestinationStream, m_bLossless ? sof3 : (m_precision <= 8 ? sof0 : sof1));

	// Write the quantization tables
	////////////////////////////////////////////////////////////////
	writeTag(pDestinationStream, dqt);

	for(int phase = 0; phase < 2; ++phase)
	{
		if(phase == 1)
		{
			// Write the huffman tables
			////////////////////////////////////////////////////////////////
			writeTag(pDestinationStream, dht);
		}

		// Write the scans
		////////////////////////////////////////////////////////////////
		memset(m_channelsList, 0, sizeof(m_channelsList));
		if(bInterleaved)
		{
			size_t scanChannels(0);
			for(auto & channelsIterator : m_channelsMap)
			{
				m_channelsList[scanChannels++] = channelsIterator.second.get();
			}
			writeScan(pDestinationStream, phase == 0);
		}
		else
		{
			for(auto & channelsIterator : m_channelsMap)
			{
				m_channelsList[0] = channelsIterator.second.get();
				writeScan(pDestinationStream, phase == 0);
			}
		}
	}

	writeTag(pDestinationStream, eoi);

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Write a single scan (SOS tag + channels)
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void jpegCodec::writeScan(streamWriter* pDestinationStream, bool bCalcHuffman)
{
	PUNTOEXE_FUNCTION_START(L"jpegCodec::writeScan");

	findMcuSize();

	if(!bCalcHuffman)
	{
		if(m_bLossless)
		{
			m_spectralIndexStart = 1;
		}
		writeTag(pDestinationStream, sos);
	}

	jpeg::jpegChannel* pChannel; // Used in the loops
	while(m_mcuProcessed < m_mcuNumberTotal)
	{
		// Write an MCU
		///////////////////////////////////////////////////////////

		// Scan all components
		///////////////////////////////////////////////////////////
		for(jpeg::jpegChannel** channelsIterator = m_channelsList; *channelsIterator != nullptr; ++channelsIterator)
		{
			pChannel = *channelsIterator;

			// Write a lossless pixel
			///////////////////////////////////////////////////////////
			if(m_bLossless)
			{
				imbxInt32 lastValue = pChannel->m_lastDCValue;
				imbxInt32* pBuffer = pChannel->m_pBuffer + pChannel->m_losslessPositionY * pChannel->m_sizeX + pChannel->m_losslessPositionX;

				for(int scanBlock = pChannel->m_blockMcuXY; scanBlock != 0; --scanBlock)
				{
					imbxInt32 value(*pBuffer);
					if(pChannel->m_losslessPositionX == 0 && pChannel->m_losslessPositionY != 0)
					{
						lastValue = *(pBuffer - pChannel->m_sizeX);
					}
					++pBuffer;
					imbxInt32 diff = value - lastValue;
					imbxInt32 diff1 = value + ((imbxInt32)1 << m_precision) - lastValue;
					imbxInt32 diff2 = value - ((imbxInt32)1 << m_precision) - lastValue;
					if(labs(diff1) < labs(diff))
					{
						diff = diff1;
					}
					if(labs(diff2) < labs(diff))
					{
						diff = diff2;
					}

					// Calculate amplitude and build the huffman table
					imbxUint32 amplitudeLength = 0;
					imbxUint32 amplitude = 0;
					if(diff != 0)
					{
						amplitude = diff > 0 ? diff : -diff;
						for(amplitudeLength = 32; (amplitude & ((imbxUint32)1 << (amplitudeLength -1))) == 0; --amplitudeLength){};

						if(diff < 0)
						{
							amplitude = ((imbxUint32)1 << amplitudeLength) + diff - 1;
						}

					}

					pChannel->m_lastDCValue = value;
					if(++(pChannel->m_losslessPositionX) == pChannel->m_sizeX)
					{
						++(pChannel->m_losslessPositionY);
						pChannel->m_losslessPositionX = 0;
					}

					if(bCalcHuffman)
					{
						pChannel->m_pActiveHuffmanTableDC->incValueFreq(amplitudeLength);
						continue;
					}
					pChannel->m_pActiveHuffmanTableDC->writeHuffmanCode(amplitudeLength, pDestinationStream);
					pDestinationStream->writeBits(amplitude, amplitudeLength);
				}

				continue;
			}

			// write a lossy MCU
			///////////////////////////////////////////////////////////
			imbxUint32 bufferPointer = (m_mcuProcessedY * pChannel->m_blockMcuY * ((m_jpegImageSizeX * pChannel->m_samplingFactorX / m_maxSamplingFactorX) >> 3) + m_mcuProcessedX * pChannel->m_blockMcuX) * 64;

			for(int scanBlockY = 0; scanBlockY < pChannel->m_blockMcuY; ++scanBlockY)
			{
				for(int scanBlockX = 0; scanBlockX < pChannel->m_blockMcuX; ++scanBlockX)
				{
					writeBlock(pDestinationStream, &(pChannel->m_pBuffer[bufferPointer]), pChannel, bCalcHuffman);
					bufferPointer += 64;
				}
				bufferPointer += (m_mcuNumberX -1) * pChannel->m_blockMcuX * 64;
			}
		}

		++m_mcuProcessed;
		if(++m_mcuProcessedX == m_mcuNumberX)
		{
			m_mcuProcessedX = 0;
			++m_mcuProcessedY;
		}
	}

	if(!bCalcHuffman)
	{
		pDestinationStream->resetOutBitsBuffer();
	}

	PUNTOEXE_FUNCTION_END();
}


/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
//
//
// Write a single jpeg tag
//
//
/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
void jpegCodec::writeTag(streamWriter* pDestinationStream, tTagId tagId)
{
	PUNTOEXE_FUNCTION_START(L"jpegCodec::writeTag");

	ptr<jpeg::tag> pTag;
	tTagsMap::iterator findTag = m_tagsMap.find((imbxUint8)tagId);
	if(findTag == m_tagsMap.end())
	{
		return;
	}
	static imbxUint8 ff(0xff);
	imbxUint8 byteTagId(tagId);
	pDestinationStream->write(&ff, 1);
	pDestinationStream->write(&byteTagId, 1);
	findTag->second->writeTag(pDestinationStream, this);

	PUNTOEXE_FUNCTION_END();
}


/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
//
//
// Read a single MCU's block.
//
//
/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
inline void jpegCodec::readBlock(streamReader* pStream, imbxInt32* pBuffer, jpeg::jpegChannel* pChannel)
{
	PUNTOEXE_FUNCTION_START(L"jpegCodec::readBlock");

	// Scan all the requested spectral values
	/////////////////////////////////////////////////////////////////
	imbxUint32 spectralIndex(m_spectralIndexStart);

	// If an eob-run is defined, then don't read the DC value
	/////////////////////////////////////////////////////////////////
	if(m_eobRun && spectralIndex == 0)
	{
		++spectralIndex;
	}

	imbxUint32 amplitude;
	imbxUint32 hufCode;
	imbxInt32 value = 0;
	imbxInt32 oldValue;
	imbxUint8 amplitudeLength;
	int runLength;
	imbxUint32 tempEobRun;
	imbxInt32 positiveBitLow((imbxInt32)1 << m_bitLow);
	imbxInt32 negativeBitLow((imbxInt32)-1 << m_bitLow);

	// Scan the specified spectral values
	/////////////////////////////////////////////////////////////////
	for(; spectralIndex <= m_spectralIndexEnd; ++spectralIndex)
	{
		// Read AC progressive bits for non-zero coefficients
		/////////////////////////////////////////////////////////////////
		if(m_eobRun != 0)
		{
			if(m_bitHigh == 0)
			{
				break;
			}
			oldValue = pBuffer[JpegDeZigZagOrder[spectralIndex]];
			if(oldValue == 0)
			{
				continue;
			}

			amplitude = pStream->readBit();

			if(amplitude != 0 && (oldValue & positiveBitLow)==0)
			{
				oldValue += (oldValue>0 ? positiveBitLow : negativeBitLow);
				pBuffer[JpegDeZigZagOrder[spectralIndex]] = oldValue;
			}
			continue;
		}

		//
		// AC/DC pass
		//
		/////////////////////////////////////////////////////////////////
		if(spectralIndex != 0)
		{
			hufCode = pChannel->m_pActiveHuffmanTableAC->readHuffmanCode(pStream);

			// End of block reached
			/////////////////////////////////////////////////////////////////
			if(hufCode == 0)
			{
				++m_eobRun;
				--spectralIndex;
				continue;
			}
		}
		else
		{
			// First pass
			/////////////////////////////////////////////////////////////////
			if(m_bitHigh)
			{
				hufCode = pStream->readBit();
				value=(int)hufCode;
				hufCode = 0;
			}
			else
			{
				hufCode = pChannel->m_pActiveHuffmanTableDC->readHuffmanCode(pStream);
			}
		}


		//
		// Get AC or DC amplitude or zero run
		//
		/////////////////////////////////////////////////////////////////

		// Find bit coded coeff. amplitude
		/////////////////////////////////////////////////////////////////
		amplitudeLength=(imbxUint8)(hufCode & 0xf);

		// Find zero run length
		/////////////////////////////////////////////////////////////////
		runLength=(int)(hufCode>>4);

		// First DC or AC pass or refine AC pass but not EOB run
		/////////////////////////////////////////////////////////////////
		if(spectralIndex == 0 || amplitudeLength != 0 || runLength == 0xf)
		{
			//
			// First DC pass and all the AC passes are similar,
			//  then use the same algorithm
			//
			/////////////////////////////////////////////////////////////////
			if(m_bitHigh == 0 || spectralIndex != 0)
			{
				// Read coeff
				/////////////////////////////////////////////////////////////////
				if(amplitudeLength != 0)
				{
					value = (imbxInt32)(pStream->readBits(amplitudeLength));
					if(value < ((imbxInt32)1 << (amplitudeLength-1)) )
					{
						value -= ((imbxInt32)1 << amplitudeLength) - 1;
					}
				}
				else
				{
					value = 0;
				}

				// Move spectral index forward by zero run length
				/////////////////////////////////////////////////////////////////
				if(m_bitHigh && spectralIndex)
				{
					// Read the correction bits
					/////////////////////////////////////////////////////////////////
					for(/* none */; spectralIndex <= m_spectralIndexEnd; ++spectralIndex)
					{
						oldValue = pBuffer[JpegDeZigZagOrder[spectralIndex]];
						if(oldValue != 0)
						{
							amplitude = pStream->readBit();
							if(amplitude != 0 && (oldValue & positiveBitLow) == 0)
							{
								oldValue += (oldValue>0 ? positiveBitLow : negativeBitLow);
								pBuffer[JpegDeZigZagOrder[spectralIndex]] = oldValue;
							}
							continue;
						}
						if(runLength == 0)
						{
							break;
						}
						--runLength;
					}
				}
				else
				{
					spectralIndex += runLength;
					runLength = 0;
				}
			}

			// Store coeff.
			/////////////////////////////////////////////////////////////////
			if(spectralIndex<=m_spectralIndexEnd)
			{
				oldValue = value<<m_bitLow;
				if(m_bitHigh)
					oldValue |= pBuffer[JpegDeZigZagOrder[spectralIndex]];

				// DC coeff added to the previous value.
				/////////////////////////////////////////////////////////////////
				if(spectralIndex == 0 && m_bitHigh == 0)
				{
					oldValue += pChannel->m_lastDCValue;
					pChannel->m_lastDCValue=oldValue;
				}
				pBuffer[JpegDeZigZagOrder[spectralIndex]]=oldValue;
			}
		} // ----- End of first DC or AC pass or refine AC pass but not EOB run

		// EOB run found
		/////////////////////////////////////////////////////////////////
		else
		{
			tempEobRun = pStream->readBits(runLength);
			m_eobRun+=(imbxUint32)1<<runLength;
			m_eobRun+=tempEobRun;
			--spectralIndex;
		}
	}

	//
	// EOB run processor
	//
	/////////////////////////////////////////////////////////////////
	if(m_eobRun != 0)
		m_eobRun--;

	PUNTOEXE_FUNCTION_END();
}


/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
//
//
// Write a single MCU's block.
//
//
/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
inline void jpegCodec::writeBlock(streamWriter* pStream, imbxInt32* pBuffer, jpeg::jpegChannel* pChannel, bool bCalcHuffman)
{
	PUNTOEXE_FUNCTION_START(L"jpegCodec::writeBlock");

	if(bCalcHuffman)
	{
		FDCT(pBuffer, m_compressionQuantizationTable[pChannel->m_quantTable]);
	}

	// Scan the specified spectral values
	/////////////////////////////////////////////////////////////////
	imbxUint8 zeroRun = 0;
		imbxInt32 value;
		const imbxUint32* pJpegDeZigZagOrder(&(JpegDeZigZagOrder[m_spectralIndexStart]));
		huffmanTable* pActiveHuffmanTable;

	for(imbxUint32 spectralIndex=m_spectralIndexStart; spectralIndex <= m_spectralIndexEnd; ++spectralIndex)
	{
		value = pBuffer[*(pJpegDeZigZagOrder++)];

		if(value > 32767)
		{
			value = 32767;
		}
				else if(value < -32767)
		{
			value = -32767;
		}
		if(spectralIndex == 0)
		{
			value -= pChannel->m_lastDCValue;
			pChannel->m_lastDCValue += value;
			pActiveHuffmanTable = pChannel->m_pActiveHuffmanTableDC;
		}
		else
		{
			pActiveHuffmanTable = pChannel->m_pActiveHuffmanTableAC;
			if(value == 0)
			{
				++zeroRun;
				continue;
			}
		}

		//Write out the zero runs
		/////////////////////////////////////////////////////////////////
		while(zeroRun >= 16)
		{
			zeroRun -= 16;
			static imbxUint32 zeroRunCode = 0xf0;
			if(bCalcHuffman)
			{
				pActiveHuffmanTable->incValueFreq(zeroRunCode);
				continue;
			}
			pActiveHuffmanTable->writeHuffmanCode(zeroRunCode, pStream);
		}

		imbxUint32 hufCode = (zeroRun << 4);
		zeroRun = 0;

		// Write out the value
		/////////////////////////////////////////////////////////////////
		imbxUint8 amplitudeLength = 0;
		imbxUint32 amplitude = 0;
		if(value != 0)
		{
			amplitude = (value > 0) ? value : -value;
			for(amplitudeLength = 15; (amplitude & ((imbxUint32)1 << (amplitudeLength -1))) == 0; --amplitudeLength){};

			if(value < 0)
			{
				amplitude = ((imbxUint32)1 << amplitudeLength) + value -1;
			}
			hufCode |= amplitudeLength;
		}

		if(bCalcHuffman)
		{
			pActiveHuffmanTable->incValueFreq(hufCode);
			continue;
		}
		pActiveHuffmanTable->writeHuffmanCode(hufCode, pStream);
		if(amplitudeLength != 0)
		{
			pStream->writeBits(amplitude, amplitudeLength);
		}
	}

	if(zeroRun == 0)
	{
		return;
	}

	static imbxUint32 zero = 0;
	if(bCalcHuffman)
	{
		pChannel->m_pActiveHuffmanTableAC->incValueFreq(zero);
		return;
	}
	pChannel->m_pActiveHuffmanTableAC->writeHuffmanCode(zero, pStream);

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Calculate the quantization tables with the correction
//  factor needed by the IDCT/FDCT
//
//
///////////////////////////////////////////////////////////;
///////////////////////////////////////////////////////////
void jpegCodec::recalculateQuantizationTables(int table)
{
	// Adjust the tables for compression/decompression
	///////////////////////////////////////////////////////////
	imbxUint8 tableIndex = 0;
	for(float row : JpegDctScaleFactor)
	{
		for(float col : JpegDctScaleFactor)
		{
			m_decompressionQuantizationTable[table][tableIndex]=(long long)((float)((m_quantizationTable[table][tableIndex])<<JPEG_DECOMPRESSION_BITS_PRECISION)*col*row);
			m_compressionQuantizationTable[table][tableIndex]=1.0f/((float)((m_quantizationTable[table][tableIndex])<<3)*col*row);
			++tableIndex;
		}
	}
}


/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
//
//
// Calc FDCT on MCU
// This routine comes from the IJG software version 6b
//
// Values must be Zero centered (-x...0...+x)
//
//
/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
void jpegCodec::FDCT(imbxInt32* pIOMatrix, float* pDescaleFactors)
{
	// Temporary values
	/////////////////////////////////////////////////////////////////
	float tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7;
	float tmp10, tmp11, tmp12, tmp13;
	float z1, z2, z3, z4, z5, z11, z13;

	// Rows FDCT
	/////////////////////////////////////////////////////////////////
	imbxInt32 *pointerOperator0, *pointerOperator1;
	for(int scanBlockY=0; scanBlockY<64; scanBlockY+=8)
	{
		pointerOperator0 = &(pIOMatrix[scanBlockY]);
		pointerOperator1 = &(pIOMatrix[scanBlockY + 7]);
		tmp0 = (float)(*pointerOperator0 + *pointerOperator1);
		tmp7 = (float)(*pointerOperator0 - *pointerOperator1);

		tmp1 = (float)(*(++pointerOperator0) + *(--pointerOperator1));
		tmp6 = (float)(*pointerOperator0 - *pointerOperator1);

		tmp2 = (float)(*(++pointerOperator0) + *(--pointerOperator1));
		tmp5 = (float)(*pointerOperator0 - *pointerOperator1);

		tmp3 = (float)(*(++pointerOperator0) + *(--pointerOperator1));
		tmp4 = (float)(*pointerOperator0 - *pointerOperator1);

		// Phase 2
		tmp10 = tmp0 + tmp3;
		tmp13 = tmp0 - tmp3;
		tmp11 = tmp1 + tmp2;
		tmp12 = tmp1 - tmp2;

		// Phase 3
		m_fdctTempMatrix[scanBlockY]   = tmp10 + tmp11;
		m_fdctTempMatrix[scanBlockY+4] = tmp10 - tmp11;

		z1 = (tmp12 + tmp13)*0.707106781f;     // c4

		// Phase 5
		m_fdctTempMatrix[scanBlockY+2] = tmp13 + z1;
		m_fdctTempMatrix[scanBlockY+6] = tmp13 - z1;

		// Odd part
		// Phase 2
		tmp10 = tmp4 + tmp5;
		tmp11 = tmp5 + tmp6;
		tmp12 = tmp6 + tmp7;

		// The rotator is modified from fig 4-8 to avoid extra negations.
		z5 =  (tmp10 - tmp12)*0.382683433f;    // c6
		z2 =  tmp10*0.541196100f + z5;         // c2-c6
		z4 =  tmp12*1.306562965f + z5;         // c2+c6
		z3 =  tmp11*0.707106781f;              // c4

		// Phase 5
		z11 = tmp7 + z3;
		z13 = tmp7 - z3;

		// Phase 6
		m_fdctTempMatrix[scanBlockY+5] = z13 + z2;
		m_fdctTempMatrix[scanBlockY+3] = z13 - z2;
		m_fdctTempMatrix[scanBlockY+1] = z11 + z4;
		m_fdctTempMatrix[scanBlockY+7] = z11 - z4;
	}

	// Columns FDCT
	/////////////////////////////////////////////////////////////////
	float *pointerOperatorFloat0, *pointerOperatorFloat1;
	for(int scanBlockX = 0; scanBlockX < 8; ++scanBlockX)
	{
		pointerOperatorFloat0 = &(m_fdctTempMatrix[scanBlockX]);
		pointerOperatorFloat1 = &(m_fdctTempMatrix[scanBlockX + 56]);

		tmp0 = *pointerOperatorFloat0 + *pointerOperatorFloat1;
		tmp7 = *pointerOperatorFloat0 - *pointerOperatorFloat1;

		pointerOperatorFloat0 += 8;
		pointerOperatorFloat1 -= 8;
		tmp1 = *pointerOperatorFloat0 + *pointerOperatorFloat1;
		tmp6 = *pointerOperatorFloat0 - *pointerOperatorFloat1;

		pointerOperatorFloat0 += 8;
		pointerOperatorFloat1 -= 8;
		tmp2 = *pointerOperatorFloat0 + *pointerOperatorFloat1;
		tmp5 = *pointerOperatorFloat0 - *pointerOperatorFloat1;

		pointerOperatorFloat0 += 8;
		pointerOperatorFloat1 -= 8;
		tmp3 = *pointerOperatorFloat0 + *pointerOperatorFloat1;
		tmp4 = *pointerOperatorFloat0 - *pointerOperatorFloat1;

		// Even part
		// Phase 2
		tmp10 = tmp0 + tmp3;
		tmp13 = tmp0 - tmp3;
		tmp11 = tmp1 + tmp2;
		tmp12 = tmp1 - tmp2;

		// Phase 3
		m_fdctTempMatrix[scanBlockX   ] = tmp10 + tmp11;
		m_fdctTempMatrix[scanBlockX+32] = tmp10 - tmp11;

		z1 = (tmp12 + tmp13)*0.707106781f;     // c4

		// Phase 5
		m_fdctTempMatrix[scanBlockX+16] = (tmp13 + z1);
		m_fdctTempMatrix[scanBlockX+48] = (tmp13 - z1);

		// Odd part
		// Phase 2
		tmp10 = tmp4 + tmp5;
		tmp11 = tmp5 + tmp6;
		tmp12 = tmp6 + tmp7;

		// Avoid extra negations.
		z5 =  (tmp10 - tmp12)*0.382683433f;    // c6
		z2 =  tmp10*0.541196100f + z5;         // c2-c6
		z4 =  tmp12*1.306562965f + z5;         // c2+c6
		z3 =  tmp11*0.707106781f;              // c4

		// Phase 5
		z11 = tmp7 + z3;
		z13 = tmp7 - z3;

		// Phase 6
		m_fdctTempMatrix[scanBlockX+40] = (z13 + z2);
		m_fdctTempMatrix[scanBlockX+24] = (z13 - z2);
		m_fdctTempMatrix[scanBlockX+ 8] = (z11 + z4);
		m_fdctTempMatrix[scanBlockX+56] = (z11 - z4);
	}

	// Descale FDCT results
	/////////////////////////////////////////////////////////////////
	for(int descale = 0; descale < 64; ++descale)
		pIOMatrix[descale]=(imbxInt32)(m_fdctTempMatrix[descale]*pDescaleFactors[descale]+.5f);

}


/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
//
//
// Calc IDCT on MCU
// This routine comes from the IJG software version 6b
//
// Values must be Zero centered (-x...0...+x)
//
//
/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
void jpegCodec::IDCT(imbxInt32* pIOMatrix, long long* pScaleFactors)
{
	static const double multiplier((float)((long long)1 << JPEG_DECOMPRESSION_BITS_PRECISION));
	static const long long multiplier_1_414213562f((long long)(multiplier * 1.414213562f + .5f));
	static const long long multiplier_1_847759065f((long long)(multiplier * 1.847759065f + .5f));
	static const long long multiplier_1_0823922f((long long)(multiplier * 1.0823922f + .5f));
	static const long long multiplier_2_61312593f((long long)(multiplier * 2.61312593f + .5f));
	static const long long zero_point_five((long long)1 << (JPEG_DECOMPRESSION_BITS_PRECISION - 1));
	static const long long zero_point_five_by_8((imbxInt32)zero_point_five << 3);


	// Temporary values
	/////////////////////////////////////////////////////////////////
	long long tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7;
	long long tmp10, tmp11, tmp12, tmp13;
	long long z5, z10, z11, z12, z13;

	//
	// Rows IDCT
	//
	/////////////////////////////////////////////////////////////////
	imbxInt32* pMatrix(pIOMatrix);
	imbxInt32* pCheckMatrix(pIOMatrix);

	imbxInt32 checkZero;
	long long* pTempMatrix(m_idctTempMatrix);

	for(int scanBlockY(8); scanBlockY != 0; --scanBlockY)
	{
		checkZero = *(++pCheckMatrix);
		checkZero |= *(++pCheckMatrix);
		checkZero |= *(++pCheckMatrix);
		checkZero |= *(++pCheckMatrix);
		checkZero |= *(++pCheckMatrix);
		checkZero |= *(++pCheckMatrix);
		checkZero |= *(++pCheckMatrix);
		++pCheckMatrix; // Point pCheckMatrix to the next row

		// Check for AC coefficients value.
		// If they are all NULL, then apply the DC value to all
		/////////////////////////////////////////////////////////////////
		if(checkZero == 0)
		{
			tmp0 = (long long)(*pMatrix) * (*pScaleFactors);
			*(pTempMatrix++) = tmp0;
			*(pTempMatrix++) = tmp0;
			*(pTempMatrix++) = tmp0;
			*(pTempMatrix++) = tmp0;
			*(pTempMatrix++) = tmp0;
			*(pTempMatrix++) = tmp0;
			*(pTempMatrix++) = tmp0;
			*(pTempMatrix++) = tmp0;
			pMatrix = pCheckMatrix;
			pScaleFactors += 8;
			continue;
		}

		tmp0 = (long long)*pMatrix++ * (*pScaleFactors++);
		tmp4 = (long long)*pMatrix++ * (*pScaleFactors++);
		tmp1 = (long long)*pMatrix++ * (*pScaleFactors++);
		tmp5 = (long long)*pMatrix++ * (*pScaleFactors++);
		tmp2 = (long long)*pMatrix++ * (*pScaleFactors++);
		tmp6 = (long long)*pMatrix++ * (*pScaleFactors++);
		tmp3 = (long long)*pMatrix++ * (*pScaleFactors++);
		tmp7 = (long long)*pMatrix++ * (*pScaleFactors++);

		// Phase 3
		tmp10 = tmp0 + tmp2;
		tmp11 = tmp0 - tmp2;

		// Phases 5-3
		tmp13 = tmp1 + tmp3;
		tmp12 = (((tmp1 - tmp3) * multiplier_1_414213562f + zero_point_five) >> JPEG_DECOMPRESSION_BITS_PRECISION) - tmp13; // 2*c4

		// Phase 2
		tmp0 = tmp10 + tmp13;
		tmp3 = tmp10 - tmp13;
		tmp1 = tmp11 + tmp12;
		tmp2 = tmp11 - tmp12;

		// Phase 6
		z13 = tmp6 + tmp5;
		z10 = tmp6 - tmp5;
		z11 = tmp4 + tmp7;
		z12 = tmp4 - tmp7;

		// Phase 5
		tmp7 = z11 + z13;
		z5 = ((z10 + z12) * multiplier_1_847759065f + zero_point_five) >> JPEG_DECOMPRESSION_BITS_PRECISION;    // 2*c2

		// Phase 2
		tmp6 = z5 - ((z10 *multiplier_2_61312593f + zero_point_five) >> JPEG_DECOMPRESSION_BITS_PRECISION) - tmp7;
		tmp5 = (((z11 - z13) * multiplier_1_414213562f + zero_point_five) >> JPEG_DECOMPRESSION_BITS_PRECISION) - tmp6;
		tmp4 = ((z12 * multiplier_1_0823922f + zero_point_five) >> JPEG_DECOMPRESSION_BITS_PRECISION) - z5 + tmp5;

		*(pTempMatrix++) = tmp0 + tmp7;
		*(pTempMatrix++) = tmp1 + tmp6;
		*(pTempMatrix++) = tmp2 + tmp5;
		*(pTempMatrix++) = tmp3 - tmp4;
		*(pTempMatrix++) = tmp3 + tmp4;
		*(pTempMatrix++) = tmp2 - tmp5;
		*(pTempMatrix++) = tmp1 - tmp6;
		*(pTempMatrix++) = tmp0 - tmp7;
	}

	//
	// Columns IDCT
	//
	/////////////////////////////////////////////////////////////////
	pMatrix = pIOMatrix;
	pTempMatrix = m_idctTempMatrix;
	for(int scanBlockX(8); scanBlockX != 0; --scanBlockX)
	{
		tmp0 = *pTempMatrix;
		pTempMatrix += 8;
		tmp4 = *pTempMatrix;
		pTempMatrix += 8;
		tmp1 = *pTempMatrix;
		pTempMatrix += 8;
		tmp5 = *pTempMatrix;
		pTempMatrix += 8;
		tmp2 = *pTempMatrix;
		pTempMatrix += 8;
		tmp6 = *pTempMatrix;
		pTempMatrix += 8;
		tmp3 = *pTempMatrix;
		pTempMatrix += 8;
		tmp7 = *pTempMatrix;
		pTempMatrix -= 55;

		// Phase 3
		tmp10 = tmp0 + tmp2;
		tmp11 = tmp0 - tmp2;

		// Phases 5-3
		tmp13 = tmp1 + tmp3;
		tmp12 = (((tmp1 - tmp3) * multiplier_1_414213562f + zero_point_five) >> JPEG_DECOMPRESSION_BITS_PRECISION) - tmp13; // 2*c4

		// Phase 2
		tmp0 = tmp10 + tmp13;
		tmp3 = tmp10 - tmp13;
		tmp1 = tmp11 + tmp12;
		tmp2 = tmp11 - tmp12;

		// Phase 6
		z13 = tmp6 + tmp5;
		z10 = tmp6 - tmp5;
		z11 = tmp4 + tmp7;
		z12 = tmp4 - tmp7;

		// Phase 5
		tmp7 = z11 + z13;
		tmp11 = ((z11 - z13) * multiplier_1_414213562f + zero_point_five) >> JPEG_DECOMPRESSION_BITS_PRECISION; // 2*c4

		z5 = ((z10 + z12) * multiplier_1_847759065f + zero_point_five) >> JPEG_DECOMPRESSION_BITS_PRECISION;    // 2*c2
		tmp10 = ((z12 * multiplier_1_0823922f + zero_point_five) >> JPEG_DECOMPRESSION_BITS_PRECISION) - z5;    // 2*(c2-c6)
		tmp12 = z5 - ((z10 *multiplier_2_61312593f + zero_point_five) >> JPEG_DECOMPRESSION_BITS_PRECISION);      // -2*(c2+c6)

		// Phase 2
		tmp6 = tmp12 - tmp7;
		tmp5 = tmp11 - tmp6;
		tmp4 = tmp10 + tmp5;

		// Final output stage: scale down by a factor of 8 (+JPEG_DECOMPRESSION_BITS_PRECISION bits)
		*pMatrix = (imbxInt32)((tmp0 + tmp7 + zero_point_five_by_8)>>(JPEG_DECOMPRESSION_BITS_PRECISION + 3));
		pMatrix += 8;
		*pMatrix = (imbxInt32)((tmp1 + tmp6 + zero_point_five_by_8)>>(JPEG_DECOMPRESSION_BITS_PRECISION + 3));
		pMatrix += 8;
		*pMatrix = (imbxInt32)((tmp2 + tmp5 + zero_point_five_by_8)>>(JPEG_DECOMPRESSION_BITS_PRECISION + 3));
		pMatrix += 8;
		*pMatrix = (imbxInt32)((tmp3 - tmp4 + zero_point_five_by_8)>>(JPEG_DECOMPRESSION_BITS_PRECISION + 3));
		pMatrix += 8;
		*pMatrix = (imbxInt32)((tmp3 + tmp4 + zero_point_five_by_8)>>(JPEG_DECOMPRESSION_BITS_PRECISION + 3));
		pMatrix += 8;
		*pMatrix = (imbxInt32)((tmp2 - tmp5 + zero_point_five_by_8)>>(JPEG_DECOMPRESSION_BITS_PRECISION + 3));
		pMatrix += 8;
		*pMatrix = (imbxInt32)((tmp1 - tmp6 + zero_point_five_by_8)>>(JPEG_DECOMPRESSION_BITS_PRECISION + 3));
		pMatrix += 8;
		*pMatrix = (imbxInt32)((tmp0 - tmp7 + zero_point_five_by_8)>>(JPEG_DECOMPRESSION_BITS_PRECISION + 3));
		pMatrix -= 55;
	}
}


namespace jpeg
{
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
//
// jpegChannel
//
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////

void jpegChannel::processUnprocessedAmplitudes()
{
	if(m_unprocessedAmplitudesCount == 0)
	{
		return;
	}

	imbxInt32* pDest = m_pBuffer + (m_losslessPositionY * m_sizeX + m_losslessPositionX);
	imbxInt32* pSource = m_unprocessedAmplitudesBuffer;

	if(m_unprocessedAmplitudesPredictor == 0)
	{
		while(m_unprocessedAmplitudesCount != 0)
		{
			--m_unprocessedAmplitudesCount;
			*(pDest++) = *(pSource++) & m_valuesMask;
			if(++m_losslessPositionX == m_sizeX)
			{
				m_losslessPositionX = 0;
				++m_losslessPositionY;
			}
		}
		m_lastDCValue = *(pDest - 1);
		return;
	}

	int applyPrediction;
	imbxInt32* pPreviousLine = pDest - m_sizeX;
	imbxInt32* pPreviousLineColumn = pDest - m_sizeX - 1;
	while(m_unprocessedAmplitudesCount != 0)
	{
		--m_unprocessedAmplitudesCount;

		applyPrediction = (int)m_unprocessedAmplitudesPredictor;
		if(m_losslessPositionY == 0)
		{
			applyPrediction = 1;
		}
		else if(m_losslessPositionX == 0)
		{
			applyPrediction = 2;
		}
		switch(applyPrediction)
		{
		case 1:
			m_lastDCValue += *(pSource++);
			break;
		case 2:
			m_lastDCValue = *(pSource++) + *pPreviousLine;
			break;
		case 3:
			m_lastDCValue = *(pSource++) + *pPreviousLineColumn;
			break;
		case 4:
			m_lastDCValue += *(pSource++) + *pPreviousLine - *pPreviousLineColumn;
			break;
		case 5:
			m_lastDCValue += *(pSource++) + ((*pPreviousLine - *pPreviousLineColumn)>>1);
			break;
		case 6:
			m_lastDCValue -= *pPreviousLineColumn;
			m_lastDCValue >>= 1;
			m_lastDCValue += *(pSource++) + *pPreviousLine;
			break;
		case 7:
			m_lastDCValue += *pPreviousLine;
			m_lastDCValue >>= 1;
			m_lastDCValue += *(pSource++);
			break;
		}

		m_lastDCValue &= m_valuesMask;
		*pDest++ = m_lastDCValue;

		++pPreviousLine;
		++pPreviousLineColumn;
		if(++m_losslessPositionX == m_sizeX)
		{
			++m_losslessPositionY;
			m_losslessPositionX = 0;
		}
	}
}

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
//
// jpegCodecTag
//
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Write the tag's length
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void tag::writeLength(streamWriter* pStream, imbxUint16 length)
{
	PUNTOEXE_FUNCTION_START(L"jpeg::tag::writeLength");

	length+=sizeof(length);
	pStream->adjustEndian((imbxUint8*)&length, sizeof(length), streamController::highByteEndian);
	pStream->write((imbxUint8*)&length, sizeof(length));

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Read the tag's length
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
imbxInt32 tag::readLength(streamReader* pStream)
{
	PUNTOEXE_FUNCTION_START(L"jpeg::tag::readLength");

	imbxUint16 length = 0;
	pStream->read((imbxUint8*)&length, sizeof(length));
	pStream->adjustEndian((imbxUint8*)&length, sizeof(length), streamController::highByteEndian);
	if(length > 1)
		length -= 2;
	return (imbxInt32)((imbxUint32)length);

	PUNTOEXE_FUNCTION_END();
}



///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
//
// jpegCodecTagUnknown
//
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Write the tag's content
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void tagUnknown::writeTag(streamWriter* pStream, jpegCodec* /* pCodec */)
{
	PUNTOEXE_FUNCTION_START(L"jpeg::tagUnknown::writeTag");

	writeLength(pStream, 0);

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Read the tag's content
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void tagUnknown::readTag(streamReader* pStream, jpegCodec* /* pCodec */, imbxUint8 /* tagEntry */)
{
	PUNTOEXE_FUNCTION_START(L"jpeg::tagUnknown::readTag");

	imbxInt32 tagLength=readLength(pStream);
	pStream->seek(tagLength, true);

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
//
// jpegCodecTagSOF
//
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Write the tag's content
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void tagSOF::writeTag(streamWriter* pStream, jpegCodec* pCodec)
{
	PUNTOEXE_FUNCTION_START(L"jpeg::tagSOF::writeTag");

	// Calculate the components number
	///////////////////////////////////////////////////////////
	imbxUint8 componentsNumber = (imbxUint8)(pCodec->m_channelsMap.size());

	// Write the tag's length
	///////////////////////////////////////////////////////////
	writeLength(pStream, 6+componentsNumber*3);

	// Write the precision, in bits
	///////////////////////////////////////////////////////////
	imbxUint8 precisionBits=(imbxUint8)pCodec->m_precision;
	pStream->write(&precisionBits, 1);

	// Write the image's size, in pixels
	///////////////////////////////////////////////////////////
	imbxUint16 imageSizeX=(imbxUint16)pCodec->m_imageSizeX;
	imbxUint16 imageSizeY=(imbxUint16)pCodec->m_imageSizeY;
	pStream->adjustEndian((imbxUint8*)&imageSizeY, 2, streamController::highByteEndian);
	pStream->adjustEndian((imbxUint8*)&imageSizeX, 2, streamController::highByteEndian);
	pStream->write((imbxUint8*)&imageSizeY, 2);
	pStream->write((imbxUint8*)&imageSizeX, 2);

	// write the components number
	///////////////////////////////////////////////////////////
	pStream->write((imbxUint8*)&componentsNumber, 1);

	// Write all the components specifications
	///////////////////////////////////////////////////////////
	imbxUint8 componentId;
	imbxUint8 componentSamplingFactor;
	imbxUint8 componentQuantTable;

	for(auto & channelsIterator : pCodec->m_channelsMap)
	{
		ptrChannel pChannel=channelsIterator.second;

		componentId=channelsIterator.first;
		componentSamplingFactor=((imbxUint8)pChannel->m_samplingFactorX<<4) | ((imbxUint8)pChannel->m_samplingFactorY);
		componentQuantTable=(imbxUint8)pChannel->m_quantTable;

		pStream->write((imbxUint8*)&componentId, 1);
		pStream->write((imbxUint8*)&componentSamplingFactor, 1);
		pStream->write((imbxUint8*)&componentQuantTable, 1);
	}

	PUNTOEXE_FUNCTION_END();
}


/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
//
//
// Read the tag's content
//
//
/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
void tagSOF::readTag(streamReader* pStream, jpegCodec* pCodec, imbxUint8 tagEntry)
{
	PUNTOEXE_FUNCTION_START(L"jpeg::tagSOF::readTag");

	imbxInt32 tagLength=readLength(pStream);

	pCodec->m_bLossless=(tagEntry==0xc3) || (tagEntry==0xc7);
	pCodec->m_process=tagEntry-0xc0;

	// Read the precision, in bits
	///////////////////////////////////////////////////////////
	imbxUint8 precisionBits;
	pStream->read(&precisionBits, 1);
	pCodec->m_precision=(int)precisionBits;
	tagLength--;

	// Read the image's size, in pixels
	///////////////////////////////////////////////////////////
	imbxUint16 imageSizeX, imageSizeY;
	pStream->read((imbxUint8*)&imageSizeY, 2);
	pStream->read((imbxUint8*)&imageSizeX, 2);
	pStream->adjustEndian((imbxUint8*)&imageSizeY, 2, streamController::highByteEndian);
	pStream->adjustEndian((imbxUint8*)&imageSizeX, 2, streamController::highByteEndian);
	pCodec->m_imageSizeX=(int)imageSizeX;
	pCodec->m_imageSizeY=(int)imageSizeY;
	tagLength-=4;

	// Read the components number
	///////////////////////////////////////////////////////////
	pCodec->eraseChannels();
	imbxUint8 componentsNumber;
	pStream->read(&componentsNumber, 1);
	tagLength--;

	// Get all the components specifications
	///////////////////////////////////////////////////////////
	imbxUint8 componentId;
	imbxUint8 componentSamplingFactor;
	imbxUint8 componentQuantTable;
	for(imbxUint8 scanComponents = 0; (tagLength > 0) && (scanComponents<componentsNumber); ++scanComponents)
	{
		pStream->read(&componentId, 1);
		pStream->read(&componentSamplingFactor, 1);
		pStream->read(&componentQuantTable, 1);
		tagLength-=3;

		ptrChannel pChannel(new jpeg::jpegChannel);
		pChannel->m_quantTable=(int)componentQuantTable;
		pChannel->m_samplingFactorX=(int)(componentSamplingFactor>>4);
		pChannel->m_samplingFactorY=(int)(componentSamplingFactor & 0x0f);
		pCodec->m_channelsMap[componentId] = pChannel;
	}

	if(tagLength < 0)
	{
		PUNTOEXE_THROW(codecExceptionCorruptedFile, "Corrupted SOF tag found");
	}

	if(tagLength > 0)
		pStream->seek(tagLength, true);

	// Recalculate the MCUs' attributes
	///////////////////////////////////////////////////////////
	pCodec->allocChannels();

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
//
// jpegCodecTagDHT
//
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Write the DHT entry
//
//
/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
void tagDHT::writeTag(streamWriter* pStream, jpegCodec* pCodec)
{
	PUNTOEXE_FUNCTION_START(L"jpeg::tagDHT::writeTag");

	// Used to write bytes into the stream
	/////////////////////////////////////////////////////////////////
	imbxUint8 byte;

	// There are two phases:
	//  the first phase calculates the tag's length, the second one
	//  write the tables' definitions
	/////////////////////////////////////////////////////////////////
	imbxUint16 tagLength = 0;
	for(int phase = 0; phase < 2; ++phase)
	{
		// Write the tag's length
		/////////////////////////////////////////////////////////////////
		if(phase == 1)
			writeLength(pStream, tagLength);

		// Scan all the used tables
		/////////////////////////////////////////////////////////////////
		for(int tableNum = 0; tableNum < 16; ++tableNum)
		{
			// Scan for DC and AC tables
			/////////////////////////////////////////////////////////////////
			for(int DcAc = 0; DcAc < 2; ++DcAc)
			{
				// bAdd is true if the huffman table is used by a channel
				/////////////////////////////////////////////////////////////////
				bool bAdd=false;

				for(jpegCodec::tChannelsMap::iterator channelsIterator = pCodec->m_channelsMap.begin(); !bAdd && channelsIterator!=pCodec->m_channelsMap.end(); ++channelsIterator)
				{
					ptrChannel pChannel=channelsIterator->second;
					bAdd= DcAc==0 ? (tableNum == pChannel->m_huffmanTableDC) : (tableNum == pChannel->m_huffmanTableAC);
				}

				// If the table is used by at least one channel, then write
				//  its definition
				/////////////////////////////////////////////////////////////////
				if(!bAdd)
				{
					continue;
				}
				ptr<huffmanTable> pHuffman;

				if(DcAc==0)
				{
					pHuffman=pCodec->m_pHuffmanTableDC[tableNum];
				}
				else
				{
					pHuffman=pCodec->m_pHuffmanTableAC[tableNum];
				}

				// Calculate the tag's length
				/////////////////////////////////////////////////////////////////
				if(phase == 0)
				{
					pHuffman->incValueFreq(0x100);
					pHuffman->calcHuffmanCodesLength(16);
					// Remove the value 0x100 now
					pHuffman->removeLastCode();

					pHuffman->calcHuffmanTables();
					tagLength+=17;
					for(int scanLength = 0; scanLength < 16;)
					{
						tagLength += (imbxUint16)(pHuffman->m_valuesPerLength[++scanLength]);
					}
					continue;
				}

				// Write the huffman table
				/////////////////////////////////////////////////////////////////

				// Write the table ID
				/////////////////////////////////////////////////////////////////
				imbxUint8 tableID=(imbxUint8)((DcAc<<4) | tableNum);
				pStream->write(&tableID, 1);

				// Write the values per length.
				/////////////////////////////////////////////////////////////////
				int scanLength;
				for(scanLength=0; scanLength<16;)
				{
					byte=(imbxUint8)(pHuffman->m_valuesPerLength[++scanLength]);
					pStream->write(&byte, 1);
				}

				// Write the table values
				/////////////////////////////////////////////////////////////////
				imbxUint32 valueIndex = 0;
				for(scanLength = 0; scanLength < 16; ++scanLength)
				{
					for(imbxUint32 scanValues = 0; scanValues<pHuffman->m_valuesPerLength[scanLength+1]; ++scanValues)
					{
						byte=(imbxUint8)(pHuffman->m_orderedValues[valueIndex++]);
						pStream->write(&byte, 1);
					}
				}
			} // DcAc
		} // tableNum
	} // phase

	PUNTOEXE_FUNCTION_END();
}

/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
//
//
// Read the DHT entry
//
//
/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
void tagDHT::readTag(streamReader* pStream, jpegCodec* pCodec, imbxUint8 /* tagEntry */)
{
	PUNTOEXE_FUNCTION_START(L"jpeg::tagDHT::readTag");

	// Read the tag's length
	/////////////////////////////////////////////////////////////////
	imbxInt32 tagLength=readLength(pStream);

	// Used to read bytes from the stream
	/////////////////////////////////////////////////////////////////
	imbxUint8 byte;

	// Read all the defined tables
	/////////////////////////////////////////////////////////////////
	while(tagLength > 0)
	{
		// Read the table's ID
		/////////////////////////////////////////////////////////////////
		pStream->read(&byte, 1);
		tagLength--;

		// Get a pointer to the right table
		/////////////////////////////////////////////////////////////////
		ptr<huffmanTable> pHuffman;
		if((byte & 0xf0) == 0)
			pHuffman=pCodec->m_pHuffmanTableDC[byte & 0xf];
		else
			pHuffman=pCodec->m_pHuffmanTableAC[byte & 0xf];

		// Reset the table
		/////////////////////////////////////////////////////////////////
		pHuffman->reset();

		// Used to scan all the codes lengths
		/////////////////////////////////////////////////////////////////
		int scanLength;

		// Read the number of codes per length
		/////////////////////////////////////////////////////////////////
		for(scanLength=0; scanLength<16L; )
		{
			pStream->read(&byte, 1);
			pHuffman->m_valuesPerLength[++scanLength]=(imbxUint32)byte;
			--tagLength;
		}

		// Used to store the values into the table
		/////////////////////////////////////////////////////////////////
		imbxUint32 valueIndex = 0;

		// Read all the values and store them into the huffman table
		/////////////////////////////////////////////////////////////////
		for(scanLength = 0; scanLength < 16; ++scanLength)
		{
			for(imbxUint32 scanValues = 0; scanValues < pHuffman->m_valuesPerLength[scanLength+1]; ++scanValues)
			{
				pStream->read(&byte, 1);
				pHuffman->m_orderedValues[valueIndex++]=(imbxUint32)byte;
				--tagLength;
			}
		}

		// Calculate the huffman tables
		/////////////////////////////////////////////////////////////////
		pHuffman->calcHuffmanTables();
	}

	if(tagLength < 0)
	{
		PUNTOEXE_THROW(codecExceptionCorruptedFile, "Corrputed tag DHT found");
	}

	// Move to the end of the tag
	/////////////////////////////////////////////////////////////////
	if(tagLength > 0)
		pStream->seek(tagLength, true);

	PUNTOEXE_FUNCTION_END();
}




/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
//
//
//
// jpegCodecTagSOS
//
//
//
/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
//
//
// Write the SOS entry
//
//
/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
void tagSOS::writeTag(streamWriter* pStream, jpegCodec* pCodec)
{
	PUNTOEXE_FUNCTION_START(L"jpeg::tagSOS::writeTag");

	// Calculate the components number
	/////////////////////////////////////////////////////////////////
	imbxUint8 componentsNumber(0);
	while(pCodec->m_channelsList[componentsNumber] != nullptr)
	{
		++componentsNumber;
	}

	// Write the tag's length
	/////////////////////////////////////////////////////////////////
	writeLength(pStream, 4+2*componentsNumber);

	// Write the component's number
	/////////////////////////////////////////////////////////////////
	pStream->write(&componentsNumber, 1);

	// Scan all the channels in the current scan
	/////////////////////////////////////////////////////////////////
	jpeg::jpegChannel* pChannel; // used in the loop
	for(jpeg::jpegChannel** listIterator = pCodec->m_channelsList; *listIterator != nullptr; ++listIterator)
	{
		pChannel = *listIterator;

		imbxUint8 channelId(0);

		pChannel->m_lastDCValue = pChannel->m_defaultDCValue;

		// Find the channel's ID
		/////////////////////////////////////////////////////////////////
		for(auto & mapIterator : pCodec->m_channelsMap)
		{
			if(mapIterator.second.get() == pChannel)
			{
				channelId=mapIterator.first;
				break;
			}
		}

		// Write the channel's ID
		/////////////////////////////////////////////////////////////////
		pStream->write(&channelId, 1);

		// Write the ac/dc tables ID
		/////////////////////////////////////////////////////////////////
		imbxUint8 acdc=(imbxUint8)((pChannel->m_huffmanTableDC & 0xf)<<4);
		acdc |= (imbxUint8)(pChannel->m_huffmanTableAC & 0xf);

		pStream->write(&acdc, 1);
	}

	imbxUint8 byte;

	// Write the spectral index start
	/////////////////////////////////////////////////////////////////
	byte=(imbxUint8)pCodec->m_spectralIndexStart;
	pStream->write(&byte, 1);

	// Write the spectral index end
	/////////////////////////////////////////////////////////////////
	byte=(imbxUint8)pCodec->m_spectralIndexEnd;
	pStream->write(&byte, 1);

	// Write the hi/lo bit
	/////////////////////////////////////////////////////////////////
	byte=(imbxUint8)(pCodec->m_bitHigh & 0xf)<<4;
	byte|=(imbxUint8)(pCodec->m_bitLow & 0xf);
	pStream->write(&byte, 1);

	PUNTOEXE_FUNCTION_END();
}


/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
//
//
// Read the SOS entry
//
//
/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
void tagSOS::readTag(streamReader* pStream, jpegCodec* pCodec, imbxUint8 /* tagEntry */)
{
	PUNTOEXE_FUNCTION_START(L"jpeg::tagSOS::readTag");

	// Read the tag's length
	/////////////////////////////////////////////////////////////////
	imbxInt32 tagLength=readLength(pStream);

	pCodec->m_eobRun = 0;
	memset(pCodec->m_channelsList, 0, sizeof(pCodec->m_channelsList));

	imbxUint8 componentsNumber;
	pStream->read(&componentsNumber, 1);
	tagLength--;

	imbxUint8 byte;
	for(imbxUint8 scanComponents = 0; (tagLength >= 5) && (scanComponents < componentsNumber); ++scanComponents)
	{
		pStream->read(&byte, 1);
		--tagLength;

		ptrChannel pChannel=pCodec->m_channelsMap[byte];

		pChannel->processUnprocessedAmplitudes();

		pStream->read(&byte, 1);
		--tagLength;

		pChannel->m_huffmanTableDC=byte>>4;
		pChannel->m_huffmanTableAC=byte & 0xf;
		pChannel->m_pActiveHuffmanTableDC = pCodec->m_pHuffmanTableDC[pChannel->m_huffmanTableDC].get();
		pChannel->m_pActiveHuffmanTableAC = pCodec->m_pHuffmanTableAC[pChannel->m_huffmanTableAC].get();

		pChannel->m_lastDCValue = pChannel->m_defaultDCValue;

		pCodec->m_channelsList[scanComponents] = pChannel.get();

	}

	pStream->read(&byte, 1);
	pCodec->m_spectralIndexStart=(int)byte;
	tagLength--;

	pStream->read(&byte, 1);
	pCodec->m_spectralIndexEnd=(int)byte;
	tagLength--;

	pStream->read(&byte, 1);
	pCodec->m_bitHigh=(int)(byte>>4);
	pCodec->m_bitLow=(int)(byte & 0xf);
	tagLength--;

	if(tagLength < 0)
	{
		PUNTOEXE_THROW(codecExceptionCorruptedFile, "Corrupted tag SOS found");
	}

	// Move to the end of the tag
	/////////////////////////////////////////////////////////////////
	if(tagLength > 0)
		pStream->seek(tagLength, true);

	pCodec->findMcuSize();

	PUNTOEXE_FUNCTION_END();
}



/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
//
//
//
// jpegCodecTagDQT
//
//
//
/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
//
//
// Write the DQT tag
//
//
/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
void tagDQT::writeTag(streamWriter* pStream, jpegCodec* pCodec)
{
	PUNTOEXE_FUNCTION_START(L"jpeg::tagDQT::writeTag");

	// Read the tag's length
	/////////////////////////////////////////////////////////////////
	imbxInt16 tagLength = 0;

	imbxUint8  tablePrecision;
	imbxUint8  tableValue8;
	imbxUint16 tableValue16;

	for(int phase = 0; phase < 2; ++phase)
	{
		if(phase != 0)
		{
			writeLength(pStream, tagLength);
		}
		for(imbxUint8 tableId = 0; tableId < 16; ++tableId)
		{
			// bAdd is true if the huffman table is used by a channel
			/////////////////////////////////////////////////////////////////
			bool bAdd=false;

			for(jpegCodec::tChannelsMap::iterator channelsIterator=pCodec->m_channelsMap.begin(); !bAdd && channelsIterator!=pCodec->m_channelsMap.end(); ++channelsIterator)
			{
				ptrChannel pChannel=channelsIterator->second;
				bAdd=pChannel->m_quantTable==tableId;
			}

			if(!bAdd)
			{
				continue;
			}
			// Calculate the table's precision
			bool b16Bits = pCodec->m_precision > 8;
			for(int tableIndex = 0; !b16Bits && (tableIndex < 64); ++tableIndex)
			{
				if(pCodec->m_quantizationTable[tableId][tableIndex] >= 256)
				{
					b16Bits=true;
				}
			}

			if(phase == 0)
			{
				tagLength += 1+(b16Bits ? 128 : 64);
			}
			else
			{
				tablePrecision = tableId | (b16Bits ? 0x10 : 0);
				pStream->write(&tablePrecision, 1);
				if(b16Bits)
				{
					for(unsigned int tableIndex : JpegDeZigZagOrder)
					{
						tableValue16 = (imbxUint16)pCodec->m_quantizationTable[tableId][tableIndex];
						pStream->adjustEndian((imbxUint8*)&tableValue16, 2, streamController::highByteEndian);
						pStream->write((imbxUint8*)&tableValue16, 2);
					}
				}
				else
				{
					for(unsigned int tableIndex : JpegDeZigZagOrder)
					{
						tableValue8=(imbxUint8)pCodec->m_quantizationTable[tableId][tableIndex];
						pStream->write(&tableValue8, 1);
					}
				}
			}

			pCodec->recalculateQuantizationTables(tableId);
		}
	}

	PUNTOEXE_FUNCTION_END();
}


/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
//
//
// Read the DQT tag
//
//
/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
void tagDQT::readTag(streamReader* pStream, jpegCodec* pCodec, imbxUint8 /* tagEntry */)
{
	PUNTOEXE_FUNCTION_START(L"jpeg::tagDQT::readTag");

	// Read the tag's length
	/////////////////////////////////////////////////////////////////
	imbxInt32 tagLength=readLength(pStream);

	imbxUint8  tablePrecision;
	imbxUint8  tableValue8;
	imbxUint16 tableValue16;
	while(tagLength > 0)
	{
		pStream->read(&tablePrecision, 1);
		tagLength--;

		// Read a DQT table
		/////////////////////////////////////////////////////////////////
		for(unsigned int tableIndex : JpegDeZigZagOrder)
		{
			// 16 bits precision
			/////////////////////////////////////////////////////////////////
			if((tablePrecision & 0xf0) != 0)
			{
				pStream->read((imbxUint8*)&tableValue16, 2);
				tagLength-=2;
				pStream->adjustEndian((imbxUint8*)&tableValue16, 2, streamController::highByteEndian);
				pCodec->m_quantizationTable[tablePrecision & 0x0f][tableIndex]=tableValue16;
			}

			// 8 bits precision
			/////////////////////////////////////////////////////////////////
			else
			{
				pStream->read(&tableValue8, 1);
				tagLength--;
				pCodec->m_quantizationTable[tablePrecision & 0x0f][tableIndex]=tableValue8;
			}


		} // ----- End of table reading

		pCodec->recalculateQuantizationTables(tablePrecision & 0x0f);
	}

	if(tagLength < 0)
	{
		PUNTOEXE_THROW(codecExceptionCorruptedFile, "Corrupted tag DQT found");
	}

	// Move to the end of the tag
	/////////////////////////////////////////////////////////////////
	if(tagLength > 0)
		pStream->seek(tagLength, true);

	PUNTOEXE_FUNCTION_END();
}



/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
//
//
//
// jpegCodecTagDRI
//
//
//
/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
//
//
// Write the DRI tag
//
//
/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
void tagDRI::writeTag(streamWriter* pStream, jpegCodec* pCodec)
{
	PUNTOEXE_FUNCTION_START(L"jpeg::tagDRI::writeTag");

	// Write the tag's length
	/////////////////////////////////////////////////////////////////
	writeLength(pStream, 2);

	// Write the MCU per restart interval
	/////////////////////////////////////////////////////////////////
	imbxUint16 unitsPerRestartInterval=pCodec->m_mcuPerRestartInterval;
	pStream->adjustEndian((imbxUint8*)&unitsPerRestartInterval, 2, streamController::highByteEndian);
	pStream->write((imbxUint8*)&unitsPerRestartInterval, 2);

	PUNTOEXE_FUNCTION_END();
}


/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
//
//
// Read the DRI tag
//
//
/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
void tagDRI::readTag(streamReader* pStream, jpegCodec* pCodec, imbxUint8 /* tagEntry */)
{
	PUNTOEXE_FUNCTION_START(L"jpeg::tagDRI::readTag");

	// Read the tag's length
	/////////////////////////////////////////////////////////////////
	imbxInt32 tagLength=readLength(pStream);

	imbxUint16 unitsPerRestartInterval;
	pStream->read((imbxUint8*)&unitsPerRestartInterval, 2);
	tagLength-=2;
	pStream->adjustEndian((imbxUint8*)&unitsPerRestartInterval, 2, streamController::highByteEndian);
	pCodec->m_mcuPerRestartInterval=unitsPerRestartInterval;

	if(tagLength < 0)
	{
		PUNTOEXE_THROW(codecExceptionCorruptedFile, "Corrupted tag DRI found");
	}

	// Move to the end of the tag
	/////////////////////////////////////////////////////////////////
	if(tagLength > 0)
		pStream->seek(tagLength, true);

	PUNTOEXE_FUNCTION_END();
}



/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
//
//
//
// jpegCodecTagRST
//
//
//
/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
//
//
// Write the RST tag
//
//
/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
void tagRST::writeTag(streamWriter* /* pStream */, jpegCodec* /* pCodec */)
{
	return;
}


/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
//
//
// Read the RST tag
//
//
/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
void tagRST::readTag(streamReader* /* pStream */, jpegCodec* pCodec, imbxUint8 tagEntry)
{
	PUNTOEXE_FUNCTION_START(L"jpeg::tagRST::readTag");

	// Reset the channels last dc value
	/////////////////////////////////////////////////////////////////
	for(jpeg::jpegChannel** channelsIterator = pCodec->m_channelsList; *channelsIterator != nullptr; ++channelsIterator)
	{
		(*channelsIterator)->processUnprocessedAmplitudes();
		(*channelsIterator)->m_lastDCValue = (*channelsIterator)->m_defaultDCValue;
	}

	// Calculate the mcu processed counter
	/////////////////////////////////////////////////////////////////
	if(pCodec->m_mcuPerRestartInterval > 0)
	{
		imbxUint32 doneRestartInterval=(pCodec->m_mcuProcessed+pCodec->m_mcuPerRestartInterval-1)/pCodec->m_mcuPerRestartInterval-1;
		imbxUint8 doneRestartIntervalID=(imbxUint8)(doneRestartInterval & 0x7);
		imbxUint8 foundRestartIntervalID=tagEntry & 0x7;
		if(foundRestartIntervalID<doneRestartIntervalID)
			doneRestartInterval+=8L;
		doneRestartInterval-=doneRestartIntervalID;
		doneRestartInterval+=foundRestartIntervalID;
		pCodec->m_mcuProcessed=(doneRestartInterval+1)*pCodec->m_mcuPerRestartInterval;
		pCodec->m_mcuProcessedY = pCodec->m_mcuProcessed / pCodec->m_mcuNumberX;
		pCodec->m_mcuProcessedX = pCodec->m_mcuProcessed - (pCodec->m_mcuProcessedY * pCodec->m_mcuNumberX);
		pCodec->m_mcuLastRestart = pCodec->m_mcuProcessed;

		// Update the lossless pixel's counter in the channels
		/////////////////////////////////////////////////////////////////
		for(jpeg::jpegChannel** channelsIterator = pCodec->m_channelsList; *channelsIterator != nullptr; ++channelsIterator)
		{
			jpeg::jpegChannel* pChannel(*channelsIterator);
			pChannel->m_losslessPositionX = pCodec->m_mcuProcessedX / pChannel->m_blockMcuX;
			pChannel->m_losslessPositionY = pCodec->m_mcuProcessedY / pChannel->m_blockMcuY;
		}
	}

	pCodec->m_eobRun = 0;

	PUNTOEXE_FUNCTION_END();
}



/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
//
//
//
// jpegCodecTagEOI
//
//
//
/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
//
//
// Write the EOI tag
//
//
/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
void tagEOI::writeTag(streamWriter* pStream, jpegCodec* /* pCodec */)
{
	PUNTOEXE_FUNCTION_START(L"jpeg::tagEOI::writeTag");

	writeLength(pStream, 0);

	PUNTOEXE_FUNCTION_END();
}


/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
//
//
// Read the EOI tag
//
//
/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
void tagEOI::readTag(streamReader* /* pStream */, jpegCodec* pCodec, imbxUint8 /* tagEntry */)
{
	pCodec->m_bEndOfImage=true;
}

} // namespace jpeg

} // namespace codecs

} // namespace imebra

} // namespace puntoexe



