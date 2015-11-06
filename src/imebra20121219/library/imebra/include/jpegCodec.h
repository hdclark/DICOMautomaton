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

/*! \file jpegCodec.h
    \brief Declaration of the class jpegCodec.

*/

#if !defined(imebraJpegCodec_7F63E846_8824_42c6_A048_DD59C657AED4__INCLUDED_)
#define imebraJpegCodec_7F63E846_8824_42c6_A048_DD59C657AED4__INCLUDED_

#include "codec.h"
#include <map>
#include <list>

///////////////////////////////////////////////////////////
//
// Everything is in the namespace puntoexe::imebra
//
///////////////////////////////////////////////////////////
namespace puntoexe
{

class huffmanTable;

namespace imebra
{

namespace codecs
{

/// \addtogroup group_codecs
///
/// @{

// The following classes are used in the jpegCodec
//  declaration
///////////////////////////////////////////////////////////
namespace jpeg
{
	class tag;
	class jpegChannel;
}

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
/// \brief The Jpeg codec.
///
/// This class is used to decode and encode a Jpeg stream.
///
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
class jpegCodec : public codec
{
	friend class jpeg::tag;

public:
	// Constructor
	///////////////////////////////////////////////////////////
	jpegCodec();

	// Allocate the image's channels
	///////////////////////////////////////////////////////////
	void allocChannels();

	// Find the mcu's size
	///////////////////////////////////////////////////////////
	void findMcuSize();

	// Recalculate the tables for dequantization/quantization
	void recalculateQuantizationTables(int table);


	// Erase the allocated channels
	///////////////////////////////////////////////////////////
	void eraseChannels();

	// Retrieve the image from a dataset
	///////////////////////////////////////////////////////////
	virtual ptr<image> getImage(ptr<dataSet> sourceDataSet, ptr<streamReader> pStream, std::string dataType);

	// Insert a jpeg compressed image into a dataset
	///////////////////////////////////////////////////////////
	virtual void setImage(
		ptr<streamWriter> pDestStream,
		ptr<image> pImage,
		std::wstring transferSyntax,
		quality imageQuality,
		std::string dataType,
		imbxUint8 allocatedBits,
		bool bSubSampledX,
		bool bSubSampledY,
		bool bInterleaved,
		bool b2Complement);

	// Return true if the codec can handle the transfer
	///////////////////////////////////////////////////////////
	virtual bool canHandleTransferSyntax(std::wstring transferSyntax);

	// Returns true if the transfer syntax has to be
	//  encapsulated
	//
	///////////////////////////////////////////////////////////
	virtual bool encapsulated(std::wstring transferSyntax);

	// Return the highest bit that the transfer syntax can
	//  handle
	///////////////////////////////////////////////////////////
	virtual imbxUint32 getMaxHighBit(std::string transferSyntax);

	// Return the suggested allocated bits
	///////////////////////////////////////////////////////////
	virtual imbxUint32 suggestAllocatedBits(std::wstring transferSyntax, imbxUint32 highBit);

	// Create another jpeg codec
	///////////////////////////////////////////////////////////
	virtual ptr<codec> createCodec();

protected:
	// Destructor
	///////////////////////////////////////////////////////////
	virtual ~jpegCodec();

	// Read a jpeg stream and build a Dicom dataset
	///////////////////////////////////////////////////////////
	virtual void readStream(ptr<streamReader> pSourceStream, ptr<dataSet> pDataSet, imbxUint32 maxSizeBufferLoad = 0xffffffff);

	// Write a Dicom dataset as a Jpeg stream
	///////////////////////////////////////////////////////////
	virtual void writeStream(ptr<streamWriter> pSourceStream, ptr<dataSet> pDataSet);

	///////////////////////////////////////////////////////////
	//
	// Image's attributes. Used while reading/writing an image
	//
	///////////////////////////////////////////////////////////
public:
	// The image's size, in pixels
	///////////////////////////////////////////////////////////
	imbxUint32 m_imageSizeX;
	imbxUint32 m_imageSizeY;

	// Encoding process
	///////////////////////////////////////////////////////////
	imbxUint8  m_process;

	// The bits per color component
	///////////////////////////////////////////////////////////
	int m_precision;
	imbxInt32 m_valuesMask;

	// true when the end of the image has been reached
	///////////////////////////////////////////////////////////
	bool m_bEndOfImage;

	// The allocated channels
	///////////////////////////////////////////////////////////
	typedef ptr<jpeg::jpegChannel> ptrChannel;
	typedef std::map<imbxUint8, ptrChannel> tChannelsMap;
	tChannelsMap m_channelsMap;

