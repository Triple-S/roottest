 ////////////////////////////////////////////////////////////////////////////
// $Id: PlexCalib.cxx,v 1.1 2001/11/26 22:54:37 rhatcher Exp $
//
// PlexCalib
//
// PlexCalib is an *interface* 
//   Use this class as a common mix-in that any "Calibrator" can
//   derive from.  This allows Plexus/PlexHandle to generate a 
//   fully filled PlexSEIdAltL knowning *only* about this interface.
//
// Author:  R. Hatcher 2001.11.15
//
////////////////////////////////////////////////////////////////////////////

#include "PlexCalib.h"

ClassImp(PlexCalib)

#
