/*

Imebra 2011 build 2011-09-18_22-24-41

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

/*! \file waveform.h
    \brief Declaration of the class waveform.

*/

#if !defined(imebraWaveform_A807A3CA_FA04_44f4_85D2_C7AA2FE103C4__INCLUDED_)
#define imebraWaveform_A807A3CA_FA04_44f4_85D2_C7AA2FE103C4__INCLUDED_

#include "../../base/include/baseObject.h"


///////////////////////////////////////////////////////////
//
// Everything is in the namespace puntoexe::imebra
//
///////////////////////////////////////////////////////////
namespace puntoexe
{

namespace imebra
{

class dataSet;
namespace handlers
{
	class dataHandler;
}

/// \addtogroup group_waveform Waveforms
///
/// @{

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
/// \brief Represents a single waveform of a dicom dataset. 
///
/// Waveforms are embedded into the dicom structures
///  (represented by the dataSet class), stored in sequence
///  items (one waveform per item).
///
/// Use dataSet::getWaveform() to retrieve a waveform from
///  a dataSet.
///
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
class waveform : public baseObject
{
public:
	/// \brief Constructor. Initializes the class and connects
	///         it to the sequence item containing the waveform
	///         data.
	///
	/// @param pDataSet   the sequence item containing the
	///                    waveform. 
	///                   Use dataSet::getSequenceItem() to
	///                    retrieve the sequence item 
	///                    containing the waveform or
	///                    construct the class with
	///                    dataSet::getWaveform()
	///
	///////////////////////////////////////////////////////////
	waveform(ptr<dataSet> pDataSet);

	/// \brief Retrieve the number of channels (tag 003A,0005).
	///
	/// @return the number of channels
	///
	///////////////////////////////////////////////////////////
	imbxUint32 getChannels();

	/// \brief Retrieve the number of samples per channel
	///         (tag 003A,0010).
	///
	/// @return the number of samples per channel
	///
	///////////////////////////////////////////////////////////
	imbxUint32 getSamples();

	/// \brief Retrieve the number of bit stored 
	///         (tag 003A,021A).
	///
	/// @return the number of bits stored
	///
	///////////////////////////////////////////////////////////
	imbxUint32 getBitsStored();

	/// \brief Retrieve the number of bit allocated
	///         (tag 5400,1004).
	///
	/// @return the number of bits allocated
	///
	///////////////////////////////////////////////////////////
	imbxUint32 getBitsAllocated();

	/// \brief Return the data interpretation string 
	///         (tag 5400,1006).
	///
	/// @return the interpretation string. Possible values are:
	/// - "SB": signed 8 bit linear
	/// - "UB": unsigned 8 bit linear
	/// - "MB": 8 bit u-law 
	/// - "AB": 8 bit a-law
	/// - "SS": signed 16 bit
	/// - "US": unsigned 16 bit
	///
	///////////////////////////////////////////////////////////
	std::string getInterpretation();

	/// \brief Retrieve the decompressed waveform data.
	///
	/// Retrieve the requested channel's data and decompress
	///  it into signed long values. 8 bits uLaw and aLaw data 
	///  are decompressed into normalized 16 bits values.
	///
	/// This function takes into account the value in the 
	///  interpretation tag and returns an handler to ready to
	///  use data.
	///
	/// @param channel      the channel for which the data is
	///                      required
	/// @param paddingValue the value that the function must
	///                      write in the returned data in
	///                      place of the original padding
	///                      value. Specify a number
	///                      outside the range -32768..65535
	/// @return a data handler attached to ready to use data.
	///        Use handlers::dataHandler->GetSignedLong() or 
	///        handlers::dataHandler->GetSignedLongIncPointer()
	///        to retrieve the data
	///
	///////////////////////////////////////////////////////////
	ptr<handlers::dataHandler> getIntegerData(imbxUint32 channel, imbxInt32 paddingValue = 0x7fffffff);

	/// \brief Return the sequence item used by the waveform.
	///
	/// @return the sequence item used by the waveform
	///
	///////////////////////////////////////////////////////////
	ptr<dataSet> GetWaveformItem();
	
private:
	ptr<dataSet> m_pDataSet;
};

/// @}

} // namespace imebra

} // namespace puntoexe

#endif // !defined(imebraWaveform_A807A3CA_FA04_44f4_85D2_C7AA2FE103C4__INCLUDED_)
