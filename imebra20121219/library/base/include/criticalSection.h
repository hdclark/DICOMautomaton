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

/*! \file criticalSection.h
    \brief Declaration of the functions that handle the the mutexes.

*/

#if !defined(CImbxCriticalSection_5092DA6B_EF16_4EF9_A1CF_DC8651AA7873__INCLUDED_)
#define CImbxCriticalSection_5092DA6B_EF16_4EF9_A1CF_DC8651AA7873__INCLUDED_

#include "configuration.h"

#ifdef PUNTOEXE_WINDOWS
#include "windows.h"
#else
#include <pthread.h> // this must be the first included file
#endif

#include <exception>
#include <stdexcept>
#include <list>

#include <typeinfo>

namespace puntoexe
{

/// \addtogroup group_baseclasses
///
/// @{

#ifdef PUNTOEXE_WINDOWS
	typedef CRITICAL_SECTION tCriticalSection;
#else
	typedef pthread_mutex_t tCriticalSection;
#endif


/// \brief This class represents a critical section.
///
/// Critical sections can be used by different threads to
///  lock the access to shared resources.
///
/// Use the class lockCriticalSection to safely lock
///  a critical section; lockCriticalSection unlocks the
///  locked critical section in its destructor, therefore
///  it is safe to use it in code that throw exceptions.
///
///////////////////////////////////////////////////////////
class criticalSection
{
public:
	/// \brief Construct and initializes the critical section.
	///
	/// The critical section is initially not locked. It can be
	///  locked by calling lock() or tryLock() or by using the 
	///  class lockCriticalSection (preferred).
	///
	///////////////////////////////////////////////////////////
	criticalSection();

	/// \brief Deallocates the critical section.
	///
	/// A system crash is likely to happens in the case a
	///  critical section is destroyed while it is in a locked 
	///  state. For this reason the class lockCriticalSection
	///  should be used instead of calling directly lock(),
	///  unlock() or tryLock().
	///
	///////////////////////////////////////////////////////////
	virtual ~criticalSection();

	/// \brief Lock the critical section.
	///
	/// If the critical section is already locked by another
	///  thread then the function waits for the critical 
	///  section to become available and then locks it and set
	///  its reference conter to 1.
	///
	/// If the critical section is unlocked then the fuction
	///  locks it and set its reference counter to 1.
	///
	/// If the critical section is already locked by the same
	///  thread that called lock(), then the function simply
	///  increases the reference counter by one.
	///
	/// Use unlock() to unlock a locked critical section.
	/// 
	///////////////////////////////////////////////////////////
	void lock();

	/// \brief Decreases the reference counter of the critical
	///         section and unlocks it if the counter reaches 
	///         0.
	///
	/// Use lock() to lock the critical section.
	///
	///////////////////////////////////////////////////////////
	void unlock();

	/// \brief Try to lock a critical section.
	///
	/// If the critical section is already locked in another
	///  thread then the function returns immediatly the value
	///  false.
	///
	/// If the critical section is not locked, then the 
	///  function locks the critical section, set the reference
	///  counter to 1 and returns true.
	///
	/// If the critical section is locked by the same thread
	///  that called tryLock() then the reference counter is
	///  increased by one and the function returns true.
	///
	/// @return true if the section has been succesfully locked
	///          or false otherwise
	///
	///////////////////////////////////////////////////////////
	bool tryLock();

private:
	tCriticalSection m_criticalSection;
};


/// \brief This class locks a critical section in the
///         constructor and unlocks it in the destructor.
///
/// This helps to correctly release a critical section in
///  case of exceptions or premature exit from a function
///  that uses the critical section.
///
///////////////////////////////////////////////////////////
class lockCriticalSection
{
public:
	/// \brief Creates the object lockCriticalSection and
	///         lock the specified criticalSection.
	///
	/// @param pCriticalSection a pointer to the 
	///                          criticalSection that has to
	///                          be locked
	///
	///////////////////////////////////////////////////////////
	lockCriticalSection(criticalSection* pCriticalSection);
	
	/// \brief Destroy the object lockCriticalSection and
	///         unlock the previously locked criticalSection.
	///
	///////////////////////////////////////////////////////////
	virtual ~lockCriticalSection();
private:
	criticalSection* m_pCriticalSection;
};


/// \internal
/// \brief Represents a list of critical sections.
///
/// It is used by lockMultipleCriticalSections() and
///  unlockMultipleCriticalSections().
///
///////////////////////////////////////////////////////////
typedef std::list<criticalSection*> tCriticalSectionsList;

/// \internal
/// \brief Lock a collection of critical sections.
///
/// The list can contain several pointers to the critical
///  sections that must be locked; the function tries to
///  lock all the critical sections in the list until it
///  succeedes.
///
/// The critical sections locked with 
///  lockMultipleCriticalSections() should be unlocked by
///  unlockMultipleCriticalSections().
///
/// @param pList a list of the critical sections that must
///               be locked
/// @return      a pointer to a list that must be passed
///               to unlockMultipleCriticalSections()
///
///////////////////////////////////////////////////////////
tCriticalSectionsList* lockMultipleCriticalSections(tCriticalSectionsList* pList);

/// \internal
/// \brief Unlock a collection of critical sections
///         locked by lockMultipleCriticalSections().
///
/// @param pList a pointer the list returned by
///               lockMultipleCriticalSections().
///
///////////////////////////////////////////////////////////
void unlockMultipleCriticalSections(tCriticalSectionsList* pList);

/// \internal
/// \brief Exception thrown when a posix mutex is in an
///         error state.
///
///////////////////////////////////////////////////////////
class posixMutexException: public std::runtime_error
{
public:
	posixMutexException(const std::string& message): std::runtime_error(message){}
};

///@}

} // namespace puntoexe

#endif // !defined(CImbxCriticalSection_5092DA6B_EF16_4EF9_A1CF_DC8651AA7873__INCLUDED_)
