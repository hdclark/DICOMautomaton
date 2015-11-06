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

/*! \file codecFactory.h
    \brief Declaration of the class used to retrieve the codec able to
	        handle the requested transfer syntax.

*/

#if !defined(imebraCodecFactory_82307D4A_6490_4202_BF86_93399D32721E__INCLUDED_)
#define imebraCodecFactory_82307D4A_6490_4202_BF86_93399D32721E__INCLUDED_

#include "../../base/include/baseObject.h"
#include "dataSet.h"



#include <list>


///////////////////////////////////////////////////////////
//
// Everything is in the namespace puntoexe::imebra
//
///////////////////////////////////////////////////////////
namespace puntoexe
{

	class streamReader;

namespace imebra
{

// Classes used in the declaration
class dataSet;

namespace codecs
{

/// \addtogroup group_codecs
///
/// @{

class codec;

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
/// \brief This class maintains a list of the available
///        codecs.
///        
/// It is used to retrieve the right codec when the 
///  transfer syntax is known, or to automatically select
///  the right codec that can parse the specified stream
///  of data.
///
/// An instance of this class is automatically allocated
///  by the library and can be retrieved using the
///  static function codecFactory::getCodecFactory().
///
///
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
class codecFactory: public baseObject
{
public:
	/// \brief Register a new codec.
	///
	/// This function is called by the framework during the
	///  library's startup, in order to register all the imebra
	///  codecs.
	/// The codecs distributed with the imebra library are
	///  automatically registered.
	///
	/// @param pCodec a pointer to the codec to register
	///
	///////////////////////////////////////////////////////////
	void registerCodec(ptr<codec> pCodec);

	/// \brief Get a pointer to the codec that can handle
	///        the requested transfer syntax.
	///
	/// All the registered codecs are queried until the codec
	///  that can handle the specified transfer syntax replies.
	///
	/// A new instance of the requested codec is allocated and
	///  its pointer is returned.
	///
	/// @param transferSyntax a string with the requested
	///         transfer syntax.
	/// @return a pointer to a imebra codec that can handle the
	///        requested transfer syntax, or 0 if the function
	///         didn't find any valid codec.
	///        The returned pointer references a new instance
	///         of the codec, not the instance registered using
	///         registerCodec()
	///
	///////////////////////////////////////////////////////////
	static ptr<codec> getCodec(std::wstring transferSyntax);

	/// \brief Retrieve the only reference to the codecFactory
	///         instance.
	///
	/// An instance of codecFactory class is statically
	///  allocated by the imebra framework.
	///
	/// The applications should use only the statically 
	///  allocated instance of codecFactory.
	///
	/// @return a pointer to the only instance of the
	///          codecFactory class.
	///
	///////////////////////////////////////////////////////////
	static ptr<codecFactory> getCodecFactory();

	/// \brief Build a dataSet structure from the specified
	///         stream of data.
	///
	/// The function selects automatically the codec that can
	///  read the specified stream.
	///
	/// @param pStream the stream that contain the data to be
	///                 parsed
	/// @param maxSizeBufferLoad if a loaded buffer exceedes
	///                 the size in the parameter then it is
	///                 not loaded immediatly but it will be
	///                 loaded on demand. Some codecs may 
	///                 ignore this parameter.
	///                Set to 0xffffffff to load all the 
	///                 buffers immediatly
	/// @return a pointer to the dataSet containing the parsed
	///          data
	///
	///////////////////////////////////////////////////////////
	ptr<dataSet> load(ptr<streamReader> pStream, imbxUint32 maxSizeBufferLoad = 0xffffffff);

protected:
	// The list of the registered codecs
	///////////////////////////////////////////////////////////
	std::list<ptr<codec> > m_codecsList;


public:
	// Force the creation of the codec factory before main()
	//  starts
	///////////////////////////////////////////////////////////
	class forceCodecFactoryCreation
	{
	public:
		forceCodecFactoryCreation()
		{
			codecFactory::getCodecFactory();
		}
	};
};

/// @}

} // namespace codecs

} // namespace imebra

} // namespace puntoexe


#endif // !defined(imebraCodecFactory_82307D4A_6490_4202_BF86_93399D32721E__INCLUDED_)
