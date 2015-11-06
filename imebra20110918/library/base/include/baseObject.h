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

/*! \file baseObject.h
    \brief Declaration of the base classes used by the puntoexe library.

*/

#if !defined(CImbxBaseObject_F1BAF067_21DE_466b_AEA1_6CC4F006FAFA__INCLUDED_)
#define CImbxBaseObject_F1BAF067_21DE_466b_AEA1_6CC4F006FAFA__INCLUDED_

#include "configuration.h"
#include "criticalSection.h"

#include <string>
#include <memory>

///////////////////////////////////////////////////////////
/// \namespace puntoexe
/// \brief The base services and classes are defined
///         in this namespace.
///
/// Other namespace in the library are usually children
///  of this namespace.
///
///////////////////////////////////////////////////////////
namespace puntoexe
{

/// \addtogroup group_baseclasses Base classes
///
/// The base classes supply basic services like streams,
///  memory management and exceptions management.
///
/// They also implement the smart pointer puntoexe::ptr and
///  the base class puntoexe::baseObject
///
///////////////////////////////////////////////////////////
/// @{

class baseObject;

/// \internal
/// \brief This is the base class for the template class
///         ptr.
///
///////////////////////////////////////////////////////////
class basePtr
{
protected:
	// Default constructor
	//
	// Set the internal pointer to null.
	//
	///////////////////////////////////////////////////////////
	basePtr();

	// Construct the ptr object and keeps track of
	//         the object referenced by the pointer pObject.
	//
	// @param pObject a pointer to the allocated object.
	//        The allocated object will be automatically
	//         released by the class' destructor.\n
	//        The object must be allocated by the C++ new
	//         statement.\n
	//        The object's reference counter is increased by
	//         this function.
	//
	///////////////////////////////////////////////////////////
	basePtr(baseObject* pObject);

public:
	// The destructor.
	//
	// The reference to the tracked object is decreased
	//  automatically.
	//
	///////////////////////////////////////////////////////////
	virtual ~basePtr();

	// Release the tracked object and reset the
	//         internal pointer.
	//
	///////////////////////////////////////////////////////////
	void release();

protected:
	void addRef();

	// A pointer to the tracked object
	///////////////////////////////////////////////////////////
	baseObject* object;
};


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
/// \brief This class represents a shared pointer which
///         keeps track of the allocated objects that
///         derive from the class baseObject.
///
/// Most of the classes in this library derive from
///  baseObject.
///
/// When a ptr object is initialized with a pointer
///  to a baseObject derived object then it increases by 1
///  the reference counter of baseObject.\n
/// When ptr goes out of scope (destroyed) then the
///  reference counter of baseObject is decreased by 1,
///  and eventually baseObject is destroyed when its
///  reference counter reaches 0.
///
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
template<class objectType>
class ptr: public basePtr
{
public:
	/// \brief Default constructor.
	///
	/// Set the internal pointer to null.
	///
	///////////////////////////////////////////////////////////
	ptr(): basePtr(0){}

	/// \brief Initializes the ptr object with a reference to
	///         a baseObject.
	///
	/// The baseObject's reference counter is increased by
	///  one: this will keep the baseObject allocate at least
	///  until ptr stays allocated, release() is called
	///  or a new baseObject is associated to ptr.
	///
	/// @param pObject a pointer to the allocated object.
	///        The allocated object will be automatically
	///         released by the class' destructor.\n
	///        The object must be allocated by the C++ new
	///         statement.\n
	///        The object's reference counter is increased by
	///         this function.
	///
	///////////////////////////////////////////////////////////
	ptr(objectType* pObject): basePtr(pObject){}

	/// \brief Copy constructor.
	///
	/// The object tracked by another ptr is copied into the
	///  current ptr and the reference counter is increased.
	///
	/// @param ptrSource  the source ptr object that must be
	///                    copied into the current \ref ptr.
	///
	///////////////////////////////////////////////////////////
	ptr (ptr<objectType> const &ptrSource):
			basePtr(ptrSource.object)
	{
	}

	/// \brief Copy the object tracked by another
	///         \ref ptr.
	///
	/// @param ptrSource the ptr object that is
	///         currently tracking the object to be tracked
	///         by this ptr object.
	///        The reference counter of the new object is
	///         increased.
	///        The object previously tracked by this ptr is
	///         released.
	/// @return a reference to the ptr object.
	///
	///////////////////////////////////////////////////////////
	ptr<objectType>& operator=(const ptr<objectType>& ptrSource)
	{
		if(ptrSource.object != object)
		{
			release();
			object = ptrSource.object;
			addRef();
		}
		return *this;
	}

