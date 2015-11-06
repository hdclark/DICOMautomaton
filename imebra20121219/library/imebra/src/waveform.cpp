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

/*! \file waveform.cpp
    \brief Implementation of the class waveform.

*/

#include "../include/waveform.h"
#include "../include/dataHandlerNumeric.h"
#include "../include/dataSet.h"

namespace puntoexe
{

namespace imebra
{

///////////////////////////////////////////////////////////
//
// Constructor
//
///////////////////////////////////////////////////////////
waveform::waveform(ptr<dataSet> pDataSet):
	m_pDataSet(pDataSet)
{
}


///////////////////////////////////////////////////////////
//
// Returns the number of allocated bits
//
///////////////////////////////////////////////////////////
imbxUint32 waveform::getBitsAllocated()
{
	PUNTOEXE_FUNCTION_START(L"waveform::getBitsAllocated");

	return m_pDataSet->getUnsignedLong(0x5400, 0, 0x1004, 0);

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
//
// Returns the number of bits stored
//
///////////////////////////////////////////////////////////
imbxUint32 waveform::getBitsStored()
{
	PUNTOEXE_FUNCTION_START(L"waveform::getBitsStored");

	return m_pDataSet->getUnsignedLong(0x003A, 0, 0x021A, 0);

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
//
// Returns the number of channels
//
///////////////////////////////////////////////////////////
imbxUint32 waveform::getChannels()
{
	PUNTOEXE_FUNCTION_START(L"waveform::getChannels");

	return m_pDataSet->getUnsignedLong(0x003A, 0, 0x0005, 0);

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
//
// Returns the interpretation string
//
///////////////////////////////////////////////////////////
std::string waveform::getInterpretation()
{
	PUNTOEXE_FUNCTION_START(L"waveform::getChannels");

	return m_pDataSet->getString(0x5400, 0, 0x1006, 0);

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
//
// Returns the number of samples
//
///////////////////////////////////////////////////////////
imbxUint32 waveform::getSamples()
{
	PUNTOEXE_FUNCTION_START(L"waveform::getSamples");

	return m_pDataSet->getUnsignedLong(0x003A, 0, 0x0010, 0);

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
//
// Returns a data handler for the waveform
//
///////////////////////////////////////////////////////////
ptr<handlers::dataHandler> waveform::getIntegerData(imbxUint32 channel, imbxInt32 paddingValue)
{
	PUNTOEXE_FUNCTION_START(L"waveform::getIntegerData");

	static imbxInt32 uLawDecompressTable[256] =
	{
		-32124,-31100,-30076,-29052,-28028,-27004,-25980,-24956,
		-23932,-22908,-21884,-20860,-19836,-18812,-17788,-16764,
		-15996,-15484,-14972,-14460,-13948,-13436,-12924,-12412,
		-11900,-11388,-10876,-10364, -9852, -9340, -8828, -8316,
		-7932, -7676, -7420, -7164, -6908, -6652, -6396, -6140,
		-5884, -5628, -5372, -5116, -4860, -4604, -4348, -4092,
		-3900, -3772, -3644, -3516, -3388, -3260, -3132, -3004,
		-2876, -2748, -2620, -2492, -2364, -2236, -2108, -1980,
		-1884, -1820, -1756, -1692, -1628, -1564, -1500, -1436,
		-1372, -1308, -1244, -1180, -1116, -1052,  -988,  -924,
		-876,  -844,  -812,  -780,  -748,  -716,  -684,  -652,
		-620,  -588,  -556,  -524,  -492,  -460,  -428,  -396,
		-372,  -356,  -340,  -324,  -308,  -292,  -276,  -260,
		-244,  -228,  -212,  -196,  -180,  -164,  -148,  -132,
		-120,  -112,  -104,   -96,   -88,   -80,   -72,   -64,
		-56,   -48,   -40,   -32,   -24,   -16,    -8,     0,
		32124, 31100, 30076, 29052, 28028, 27004, 25980, 24956,
		23932, 22908, 21884, 20860, 19836, 18812, 17788, 16764,
		15996, 15484, 14972, 14460, 13948, 13436, 12924, 12412,
		11900, 11388, 10876, 10364,  9852,  9340,  8828,  8316,
		7932,  7676,  7420,  7164,  6908,  6652,  6396,  6140,
		5884,  5628,  5372,  5116,  4860,  4604,  4348,  4092,
		3900,  3772,  3644,  3516,  3388,  3260,  3132,  3004,
		2876,  2748,  2620,  2492,  2364,  2236,  2108,  1980,
		1884,  1820,  1756,  1692,  1628,  1564,  1500,  1436,
		1372,  1308,  1244,  1180,  1116,  1052,   988,   924,
		876,   844,   812,   780,   748,   716,   684,   652,
		620,   588,   556,   524,   492,   460,   428,   396,
		372,   356,   340,   324,   308,   292,   276,   260,
		244,   228,   212,   196,   180,   164,   148,   132,
		120,   112,   104,    96,    88,    80,    72,    64,
		56,    48,    40,    32,    24,    16,     8,     0
	};

	static imbxInt32 aLawDecompressTable[256] =
	{
		-5504, -5248, -6016, -5760, -4480, -4224, -4992, -4736,
		-7552, -7296, -8064, -7808, -6528, -6272, -7040, -6784,
		-2752, -2624, -3008, -2880, -2240, -2112, -2496, -2368,
		-3776, -3648, -4032, -3904, -3264, -3136, -3520, -3392,
		-22016,-20992,-24064,-23040,-17920,-16896,-19968,-18944,
		-30208,-29184,-32256,-31232,-26112,-25088,-28160,-27136,
		-11008,-10496,-12032,-11520,-8960, -8448, -9984, -9472,
		-15104,-14592,-16128,-15616,-13056,-12544,-14080,-13568,
		-344,  -328,  -376,  -360,  -280,  -264,  -312,  -296,
		-472,  -456,  -504,  -488,  -408,  -392,  -440,  -424,
		-88,   -72,   -120,  -104,  -24,   -8,    -56,   -40,
		-216,  -200,  -248,  -232,  -152,  -136,  -184,  -168,
		-1376, -1312, -1504, -1440, -1120, -1056, -1248, -1184,
		-1888, -1824, -2016, -1952, -1632, -1568, -1760, -1696,
		-688,  -656,  -752,  -720,  -560,  -528,  -624,  -592,
		-944,  -912,  -1008, -976,  -816,  -784,  -880,  -848,
		5504,  5248,  6016,  5760,  4480,  4224,  4992,  4736,
		7552,  7296,  8064,  7808,  6528,  6272,  7040,  6784,
		2752,  2624,  3008,  2880,  2240,  2112,  2496,  2368,
		3776,  3648,  4032,  3904,  3264,  3136,  3520,  3392,
		22016, 20992, 24064, 23040, 17920, 16896, 19968, 18944,
		30208, 29184, 32256, 31232, 26112, 25088, 28160, 27136,
		11008, 10496, 12032, 11520, 8960,  8448,  9984,  9472,
		15104, 14592, 16128, 15616, 13056, 12544, 14080, 13568,
		344,   328,   376,   360,   280,   264,   312,   296,
		472,   456,   504,   488,   408,   392,   440,   424,
		88,    72,   120,   104,    24,     8,    56,    40,
		216,   200,   248,   232,   152,   136,   184,   168,
		1376,  1312,  1504,  1440,  1120,  1056,  1248,  1184,
		1888,  1824,  2016,  1952,  1632,  1568,  1760,  1696,
		688,   656,   752,   720,   560,   528,   624,   592,
		944,   912,  1008,   976,   816,   784,   880,   848
	}; 

	// Lock the dataset during the interpretation of the 
	//  dataset
	///////////////////////////////////////////////////////////
	lockObject lockDataSet(m_pDataSet);

	// Get the original data
	///////////////////////////////////////////////////////////
	ptr<handlers::dataHandler> waveformData(m_pDataSet->getDataHandler(0x5400, 0x0, 0x1010, 0, false));
	std::string sourceDataType(waveformData->getDataType());
	
	// Get the interpretation, number of channels, number of
	//  samples
	///////////////////////////////////////////////////////////
	std::string waveformInterpretation(getInterpretation());
	imbxUint32 numChannels(getChannels());
	imbxUint32 numSamples(getSamples());
	imbxUint32 originalPaddingValue(0);
	bool bPaddingValueExists(false);
	ptr<handlers::dataHandler> paddingTagHandler(m_pDataSet->getDataHandler(0x5400, 0, 0x100A, 0, false));
	if(paddingTagHandler != 0)
	{
		originalPaddingValue = paddingTagHandler->getUnsignedLong(0);
		bPaddingValueExists = true;
	}

	
	// Allocate a buffer for the destination data
	///////////////////////////////////////////////////////////
	ptr<buffer> waveformBuffer(new buffer(0, "SL"));
	ptr<handlers::dataHandler> destinationHandler(waveformBuffer->getDataHandler(true, numSamples));

	// Copy the data to the destination for unsigned values
	///////////////////////////////////////////////////////////
	imbxUint32 waveformPointer(channel);
	imbxUint32 destinationPointer(0);
	if(sourceDataType == "UB" || sourceDataType == "US")
	{
		for(imbxUint32 copySamples (numSamples); copySamples != 0; --copySamples)
		{
			imbxUint32 unsignedData(waveformData->getUnsignedLong(waveformPointer));
			waveformPointer += numChannels;
			if(bPaddingValueExists && unsignedData == originalPaddingValue)
			{
				destinationHandler->setSignedLong(destinationPointer++, paddingValue);
				continue;
			}
			destinationHandler->setUnsignedLong(destinationPointer++, unsignedData);
		}
		return destinationHandler;
	}

	// Copy the data to the destination for signed values
	///////////////////////////////////////////////////////////
	int highBit(getBitsAllocated() - 1);
	imbxUint32 testBit = ((imbxUint32)1) << highBit;
	imbxUint32 orBits = ((imbxUint32)((imbxInt32)-1)) << highBit;
	for(imbxUint32 copySamples (numSamples); copySamples != 0; --copySamples)
	{
		imbxUint32 unsignedData = waveformData->getUnsignedLong(waveformPointer);
		waveformPointer += numChannels;
		if(bPaddingValueExists && unsignedData == originalPaddingValue)
		{
			destinationHandler->setSignedLong(destinationPointer++, paddingValue);;
			continue;
		}
		if((unsignedData & testBit) != 0)
		{
			unsignedData |= orBits;
		}
		destinationHandler->setSignedLong(destinationPointer++, (imbxInt32)unsignedData);
	}

	// Now decompress uLaw or aLaw
	if(waveformInterpretation == "AB") // 8bits aLaw
	{
		for(imbxUint32 aLawSamples(0); aLawSamples != numSamples; ++aLawSamples)
		{
			imbxUint32 compressed(destinationHandler->getUnsignedLong(aLawSamples));
			if(bPaddingValueExists && compressed == originalPaddingValue)
			{
				continue;
			}
			imbxInt32 decompressed(aLawDecompressTable[compressed]);
			destinationHandler->setSignedLong(aLawSamples, decompressed);
		}
	}

	// Now decompress uLaw or aLaw
	if(waveformInterpretation == "MB") // 8bits aLaw
	{
		for(imbxUint32 uLawSamples(0); uLawSamples != numSamples; ++uLawSamples)
		{
			imbxUint32 compressed(destinationHandler->getUnsignedLong(uLawSamples));
			if(bPaddingValueExists && compressed == originalPaddingValue)
			{
				continue;
			}
			imbxInt32 decompressed(uLawDecompressTable[compressed]);
			destinationHandler->setSignedLong(uLawSamples, decompressed);
		}
	}

	return destinationHandler;

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
//
// Returns the sequence item
//
///////////////////////////////////////////////////////////
ptr<dataSet> waveform::GetWaveformItem()
{
	return m_pDataSet;
}


} // namespace imebra

} // namespace puntoexe