	// The list of the channels in the active scan, zero
	//  terminated
	///////////////////////////////////////////////////////////
	jpeg::jpegChannel* m_channelsList[257]; // 256 channels + terminator

	// Huffman tables
	///////////////////////////////////////////////////////////
	ptr<huffmanTable> m_pHuffmanTableDC[16];
	ptr<huffmanTable> m_pHuffmanTableAC[16];

	//
	// Quantization tables
	//
	///////////////////////////////////////////////////////////
	imbxUint32 m_quantizationTable[16][64];

	// The number of MCUs per restart interval
	///////////////////////////////////////////////////////////
	imbxUint16 m_mcuPerRestartInterval;

	// The number of processed MCUs
	///////////////////////////////////////////////////////////
	imbxUint32 m_mcuProcessed;
	imbxUint32 m_mcuProcessedX;
	imbxUint32 m_mcuProcessedY;

	// The length of the EOB run
	///////////////////////////////////////////////////////////
	imbxUint32 m_eobRun;

	// The last found restart interval
	///////////////////////////////////////////////////////////
	imbxUint32 m_mcuLastRestart;

	// Spectral index and progressive bits reading
	///////////////////////////////////////////////////////////
	imbxUint32 m_spectralIndexStart;
	imbxUint32 m_spectralIndexEnd;
	imbxUint32 m_bitHigh;
	imbxUint32 m_bitLow;

	// true if we are reading a lossless jpeg image
	///////////////////////////////////////////////////////////
	bool m_bLossless;

	// The maximum sampling factor
	///////////////////////////////////////////////////////////
	imbxUint32 m_maxSamplingFactorX;
	imbxUint32 m_maxSamplingFactorY;

	// The number of MCUs (horizontal, vertical, total)
	///////////////////////////////////////////////////////////
	imbxUint32 m_mcuNumberX;
	imbxUint32 m_mcuNumberY;
	imbxUint32 m_mcuNumberTotal;


	// The image's size, rounded to accomodate all the MCUs
	///////////////////////////////////////////////////////////
	imbxUint32 m_jpegImageSizeX;
	imbxUint32 m_jpegImageSizeY;


	// FDCT/IDCT
	///////////////////////////////////////////////////////////
public:
	void FDCT(imbxInt32* pIOMatrix, float* pDescaleFactors);
	void IDCT(imbxInt32* pIOMatrix, long long* pScaleFactors);

protected:
	/// \internal
	/// \brief This enumeration contains the tags used by
	///         the jpeg codec
	///////////////////////////////////////////////////////////
	enum tTagId
	{
		unknown = 0xff,

		sof0 = 0xc0,
		sof1 = 0xc1,
		sof2 = 0xc2,
		sof3 = 0xc3,

		dht = 0xc4,

		sof5 = 0xc5,
		sof6 = 0xc6,
		sof7 = 0xc7,

		sof9 = 0xc9,
		sofA = 0xca,
		sofB = 0xcb,

		sofD = 0xcd,
		sofE = 0xce,
		sofF = 0xcf,

		rst0 = 0xd0,
		rst1 = 0xd1,
		rst2 = 0xd2,
		rst3 = 0xd3,
		rst4 = 0xd4,
		rst5 = 0xd5,
		rst6 = 0xd6,
		rst7 = 0xd7,

		eoi = 0xd9,
		sos = 0xda,
		dqt = 0xdb,

		dri = 0xdd
	};

	// Register a tag in the jpeg codec
	///////////////////////////////////////////////////////////
	void registerTag(tTagId tagId, ptr<jpeg::tag> pTag);

	// Read a lossy block of pixels
	///////////////////////////////////////////////////////////
	inline void readBlock(streamReader* pStream, imbxInt32* pBuffer, jpeg::jpegChannel* pChannel);

	// Write a lossy block of pixels
	///////////////////////////////////////////////////////////
	inline void writeBlock(streamWriter* pStream, imbxInt32* pBuffer, jpeg::jpegChannel* pChannel, bool bCalcHuffman);

	// Reset the internal variables
	///////////////////////////////////////////////////////////
	void resetInternal(bool bCompression, quality compQuality);

	void copyJpegChannelsToImage(ptr<image> destImage, bool b2complement, std::wstring colorSpace);
	void copyImageToJpegChannels(ptr<image> sourceImage, bool b2complement, imbxUint8 allocatedBits, bool bSubSampledX, bool bSubSampledY);

