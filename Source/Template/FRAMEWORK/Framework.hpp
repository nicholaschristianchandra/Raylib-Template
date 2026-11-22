#pragma once

#include <Framework/Precompiled.hpp>

#ifdef FRAMEWORK_UNIT_DECLARATIONS
	#include <Framework/Includes/Types.hpp>
#else
	#include <Framework/Includes/ForwardDeclarations.hpp>
#endif

#if defined(IS_DEBUG) && !defined(FRAMEWORK_HOST)
	extern "C" __declspec(dllexport) __declspec(selectany) const unsigned int ProgramStateSize = sizeof(Program::State);
	extern "C" __declspec(dllexport) __declspec(selectany) const unsigned int ProgramStateSignature = FRAMEWORK_STATE_SIGNATURE;
#endif
