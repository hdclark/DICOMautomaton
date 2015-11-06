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

/*! \file memory.cpp
    \brief Implementation of the memory manager and the memory class.

*/

#include "../include/memory.h"

namespace puntoexe
{

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
//
// memory
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
memory::memory():
	m_pMemoryBuffer(new stringUint8)
{
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Detach a managed string
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void memory::transfer(const ptr<memory>& transferFrom)
{
	m_pMemoryBuffer.reset(transferFrom->m_pMemoryBuffer.release());
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Copy the content from another memory object
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void memory::copyFrom(const ptr<memory>& sourceMemory)
{
	if(m_pMemoryBuffer.get() == 0)
	{
		m_pMemoryBuffer.reset(new stringUint8);
	}
	m_pMemoryBuffer->assign(sourceMemory->data(), sourceMemory->size());
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Clear the managed string
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void memory::clear()
{
	if(m_pMemoryBuffer.get() != 0)
	{
		m_pMemoryBuffer->clear();
	}
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Resize the memory buffer
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void memory::resize(imbxUint32 newSize)
{
	if(m_pMemoryBuffer.get() == 0)
	{
		m_pMemoryBuffer.reset(new stringUint8);
	}
	m_pMemoryBuffer->resize(newSize, 0);
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Reserve memory
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void memory::reserve(imbxUint32 reserveSize)
{
	if(m_pMemoryBuffer.get() == 0)
	{
		m_pMemoryBuffer.reset(new stringUint8);
	}
	m_pMemoryBuffer->reserve(reserveSize);
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Return the size of the managed string
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
imbxUint32 memory::size()
{
	if(m_pMemoryBuffer.get() == 0)
	{
		return 0;
	}
	return (imbxUint32)(m_pMemoryBuffer->size());
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Return a pointer to the data
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
imbxUint8* memory::data()
{
	if(m_pMemoryBuffer.get() == 0 || m_pMemoryBuffer->empty())
	{
		return 0;
	}
	return &( ( (*m_pMemoryBuffer.get()))[0]);
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Return true if the managed string is empty
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
bool memory::empty()
{
	return m_pMemoryBuffer.get() == 0 || m_pMemoryBuffer->empty();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Copy the content of a buffer in the managed string
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void memory::assign(const imbxUint8* pSource, const imbxUint32 sourceLength)
{
	if(m_pMemoryBuffer.get() == 0)
	{
		m_pMemoryBuffer.reset(new stringUint8);
	}
	m_pMemoryBuffer->assign(pSource, sourceLength);
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Check if the memory can be reused
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
bool memory::preDelete()
{
	return !(memoryPool::getMemoryPool()->reuseMemory(this));
}



///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
//
// memoryPool
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
// Destructor
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
memoryPool::~memoryPool()
{
	while(m_actualSize != 0)
	{
		m_actualSize -= m_memorySize[m_firstUsedCell];
		delete m_memoryPointer[m_firstUsedCell++];
		if(m_firstUsedCell == IMEBRA_MEMORY_POOL_SLOTS)
		{
			m_firstUsedCell = 0;
		}
	}
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Save a memory object to reuse it
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
bool memoryPool::reuseMemory(memory* pMemoryToReuse)
{
	// Check for the memory size. Don't reuse it if the memory
	//  doesn't match the requested parameters
	///////////////////////////////////////////////////////////
	imbxUint32 memorySize = pMemoryToReuse->size();
	if(memorySize == 0 || memorySize < IMEBRA_MEMORY_POOL_MIN_SIZE || memorySize > IMEBRA_MEMORY_POOL_MAX_SIZE)
	{
		return false;
	}

	// Ok to reuse
	///////////////////////////////////////////////////////////
	lockCriticalSection lockThis(&m_criticalSection);

	// Store the memory object in the pool
	///////////////////////////////////////////////////////////
	m_memorySize[m_firstFreeCell] = memorySize;
	m_memoryPointer[m_firstFreeCell] = pMemoryToReuse;
	m_actualSize += memorySize;
	if(++m_firstFreeCell >= IMEBRA_MEMORY_POOL_SLOTS)
	{
		m_firstFreeCell = 0;
	}

	// Remove old unused memory objects
	///////////////////////////////////////////////////////////
	if(m_firstFreeCell == m_firstUsedCell)
	{
		m_actualSize -= m_memorySize[m_firstUsedCell];
		delete m_memoryPointer[m_firstUsedCell];
		if(++m_firstUsedCell >= IMEBRA_MEMORY_POOL_SLOTS)
		{
			m_firstUsedCell = 0;
		}
	}

	// Remove old unused memory objects if the total unused
	//  memory is bigger than the specified parameters
	///////////////////////////////////////////////////////////
	while(m_actualSize != 0 && m_actualSize > IMEBRA_MEMORY_POOL_MAX_SIZE)
	{
		m_actualSize -= m_memorySize[m_firstUsedCell];
		delete m_memoryPointer[m_firstUsedCell];
		if(++m_firstUsedCell >= IMEBRA_MEMORY_POOL_SLOTS)
		{
			m_firstUsedCell = 0;
		}
	}

	return true;

}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Discard the currently unused memory
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void memoryPool::flush()
{
	lockCriticalSection lockThis(&m_criticalSection);

	while(m_firstUsedCell != m_firstFreeCell)
	{
		delete m_memoryPointer[m_firstUsedCell];
		m_actualSize -= m_memorySize[m_firstUsedCell];
		if(++m_firstUsedCell >= IMEBRA_MEMORY_POOL_SLOTS)
		{
			m_firstUsedCell = 0;
		}
	}
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Get a pointer to the unique instance of the memoryPool
//  class
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
memoryPool* memoryPool::getMemoryPool()
{
	static memoryPool m_staticMemoryPool;

	return &m_staticMemoryPool;
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Look for a memory object to reuse or allocate a new one
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
memory* memoryPool::getMemory(imbxUint32 requestedSize)
{
	lockCriticalSection lockThis(&m_criticalSection);

	// Look for an object to reuse
	///////////////////////////////////////////////////////////
	for(imbxUint32 findCell = m_firstUsedCell; findCell != m_firstFreeCell;)
	{
		if(m_memorySize[findCell] != requestedSize)
		{
			if(++findCell >= IMEBRA_MEMORY_POOL_SLOTS)
			{
				findCell = 0;
			}
			continue;
		}

		// Memory found
		///////////////////////////////////////////////////////////
		memory* pMemory = m_memoryPointer[findCell];
		m_actualSize -= m_memorySize[findCell];
		if(findCell == m_firstUsedCell)
		{
			if(++m_firstUsedCell >= IMEBRA_MEMORY_POOL_SLOTS)
			{
				m_firstUsedCell = 0;
			}
			return pMemory;
		}

		imbxUint32 lastUsedCell = m_firstFreeCell == 0 ? (IMEBRA_MEMORY_POOL_SLOTS - 1) : (m_firstFreeCell - 1);
		if(findCell == lastUsedCell)
		{
			m_firstFreeCell = lastUsedCell;
			return pMemory;
		}

		m_memorySize[findCell] = m_memorySize[m_firstUsedCell];
		m_memoryPointer[findCell] = m_memoryPointer[m_firstUsedCell];
		if(++m_firstUsedCell >= IMEBRA_MEMORY_POOL_SLOTS)
		{
			m_firstUsedCell = 0;
		}
		return pMemory;
	}

	memory* pMemory(new memory);
	pMemory->resize(requestedSize);
	return pMemory;
}

} // namespace puntoexe
