////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   ProjectDefines.h
//  Version:     v1.00
//  Created:     3/30/2004 by MartinM.
//  Compilers:   Visual Studio.NET
//  Description: to get some defines available in every CryEngine project 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef PROJECTDEFINES_H
#define PROJECTDEFINES_H

#define GAME_IS_FARCRY

#if defined(LINUX)

#if defined(LINUX64)
	#define NOT_USE_PUNKBUSTER_SDK
#endif
	#define _DATAPROBE
	#define NOT_USE_BINK_SDK					// mainly needed for licencees to compile without the Bink integration
	#define NOT_USE_DIVX_SDK					// mainly needed for licencees to compile without the DivX integration
#else
	
	#define _DATAPROBE

//	#define NOT_USE_PUNKBUSTER_SDK				// mainly needed for licencees to compile without the Punkbuster integration
//	#define NOT_USE_BINK_SDK					// mainly needed for licencees to compile without the Bink integration
//	#define NOT_USE_DIVX_SDK					// mainly needed for licencees to compile without the DivX integration
//	#define NOT_USE_ASE_SDK						// mainly needed for licencees to compile without the ASE integration

#endif //LINUX

#endif // PROJECTDEFINES_H