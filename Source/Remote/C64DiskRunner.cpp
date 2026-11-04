#include "C64DiskRunner.h"

#include "C64Keyboard.h"

//-----------------------------------------------------------------------------

namespace
{
	// Longest wait for BASIC's READY. after the reset
	constexpr auto	readyTimeoutMs = 10000;

	juce::String extensionOf ( const juce::String& name )
	{
		return name.fromLastOccurrenceOf ( ".", false, false ).toLowerCase ();
	}

	juce::String driveModeFor ( const juce::String& ext )
	{
		if ( ext == "d71" || ext == "g71" )		return "1571";
		if ( ext == "d81" )						return "1581";
		return "1541";
	}

	juce::String hexBytes ( std::initializer_list<int> bytes )
	{
		juce::String	hex;
		for ( const auto b : bytes )
			hex += juce::String::toHexString ( b ).paddedLeft ( '0', 2 ).toUpperCase ();
		return hex;
	}
}
//-----------------------------------------------------------------------------

C64DiskRunner::C64DiskRunner ()
	: juce::Thread ( "C64DiskRunner" )
{
}
//-----------------------------------------------------------------------------

C64DiskRunner::~C64DiskRunner ()
{
	stopThread ( 3000 );
}
//-----------------------------------------------------------------------------

bool C64DiskRunner::isDiskImage ( const juce::String& name )
{
	return juce::StringArray { "d64", "g64", "d71", "g71", "d81" }.contains ( extensionOf ( name ) );
}
//-----------------------------------------------------------------------------

bool C64DiskRunner::run ( const juce::String& _ip, const juce::String& _password, const juce::String& _imagePath, const bool _rebootFirst )
{
	if ( isThreadRunning () )
		return false;

	ip = _ip;
	password = _password;
	imagePath = _imagePath;
	rebootFirst = _rebootFirst;

	return startThread ( Priority::normal );
}
//-----------------------------------------------------------------------------

void C64DiskRunner::run ()
{
	const auto	error = runSequence ();

	if ( error.isEmpty () || threadShouldExit () )
		return;

	Z_ERR ( "Disk run failed: " << error );

	juce::MessageManager::callAsync ( [ this, error ]
	{
		if ( onError )
			onError ( error );
	} );
}
//-----------------------------------------------------------------------------

juce::String C64DiskRunner::runSequence ()
{
	auto	rest = [ this ] ( const juce::String& method, const juce::String& endpoint, const juce::StringArray& params, juce::MemoryBlock* reply = nullptr )
	{
		return C64Keyboard::request ( ip, password, method, endpoint, params, reply );
	};

	if ( rebootFirst )
	{
		if ( ! rest ( "PUT", "v1/machine:reboot", {} ) )
			return "Could not reboot the C64";

		// The same pause the app gives a reboot before the next cartridge
		wait ( 2000 );
	}

	// Drive A: on and of the image's type
	{
		juce::MemoryBlock	reply;
		if ( ! rest ( "GET", "v1/drives", {}, &reply ) )
			return "The C64 Ultimate did not answer";

		const auto	drives = juce::JSON::parse ( reply.toString () )[ "drives" ];
		juce::var	driveA;

		if ( auto* list = drives.getArray () )
			for ( const auto& d : *list )
				if ( d.hasProperty ( "a" ) )
					driveA = d[ "a" ];

		const auto	mode = driveModeFor ( extensionOf ( imagePath ) );

		if ( driveA[ "type" ].toString () != mode && ! rest ( "PUT", "v1/drives/a:set_mode", { "mode", mode } ) )
			return "Could not switch drive A to " + mode;

		if ( ! static_cast<bool> ( driveA[ "enabled" ] ) && ! rest ( "PUT", "v1/drives/a:on", {} ) )
			return "Could not switch drive A on";
	}

	if ( ! rest ( "PUT", "v1/drives/a:mount", { "image", imagePath } ) )
		return "Could not mount " + imagePath;

	if ( ! rest ( "PUT", "v1/machine:reset", {} ) )
		return "Could not reset the C64";

	// READY.: the reset's RAM test zeroes the cursor row ($D6) and CINT clears
	// the screen, so a stale READY. from before the reset cannot match. Wait
	// for the row above the cursor to read READY. (screen codes)
	const auto	start = juce::Time::getMillisecondCounter ();
	auto		cursorRow = -1;

	wait ( 500 );

	while ( ! threadShouldExit () )
	{
		juce::MemoryBlock	row;

		if ( rest ( "GET", "v1/machine:readmem", { "address", "00D6", "length", "1" }, &row ) && row.getSize () == 1 )
		{
			const auto	r = static_cast<int> ( static_cast<uint8_t> ( row[ 0 ] ) );

			juce::MemoryBlock	text;
			const auto	address = juce::String::toHexString ( 0x0400 + ( r - 1 ) * 40 ).paddedLeft ( '0', 4 ).toUpperCase ();

			if ( r >= 1 && r < 25
				 && rest ( "GET", "v1/machine:readmem", { "address", address, "length", "6" }, &text ) && text.getSize () == 6
				 && std::memcmp ( text.getData (), "\x12\x05\x01\x04\x19\x2e", 6 ) == 0 )
			{
				cursorRow = r;
				break;
			}
		}

		if ( juce::Time::getMillisecondCounter () - start > readyTimeoutMs )
			return "The C64 did not reach READY. after the reset";

		wait ( 250 );
	}

	if ( cursorRow < 0 )
		return {};

	// LOAD"*",8,1 at the cursor (screen codes), then RETURN RUN RETURN
	const auto	lineAddress = juce::String::toHexString ( 0x0400 + cursorRow * 40 ).paddedLeft ( '0', 4 ).toUpperCase ();

	if ( ! rest ( "PUT", "v1/machine:writemem", { "address", lineAddress,
					"data", hexBytes ( { 0x0C, 0x0F, 0x01, 0x04, 0x22, 0x2A, 0x22, 0x2C, 0x38, 0x2C, 0x31 } ) } ) )
		return "Could not write the LOAD line";

	if ( ! rest ( "PUT", "v1/machine:writemem", { "address", "0277", "data", hexBytes ( { 0x0D, 0x52, 0x55, 0x4E, 0x0D } ) } )
		 || ! rest ( "PUT", "v1/machine:writemem", { "address", "00C6", "data", "05" } ) )
		return "Could not type into the keyboard buffer";

	return {};
}
//-----------------------------------------------------------------------------
