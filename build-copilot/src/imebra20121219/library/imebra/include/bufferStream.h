///////////////////////////////////////////////////////////
//  bufferStream.h
//  Declaration of the Class bufferStream
///////////////////////////////////////////////////////////

#if !defined(imebraBufferStream_DE3F98A9_664E_47c0_A29B_B681F9AEB118__INCLUDED_)
#define imebraBufferStream_DE3F98A9_664E_47c0_A29B_B681F9AEB118__INCLUDED_

#include "../../base/include/memoryStream.h"
#include "dataHandlerNumeric.h"

namespace puntoexe
{

namespace imebra
{

/// \addtogroup group_dataset
///
/// @{

class bufferStream: public memoryStream
{
public:
	bufferStream(ptr<handlers::dataHandlerRaw> pDataHandler):
	  memoryStream(pDataHandler->getMemory()),
	  m_pDataHandler(pDataHandler){}
protected:

	ptr<handlers::dataHandlerRaw> m_pDataHandler;
};

/// @}

} // namespace imebra

} // namespace puntoexe

#endif // !defined(imebraBufferStream_DE3F98A9_664E_47c0_A29B_B681F9AEB118__INCLUDED_)