	void writeScan(streamWriter* pDestinationStream, bool bCalcHuffman);

	void writeTag(streamWriter* pDestinationStream, tTagId tagId);

	long long m_decompressionQuantizationTable[16][64];
	float m_compressionQuantizationTable[16][64];

	// Map of the available Jpeg tags
	///////////////////////////////////////////////////////////
	typedef ptr<jpeg::tag> ptrTag;
	typedef std::map<imbxUint8, ptrTag> tTagsMap;
	tTagsMap m_tagsMap;

	// temporary matrix used by FDCT
	///////////////////////////////////////////////////////////
	float m_fdctTempMatrix[64];

	// temporary matrix used by IDCT
	///////////////////////////////////////////////////////////
	long long m_idctTempMatrix[64];
};

/// \brief Base class for the exceptions thrown by the
///        jpeg codec
///
///////////////////////////////////////////////////////////
class jpegCodecException: public codecException
{
public:
	jpegCodecException(const std::string& message): codecException(message){}
};

/// \brief Exception thrown when the jpeg variant cannot
///         be handled.
///
///////////////////////////////////////////////////////////
class jpegCodecCannotHandleSyntax: public jpegCodecException
{
public:
	/// \brief Constructs the exception.
	///
	/// @param message   the cause of the exception
	///
	///////////////////////////////////////////////////////////
	jpegCodecCannotHandleSyntax(const std::string& message): jpegCodecException(message){}
};

/// @}

namespace jpeg
{

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// CImbsJpegCodecChannel
// An image's channel
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
class jpegChannel : public channel
{
public:
	// Constructor
	///////////////////////////////////////////////////////////
	jpegChannel():
		m_quantTable(0),
		m_blockMcuX(0),
		m_blockMcuY(0),
		m_blockMcuXY(0),
		m_lastDCValue(0),
		m_defaultDCValue(0),
		m_losslessPositionX(0),
		m_losslessPositionY(0),
		m_unprocessedAmplitudesCount(0),
		m_unprocessedAmplitudesPredictor(0),
		m_huffmanTableDC(0),
		m_huffmanTableAC(0),
		m_pActiveHuffmanTableDC(0),
		m_pActiveHuffmanTableAC(0),
		m_valuesMask(0){}

	// Quantization table's id
	///////////////////////////////////////////////////////////
	int m_quantTable;

	// Blocks per MCU
	///////////////////////////////////////////////////////////
	int m_blockMcuX;
	int m_blockMcuY;
	int m_blockMcuXY;

	// Last DC value
	///////////////////////////////////////////////////////////
	imbxInt32 m_lastDCValue;

	// Default DC value
	///////////////////////////////////////////////////////////
	imbxInt32 m_defaultDCValue;

	// Lossless position
	///////////////////////////////////////////////////////////
	imbxUint32 m_losslessPositionX;
	imbxUint32 m_losslessPositionY;

	imbxInt32 m_unprocessedAmplitudesBuffer[1024];
	imbxUint32 m_unprocessedAmplitudesCount;
	imbxUint32 m_unprocessedAmplitudesPredictor;

	// Huffman tables' id
	///////////////////////////////////////////////////////////
	int m_huffmanTableDC;
	int m_huffmanTableAC;
	huffmanTable* m_pActiveHuffmanTableDC;
	huffmanTable* m_pActiveHuffmanTableAC;

	imbxInt32 m_valuesMask;

	inline void addUnprocessedAmplitude(imbxInt32 unprocessedAmplitude, imbxUint32 predictor, bool bMcuRestart)
	{
		if(bMcuRestart ||
			predictor != m_unprocessedAmplitudesPredictor ||
			m_unprocessedAmplitudesCount == sizeof(m_unprocessedAmplitudesBuffer) / sizeof(m_unprocessedAmplitudesBuffer[0]))
		{
			processUnprocessedAmplitudes();
			if(bMcuRestart)
			{
				m_unprocessedAmplitudesPredictor = 0;
				m_unprocessedAmplitudesBuffer[0] = unprocessedAmplitude + m_defaultDCValue;
			}
			else
			{
				m_unprocessedAmplitudesPredictor = predictor;
				m_unprocessedAmplitudesBuffer[0] = unprocessedAmplitude;
			}
			++m_unprocessedAmplitudesCount;
			return;
		}
		m_unprocessedAmplitudesBuffer[m_unprocessedAmplitudesCount++] = unprocessedAmplitude;
	}