	/// \brief Compare the pointer to the tracked object with
	///         the pointer specified in the parameter.
	///
	/// @param ptrCompare the pointer to be compared with the
	///                    pointer to the tracked object
	/// @return true if the ptrCompare pointer is equal to
	///                    the pointer of the tracked object
	///
	///////////////////////////////////////////////////////////
	inline bool operator == (const objectType* ptrCompare) const
	{
		return object == ptrCompare;
	}

	/// \brief Compare the pointer to the tracked object with
	///         the pointer tracked by another ptr.
	///
	/// @param ptrCompare the ptr tracking the object to be
	///                    compared
	/// @return true if the ptrCompare pointer is equal to
	///                    the pointer of the tracked object
	///
	///////////////////////////////////////////////////////////
	inline bool operator == (const ptr<objectType>& ptrCompare) const
	{
		return object == ptrCompare.object;
	}

	inline bool operator != (const objectType* ptrCompare) const
	{
		return object != ptrCompare;
	}

	inline bool operator != (const ptr<objectType>& ptrCompare) const
	{
		return object != ptrCompare.object;
	}

	/// \brief Return the pointer to the tracked object.
	///
	/// @return the pointer to the tracked object
	///
	///////////////////////////////////////////////////////////
	inline objectType* operator ->() const
	{
		return static_cast<objectType*>(object);
	}

	/// \brief Return the pointer to the tracked object.
	///
	/// @return the pointer to the tracked object
	///
	///////////////////////////////////////////////////////////
	inline objectType* get() const
	{
		return static_cast<objectType*>(object);
	}

	/// \brief Return the pointer to the tracked object,
	///         type casted in the specified class pointer.
	///
	/// The cast is static, so its validity is checked at
	///  compile time.
	///
	/// @return the pointer to the tracked object
	///
	///////////////////////////////////////////////////////////
	template <class destType>
		inline operator destType*() const
	{
		return static_cast<destType*>(object);
	}

	/// \brief Return a new ptr object tracking the object
	///         tracked by this ptr. A type cast is performed
	///         if necessary.
	///
	/// The type cast is static, so its validity is checked at
	///  compile time.
	///
	/// @return the pointer to the tracked object
	///
	///////////////////////////////////////////////////////////
	template <class destType>
		inline operator ptr<destType>() const
	{
		return ptr<destType>(static_cast<destType*>(object));
	}
};

class lockObject;
class lockMultipleObjects;
class exceptionsManager;

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
/// \brief This is the base class of the library.
///        Almost all the classes in the library are
///         derived from this one.
///
/// The class supplies a basic mechanism for multithrading
///  locking and a reference counter, used by the helper
///  class \ref ptr.
///
/// At construction time the reference counter is set to
///  0 and must be incremented with addRef(); the helper
///  class ptr does this automatically.\n
///
/// Use release() to decrement the reference counter.\n
/// The object automatically destroy itself when the
///  reference counter reaches 0, but only if the function
///  preDelete() returns true.
///
/// An object of type baseObject should never be deleted
///  using the C++ statement "delete".
///
/// In order to manage the reference counter easily
///  your application should use the objects derived from
///  baseObject through the helper class \ref ptr.
///
/// The locking mechanism is exposed through the class
///  lockObject.
///
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
class baseObject
{
	friend class lockObject;
	friend class lockMultipleObjects;
	friend class basePtr;
	friend class exceptionsManager;

public:
    /// \internal
	/// \brief Creates the baseObject object. The reference
	///         counter is set to 0.
	///
	/// The object should be unallocated by the release()
	///  function, not by deleting it using the C++ instruction
	///  delete.
	///
	/// In order to avoid mistakes and memory leaks, objects
	///  derived from baseObject should be assigned to a
	///  \ref ptr object.
	///
	///////////////////////////////////////////////////////////
	baseObject();

	/// \brief Creates the baseObject object and set an
	///         external object to be used for the lock.
	///        The reference counter is set to 0.
	///
	/// The object should be unallocated by the release()
	///  function, not by deleting it using the C++ instruction
	///  delete.
	///
	/// In order to avoid mistakes and memory leaks, objects
	///  derived from baseObject should be assigned to a
	///  \ref ptr object.
	///
	/// @param externalLock a pointer to the object to use
	///                      to lock this one
	///
	///////////////////////////////////////////////////////////
	baseObject(const ptr<baseObject>& externalLock);

