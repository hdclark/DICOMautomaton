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

/*! \file thread.cpp
    \brief Implementation of the thread-related functions.

*/

#include "../include/thread.h"
#include "../include/exception.h"

#ifdef PUNTOEXE_WINDOWS
#include <process.h>
#endif

#ifdef PUNTOEXE_POSIX
#include <sched.h>
#include <cerrno>
#endif

namespace puntoexe
{

#ifdef PUNTOEXE_POSIX
static thread::forceKeyConstruction forceKey;
#endif

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
//
// thread
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
thread::thread():
    m_bThreadHandleValid(false),
        m_bTerminate(false),
        m_bIsRunning(false)
{
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Stop the thread before deallocating it
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
bool thread::preDelete()
{
	PUNTOEXE_FUNCTION_START(L"thread::preDelete");

	// If the thread's handle is not valid (not started yet),
	//  then return immediatly
	///////////////////////////////////////////////////////////
	{
		lockCriticalSection lockThreadHandle(&m_lockThreadHandle);
		if( !m_bThreadHandleValid )
		{
			return true;
		}
	}

	// Send a termination request to the thread's function
	///////////////////////////////////////////////////////////
	terminate();

#ifdef PUNTOEXE_WINDOWS

	// Wait for the thread termination, then close and
	//  invalidate the thread
	///////////////////////////////////////////////////////////
	::WaitForSingleObject(m_threadHandle, INFINITE);

	lockCriticalSection lockThreadHandle(&m_lockThreadHandle);
	::CloseHandle(m_threadHandle);
	m_bThreadHandleValid = false;

#else

	// Join the thread, then invalidate the handle
	///////////////////////////////////////////////////////////
	pthread_join(m_threadHandle, nullptr);

	lockCriticalSection lockThreadHandle(&m_lockThreadHandle);
	m_bThreadHandleValid = false;

#endif

	return true;

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Function that is launched in a separate thread
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
#ifdef PUNTOEXE_WINDOWS

unsigned int __stdcall thread::privateThreadFunction(void* pParameter)

#else

void* thread::privateThreadFunction(void* pParameter)

#endif
{
	try
	{
		PUNTOEXE_FUNCTION_START(L"thread::privateThreadFunction");

		// Get the thread object that launched the function
		///////////////////////////////////////////////////////////
		thread* pThread = (thread*)pParameter;

		// Enable the "isRunning" flag
		///////////////////////////////////////////////////////////
		{
			lockCriticalSection lockRunningFlag(&(pThread->m_lockRunningFlag));
			pThread->m_bIsRunning = true;
		}

		// Call the virtual thread function
		///////////////////////////////////////////////////////////
		pThread->threadFunction();

		// Disable the "isRunning" flag
		///////////////////////////////////////////////////////////
		{
			lockCriticalSection lockRunningFlag(&(pThread->m_lockRunningFlag));
			pThread->m_bIsRunning = false;
		}

		PUNTOEXE_FUNCTION_END();
	}
	catch(...)
	{
		exceptionsManager::getMessage();
	}

#ifdef PUNTOEXE_WINDOWS
    return 0u;
#else
	return nullptr;
#endif

}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Start the thread's function
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void thread::start()
{
	PUNTOEXE_FUNCTION_START(L"thread::start");

	lockCriticalSection lockThreadHandle(&m_lockThreadHandle);

	// Throw an exception if the thread is already running
	///////////////////////////////////////////////////////////
	if(m_bThreadHandleValid)
	{
		PUNTOEXE_THROW(threadExceptionAlreadyRunning, "Thread already running");
	}

#ifdef PUNTOEXE_WINDOWS
	// Start the thread
	///////////////////////////////////////////////////////////
	unsigned dummy;
	m_threadHandle = (HANDLE)::_beginthreadex(0, 0, thread::privateThreadFunction, this, 0, &dummy);
	if(m_threadHandle == 0)
	{
		PUNTOEXE_THROW(threadExceptionFailedToLaunch, "Failed to launch the thread");
	}
#else
	pthread_attr_t threadAttributes;
	pthread_attr_init(&threadAttributes);
	pthread_attr_setdetachstate(&threadAttributes, PTHREAD_CREATE_JOINABLE);
	int errorCode = pthread_create(&m_threadHandle, &threadAttributes, thread::privateThreadFunction, this);
	pthread_attr_destroy(&threadAttributes);

	if(errorCode != 0)
	{
		PUNTOEXE_THROW(threadExceptionFailedToLaunch, "Failed to launch the thread");
	}
#endif

	m_bThreadHandleValid = true;

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Send a termination request to the running thread
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void thread::terminate()
{
	lockCriticalSection lockTerminateFlag(&m_lockTerminateFlag);
	m_bTerminate = true;
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Called by the thread's function to know if it has to
//  terminate
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
bool thread::shouldTerminate()
{
	lockCriticalSection lockTerminateFlag(&m_lockTerminateFlag);
	return m_bTerminate;
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Return true if the thread's function is running
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
bool thread::isRunning()
{
	lockCriticalSection lockRunningFlag(&m_lockRunningFlag);
	return m_bIsRunning;
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Return an indentifier for the current thread
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
thread::tThreadId thread::getThreadId()
{
#ifdef PUNTOEXE_WINDOWS
	return (tThreadId)::GetCurrentThreadId();
#else
	pthread_key_t* pKey = getSharedKey();
	void* id = pthread_getspecific(*pKey);
	if(id == nullptr)
	{
		id = getNextId();
		pthread_setspecific(*pKey, id);
	}
	return id;
#endif
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Switch to another thread
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void thread::yield()
{
#ifdef PUNTOEXE_WINDOWS // WINDOWS

#if(_WIN32_WINNT>=0x0400)
			SwitchToThread();
#else
			Sleep(0);
#endif

#else // POSIX
			sched_yield();
#endif
}


#ifdef PUNTOEXE_POSIX

pthread_key_t* thread::getSharedKey()
{
	static sharedKey m_key;
	return &(m_key.m_key);
}

void* thread::getNextId()
{
    // Modified 20230420 to remove undefined behaviour associated with incrementing a nullptr. -hc
	static char m_nextId = (char)0;
	static criticalSection m_criticalSection;

	lockCriticalSection lockHere(&m_criticalSection);
	if(++m_nextId == (char)0)
	{
		m_nextId = (char)100; // Overflow. Assume that the first created threads live longer
	}
	return (void*)&m_nextId;
}

#endif

} // namespace puntoexe
