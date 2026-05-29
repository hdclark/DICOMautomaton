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

/*! \file criticalSection.cpp
    \brief Implementation of the critical sections.

*/

#include "../include/criticalSection.h"
#include "../include/exception.h"
#include <map>
#include <memory>

#ifdef PUNTOEXE_WINDOWS
#include <process.h>
#endif

#ifdef PUNTOEXE_POSIX
#include <sched.h>
#include <cerrno>
#endif

namespace puntoexe
{

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
//
// Global functions
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
// Lock several critical sections
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
tCriticalSectionsList* lockMultipleCriticalSections(tCriticalSectionsList* pList)
{
	PUNTOEXE_FUNCTION_START(L"lockMultipleCriticalSections");

	// Build a map of the involved critical sections.
	// The map is ordered by the pointer's value
	///////////////////////////////////////////////////////////
	typedef std::map<criticalSection*, bool> tCriticalSectionsMap;
	tCriticalSectionsMap CSmap;
	for(auto & copyCS : *pList)
	{
		CSmap[copyCS] = true;
	}

	// Build a list that lists all the locked critical sections
	///////////////////////////////////////////////////////////
	std::unique_ptr<tCriticalSectionsList> pLockedList(new tCriticalSectionsList);

	// Use the normal lockCriticalSection if the list contains
	//  only one critical section
	///////////////////////////////////////////////////////////
	if(CSmap.size() == 1)
	{
		CSmap.begin()->first->lock();
		pLockedList->push_back(CSmap.begin()->first);
		return pLockedList.release();
	}

	// Try to lock all the critical sections. Give way to
	//  other threads if one lock fails, then retry.
	///////////////////////////////////////////////////////////
	for(bool bOK = false; !bOK; /* empty */)
	{
		pLockedList->clear();
		bOK = true;
		for(auto & lockCS : CSmap)
		{
			if(lockCS.first->tryLock())
			{
				pLockedList->push_back(lockCS.first);
				continue;
			}

			bOK = false;
			unlockMultipleCriticalSections(pLockedList.get());

#ifdef PUNTOEXE_WINDOWS // WINDOWS

#if(_WIN32_WINNT>=0x0400)
			SwitchToThread();
#else
			Sleep(0);
#endif

#else // POSIX
			sched_yield();
#endif
			break;
		}
	}

	return pLockedList.release();

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Unlock several critical sections
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void unlockMultipleCriticalSections(tCriticalSectionsList* pList)
{
	for(tCriticalSectionsList::reverse_iterator unlockCS = pList->rbegin(); unlockCS != pList->rend(); ++unlockCS)
	{
		(*unlockCS)->unlock();
	}
}



///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
//
// lockCriticalSection
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
// Constructor: Lock a critical section
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
lockCriticalSection::lockCriticalSection(criticalSection* pCriticalSection): m_pCriticalSection(pCriticalSection)
{
	m_pCriticalSection->lock();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Destructor: unlock a critical section
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
lockCriticalSection::~lockCriticalSection()
{
	m_pCriticalSection->unlock();
}



///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
//
// criticalSection
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
// Initialize a critical section
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
criticalSection::criticalSection()
{
#ifdef PUNTOEXE_WINDOWS
	InitializeCriticalSection(&m_criticalSection);
#else
	pthread_mutexattr_t criticalSectionAttribute;

	pthread_mutexattr_init(&criticalSectionAttribute );
	pthread_mutexattr_settype(&criticalSectionAttribute, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&m_criticalSection, &criticalSectionAttribute );
	pthread_mutexattr_destroy(&criticalSectionAttribute );
#endif
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Uninitialize a critical section
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
criticalSection::~criticalSection()
{
#ifdef PUNTOEXE_WINDOWS
	DeleteCriticalSection(&m_criticalSection);
#else
	pthread_mutex_destroy(&m_criticalSection);
#endif
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Lock a critical section
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void criticalSection::lock()
{
#ifdef PUNTOEXE_WINDOWS
	EnterCriticalSection(&m_criticalSection);
#else
	pthread_mutex_lock(&m_criticalSection);
#endif
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Unlock a critical section
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void criticalSection::unlock()
{
#ifdef PUNTOEXE_WINDOWS
	LeaveCriticalSection(&m_criticalSection);
#else
	pthread_mutex_unlock(&m_criticalSection);
#endif
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Try to lock a critical section
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
bool criticalSection::tryLock()
{

#ifdef PUNTOEXE_WINDOWS // WINDOWS
	return TryEnterCriticalSection(&m_criticalSection) != 0;

#else // POSIX

	PUNTOEXE_FUNCTION_START(L"criticalSection::tryLock");

	int tryLockResult = pthread_mutex_trylock(&m_criticalSection);
	if(tryLockResult == 0)
	{
		return true;
	}
	if(tryLockResult == EBUSY)
	{
		return false;
	}
	PUNTOEXE_THROW(posixMutexException, "A mutex is in an error state");

	PUNTOEXE_FUNCTION_END();
#endif

}

} // namespace puntoexe
