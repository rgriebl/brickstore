/* Copyright (C) 2013-2014 Patrick Brans.  All rights reserved.
**
** This file is part of BrickStock.
** BrickStock is based heavily on BrickStore (http://www.brickforge.de/software/brickstore/)
** by Robert Griebl, Copyright (C) 2004-2008.
**
** This file may be distributed and/or modified under the terms of the GNU 
** General Public License version 2 as published by the Free Software Foundation 
** and appearing in the file LICENSE.GPL included in the packaging of this file.
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
** See http://fsf.org/licensing/licenses/gpl.html for GPL licensing information.
*/

if ( WScript. Arguments. Count ( ) != 1 ) {
	WScript. Echo ( "Usage: " + WScript. ScriptName + " <release>" );
	WScript. Quit ( );
}

var ForReading = 1, ForWriting = 2, ForAppending = 8;
var TristateUseDefault = -2, TristateTrue = -1, TristateFalse = 0;

var fso;

try {
	fso = new ActiveXObject ( "Scripting.FileSystemObject" );
}
catch ( e ) {
	WScript. Echo ( "Could not create ActiveX-object 'Scripting.FileSystemObject'." );
	WScript. Quit ( );
}

try {
	var release = WScript. Arguments. Item ( 0 ). split ( '.', 3 );

	var istream = fso. OpenTextFile ( "version.h.in", ForReading, true, TristateFalse );
	var ostream = fso. OpenTextFile ( "version.h", ForWriting, true, TristateFalse );

	while ( !istream. AtEndOfStream ) {
		var str = istream. ReadLine ( );

		str = str. replace ( /(^#define BRICKSTOCK_MAJOR  *)[^ ]*$/, "$1" + release [0] );
		str = str. replace ( /(^#define BRICKSTOCK_MINOR  *)[^ ]*$/, "$1" + release [1] );
		str = str. replace ( /(^#define BRICKSTOCK_PATCH  *)[^ ]*$/, "$1" + release [2] );

		ostream. WriteLine ( str );
	}
	ostream. Close ( );
	istream. Close ( );
}
catch ( e ) {
	WScript. Echo ( "An error occured: " + e. description + " (#" + ( e. number & 0xffff ) + ")" );
}