	void processUnprocessedAmplitudes();
};


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// jpegCodecTag
//
// The base class for all the jpeg tags
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
class tag : public baseObject
{
public:
	typedef ptr<jpeg::jpegChannel> ptrChannel;

public:
	// Write the tag's content.
	// The function should call WriteLength first.
	///////////////////////////////////////////////////////////
	virtual void writeTag(streamWriter* pStream, jpegCodec* pCodec)=0;

	// Read the tag's content. The function should call
	//  ReadLength first.
	///////////////////////////////////////////////////////////
	virtual void readTag(streamReader* pStream, jpegCodec* pCodec, imbxUint8 tagEntry)=0;

protected:
	// Write the tag's length
	///////////////////////////////////////////////////////////
	void writeLength(streamWriter* pStream, imbxUint16 length);

	// Read the tag's length
	///////////////////////////////////////////////////////////
	imbxInt32 readLength(streamReader* pStream);
};


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// jpegCodecTagUnknown
//
// Read/write an unknown tag
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
class tagUnknown: public tag
{
public:
	// Write the tag's content.
	///////////////////////////////////////////////////////////
	virtual void writeTag(streamWriter* pStream, jpegCodec* pCodec);

	// Read the tag's content.
	///////////////////////////////////////////////////////////
	virtual void readTag(streamReader* pStream, jpegCodec* pCodec, imbxUint8 tagEntry);

};


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// jpegCodecTagSOF
//
// Read/write a SOF tag
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
class tagSOF: public tag
{
public:
	// Write the tag's content.
	///////////////////////////////////////////////////////////
	virtual void writeTag(streamWriter* pStream, jpegCodec* pCodec);

	// Read the tag's content.
	///////////////////////////////////////////////////////////
	virtual void readTag(streamReader* pStream, jpegCodec* pCodec, imbxUint8 tagEntry);

};


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Read/write a DHT tag
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
class tagDHT: public tag
{
public:
	// Write the tag's content.
	///////////////////////////////////////////////////////////
	virtual void writeTag(streamWriter* pStream, jpegCodec* pCodec);

	// Read the tag's content.
	///////////////////////////////////////////////////////////
	virtual void readTag(streamReader* pStream, jpegCodec* pCodec, imbxUint8 tagEntry);

};


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Read/write a SOS tag
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
class tagSOS: public tag
{
public:
	// Write the tag's content.
	///////////////////////////////////////////////////////////
	virtual void writeTag(streamWriter* pStream, jpegCodec* pCodec);

	// Read the tag's content.
	///////////////////////////////////////////////////////////
	virtual void readTag(streamReader* pStream, jpegCodec* pCodec, imbxUint8 tagEntry);

};


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Read/write a DQT tag
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
class tagDQT: public tag
{
public:
	// Write the tag's content.
	///////////////////////////////////////////////////////////
	virtual void writeTag(streamWriter* pStream, jpegCodec* pCodec);

	// Read the tag's content.
	///////////////////////////////////////////////////////////
	virtual void readTag(streamReader* pStream, jpegCodec* pCodec, imbxUint8 tagEntry);

};


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Read/write a DRI tag
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
class tagDRI: public tag
{
public:
	// Write the tag's content.
	///////////////////////////////////////////////////////////
	virtual void writeTag(streamWriter* pStream, jpegCodec* pCodec);

	// Read the tag's content.
	///////////////////////////////////////////////////////////
	virtual void readTag(streamReader* pStream, jpegCodec* pCodec, imbxUint8 tagEntry);

};


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Read/write a RST tag
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
class tagRST: public tag
{
public:
	// Write the tag's content.
	///////////////////////////////////////////////////////////
	virtual void writeTag(streamWriter* pStream, jpegCodec* pCodec);

	// Read the tag's content.
	///////////////////////////////////////////////////////////
	virtual void readTag(streamReader* pStream, jpegCodec* pCodec, imbxUint8 tagEntry);

};


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Read/write an EOI tag
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
class tagEOI: public tag
{
public:
	// Write the tag's content.
	///////////////////////////////////////////////////////////
	virtual void writeTag(streamWriter* pStream, jpegCodec* pCodec);

	// Read the tag's content.
	///////////////////////////////////////////////////////////
	virtual void readTag(streamReader* pStream, jpegCodec* pCodec, imbxUint8 tagEntry);

};

} // namespace jpegCodec

} // namespace codecs

} // namespace imebra

} // namespace puntoexe

#endif // !defined(imebraJpegCodec_7F63E846_8824_42c6_A048_DD59C657AED4__INCLUDED_)