	/// \brief Returns one if the reference count is set to 1.
	///
	/// @return true if the reference counter is set to 1
	///
	///////////////////////////////////////////////////////////
	bool isReferencedOnce();

protected:
	// The destructor is protected because the object must
	//  be deleted using release()
	///////////////////////////////////////////////////////////
	virtual ~baseObject();

private:
	/// \brief Increase the object's references counter.
	///
	/// This function is called by the smart pointer \ref ptr.
	///
	/// Each call to addRef() must be matched by a call to
	///  release().
	///
	///////////////////////////////////////////////////////////
	void addRef();

	/// \brief Decreases the object's references counter.
	///        When the references counter reaches zero, then
	///         the object is deleted.
	///
	/// This function is called by the smart pointer \ref ptr.
	///
	/// See also addRef().
	///
	///////////////////////////////////////////////////////////
	void release();

	/// \internal
	/// \brief This function is called by release() before the
	///         deletion of the object.
	///
	/// The base version of this function just returns true.
	/// When the function returns true then the function
	///  release() deletes the object normally when its
	///  references counter reaches 0, otherwise the object
	///  will NOT be deleted when the references counter
	///  reaches 0.
	///
	/// @return true if the object should be deleted when
	///          the references counter reaches 0, false if
	///          the object should NOT be deleted when the
	///          references counter reaches 0.
	///
	///////////////////////////////////////////////////////////
	virtual bool preDelete();

	/// \brief References counter.
	///
	/// When the object is created it is set to 0, then the
	///  smart pointer that controls the object increases it
	///  by calling addRef().
	///
	/// When the reference counter reaches zero then the object
	///  is deleted.
	///
	///////////////////////////////////////////////////////////
	volatile long m_lockCounter;
        criticalSection m_counterCriticalSection;

	// Lock/Unlock the objects.
	///////////////////////////////////////////////////////////
	void lock();
	void unlock();

	bool m_bValid;

        class CObjectCriticalSection
        {
        public:
            CObjectCriticalSection(): m_counter(0){}
            criticalSection m_criticalSection;
            void addRef(){++m_counter;}
            void release(){if(--m_counter == 0)delete this;}
        private:
            unsigned long m_counter;
        };
        CObjectCriticalSection* m_pCriticalSection;

};


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
/// \brief Locks the access to an object of type
///         baseObject.
///
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
class lockObject
{
public:
	/// \brief Lock the access to the pObject's attributes.
	///
	/// The lock is per thread: this means that if a lockObject
	///  successfully locks a baseObject class, then all the
	///  other threads in the application will enter in a
	///  wait mode when they try to lock the same object until
	///  the original lock is released.
	///
	/// @param pObject a pointer to the object to lock
	///
	///////////////////////////////////////////////////////////
	lockObject(baseObject* pObject);

	/// \brief Unlock the access to the locked object.
	///
	///////////////////////////////////////////////////////////
	virtual ~lockObject();

	/// \brief Release the lock on the locked object.
	///
	///////////////////////////////////////////////////////////
	void unlock();

protected:
	// Pointer to the locked object
	///////////////////////////////////////////////////////////
	baseObject* m_pObject;
};


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
/// \brief Locks the access to several objects of type
///         baseObject.
///
/// The class lock ALL the objects listed in the
///  constructor's parameter.
/// If the objects cannot be locked at once, then all of
///  them are left unlocked until all of them can be locked
///  all togheter.
///
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
class lockMultipleObjects
{
public:
	/// \typedef std::list<ptr<baseObject> > tObjectsList
	/// \brief Represents a std::list of pointers to the
	///         objects to be locked (see puntoexe::ptr).
	///
	///////////////////////////////////////////////////////////
	typedef std::list<ptr<baseObject> > tObjectsList;

	/// \brief Construct the locker and lock all the objects
	///         listed in the pObjectsList.
	///
	/// The function looks for the external lockers (if any).
	/// An object may be listed several times in the list.
	///
	/// @param pObjectsList  a list of pointers to the
	///                       objects to be locked
	///
	///////////////////////////////////////////////////////////
	lockMultipleObjects(tObjectsList* pObjectsList);

	/// \brief Destroy the locker and unlock all the locked
	///         objects
	///
	///////////////////////////////////////////////////////////
	virtual ~lockMultipleObjects();

	/// \brief Unlock all the locked objects
	///
	///////////////////////////////////////////////////////////
	void unlock();

protected:
	/// \internal
	/// \brief A list of locked critical sections
	///
	///////////////////////////////////////////////////////////
	std::auto_ptr<tCriticalSectionsList> m_pLockedCS;
};

///@}

} // namespace puntoexe


#endif // !defined(CImbxBaseObject_F1BAF067_21DE_466b_AEA1_6CC4F006FAFA__INCLUDED_)

