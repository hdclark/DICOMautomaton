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

/*! \file exception.cpp
    \brief Implementation of the exception classes.

*/

#include <utility>

#include "../include/exception.h"
#include "../include/charsetConversion.h"

namespace puntoexe
{

///////////////////////////////////////////////////////////
// Force the construction of the exceptions manager before
//  main() starts
///////////////////////////////////////////////////////////
static exceptionsManager::forceExceptionsConstruction forceConstruction;

	
///////////////////////////////////////////////////////////
// Return the message info for the specified thread
///////////////////////////////////////////////////////////
std::wstring exceptionsManager::getMessage()
{
	tExceptionInfoList infoList;
	exceptionsManager::getExceptionInfo(&infoList);

	std::wstring message;
	for(auto & scanInfo : infoList)
	{
		message += scanInfo.getMessage();
		message += L"\n\n";
	}

	return message;
}


///////////////////////////////////////////////////////////
// Return the info objects for the specified thread
///////////////////////////////////////////////////////////
void exceptionsManager::getExceptionInfo(tExceptionInfoList* pList)
{
	ptr<exceptionsManager> pManager(getExceptionsManager());
	lockObject lock(pManager.get());

	tInfoMap::iterator findInformation = pManager->m_information.find(puntoexe::thread::getThreadId());
	if(findInformation == pManager->m_information.end())
	{
		return;
	}
	for(auto & scanInformation : findInformation->second)
	{
		pList->push_back(scanInformation);
	}
	pManager->m_information.erase(findInformation);
}


///////////////////////////////////////////////////////////
// Add an info object to the current thread
///////////////////////////////////////////////////////////
void exceptionsManager::addExceptionInfo(const exceptionInfo& info)
{
	ptr<exceptionsManager> pManager(getExceptionsManager());
	lockObject lock(pManager.get());
	pManager->m_information[puntoexe::thread::getThreadId()].push_back(info);
}


///////////////////////////////////////////////////////////
// Clears the information list for the current thread
///////////////////////////////////////////////////////////
void exceptionsManager::clearExceptionInfo()
{
	ptr<exceptionsManager> pManager(getExceptionsManager());
	lockObject lock(pManager.get());
	tInfoMap::iterator findInformation = pManager->m_information.find(puntoexe::thread::getThreadId());
	if(findInformation == pManager->m_information.end())
	{
		return;
	}
	pManager->m_information.erase(findInformation);
}

///////////////////////////////////////////////////////////
// Return a pointer to the exceptions manager
///////////////////////////////////////////////////////////
ptr<exceptionsManager> exceptionsManager::getExceptionsManager()
{
	static ptr<exceptionsManager> m_manager(new exceptionsManager);
	return m_manager;
}


///////////////////////////////////////////////////////////
// Construct the exceptionInfo object
///////////////////////////////////////////////////////////
exceptionInfo::exceptionInfo(std::wstring  functionName, std::string  fileName, const long lineNumber, std::string  exceptionType, std::string  exceptionMessage):
	m_functionName(std::move(functionName)), 
	m_fileName(std::move(fileName)),
	m_lineNumber(lineNumber),
	m_exceptionType(std::move(exceptionType)),
	m_exceptionMessage(std::move(exceptionMessage))
{}

///////////////////////////////////////////////////////////
// Construct the exceptionInfo object
///////////////////////////////////////////////////////////
exceptionInfo::exceptionInfo(): m_lineNumber(0)
{}
	
///////////////////////////////////////////////////////////
// Copy constructor
///////////////////////////////////////////////////////////
exceptionInfo::exceptionInfo(const exceptionInfo& right):
			m_functionName(right.m_functionName), 
			m_fileName(right.m_fileName),
			m_lineNumber(right.m_lineNumber),
			m_exceptionType(right.m_exceptionType),
			m_exceptionMessage(right.m_exceptionMessage)
{}

///////////////////////////////////////////////////////////
// Copy operator
///////////////////////////////////////////////////////////
exceptionInfo& exceptionInfo::operator=(const exceptionInfo& right)
{
	m_functionName = right.m_functionName;
	m_fileName = right.m_fileName;
	m_lineNumber = right.m_lineNumber;
	m_exceptionType = right.m_exceptionType;
	m_exceptionMessage = right.m_exceptionMessage;
	return *this;
}

///////////////////////////////////////////////////////////
// Return the exceptionInfo content in a string
///////////////////////////////////////////////////////////
std::wstring exceptionInfo::getMessage()
{
	charsetConversion convertUnicode;
	convertUnicode.initialize("ASCII");
	std::wostringstream message;
	message << "[" << m_functionName << "]" << "\n";
	message << " file: " << convertUnicode.toUnicode(m_fileName) << "  line: " << m_lineNumber << "\n";
	message << " exception type: " << convertUnicode.toUnicode(m_exceptionType) << "\n";
	message << " exception message: " << convertUnicode.toUnicode(m_exceptionMessage) << "\n";
	return message.str();
}



} // namespace puntoexe
