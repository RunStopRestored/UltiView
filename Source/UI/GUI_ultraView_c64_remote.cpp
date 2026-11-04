#include "GUI_ultraView.h"

#include "ultra-shared/Config/DataSource.h"

//-----------------------------------------------------------------------------

void GUI_ultraView::c64_reboot ()
{
	network.put ( "v1/machine:reboot" );
}
//-----------------------------------------------------------------------------

void GUI_ultraView::c64_run ( const juce::String& type, const juce::MemoryBlock& crtData, const juce::String& filename )
{
	// Find game in our database, so that we can configure the C64 and CRT-emulation before running it
	const auto	entry =	findGameEntry ( filename );

	c64_forceSystemMode ( entry.isNTSC ? "NTSC" : "PAL" );
	c64_forceJoystickSwapper ( entry.firstJoyport ? "Swapped" : "Normal" );
	mainScreen.crt.setFirstLuma ( entry.firstLuma );

	network.post ( "v1/runners:run_" + type, crtData, [ filename ] ( const juce::var& response, const int statusCode )
	{
		if ( statusCode != 200 )
		{
			Z_ERR ( "Failed to upload file: " << filename.quoted () << "\n" << response[ "errors" ].toString () );
		}
	} );
}
//-----------------------------------------------------------------------------

void GUI_ultraView::c64_runRemoteFile ( const juce::String& path )
{
	Z_INFO ( "Run on the C64u: " << path.quoted () );

	const auto	filename = path.fromLastOccurrenceOf ( "/", false, false );
	const auto	ext = filename.fromLastOccurrenceOf ( ".", false, false ).toLowerCase ();

	auto	reportError = [ this, filename ] ( const juce::String& error )
	{
		Z_ERR ( "Failed to run " << filename.quoted () << ": " << error );
		setTitleStatus ( "Could not run " + filename + ": " + error );
	};

	if ( ext == "sid" || ext == "mod" )
	{
		network.put ( "v1/runners:" + juce::String ( ext == "sid" ? "sidplay" : "modplay" ), { "file", path },
					  [ reportError ] ( const juce::var& response, const int statusCode )
		{
			if ( statusCode != 200 )
				reportError ( response[ "errors" ].toString () );
		} );
		return;
	}

	// Games get their PAL/NTSC, joystick port and luma from the database, as
	// with dropped files
	const auto	entry = findGameEntry ( filename );

	c64_forceSystemMode ( entry.isNTSC ? "NTSC" : "PAL" );
	c64_forceJoystickSwapper ( entry.firstJoyport ? "Swapped" : "Normal" );
	mainScreen.crt.setFirstLuma ( entry.firstLuma );

	if ( C64DiskRunner::isDiskImage ( filename ) )
	{
		diskRunner.onError = reportError;

		// A cartridge survives a reset and would start instead of BASIC
		if ( ! diskRunner.run ( settings->get<juce::String> ( "network/last-ip" ).trim (), settings->get<juce::String> ( "network/password" ), path, cartridgeActive ) )
			reportError ( "the previous disk is still starting" );
		else
			cartridgeActive = loadedEasyFlash = false;

		return;
	}

	auto	runIt = [ this, path, ext, reportError ]
	{
		network.put ( "v1/runners:run_" + ext, { "file", path }, [ reportError ] ( const juce::var& response, const int statusCode )
		{
			if ( statusCode != 200 )
				reportError ( response[ "errors" ].toString () );
		} );
	};

	if ( ext == "crt" )
	{
		// The cartridge type is unknown without reading the file: assume an
		// EasyFlash, the next cartridge then reboots first (see c64run)
		const auto	wasEasyFlash = std::exchange ( loadedEasyFlash, true );
		cartridgeActive = true;

		if ( wasEasyFlash )
		{
			c64_reboot ();
			juce::Timer::callAfterDelay ( 2000, runIt );
			return;
		}
	}

	runIt ();
}
//-----------------------------------------------------------------------------

void GUI_ultraView::c64_forceSystemMode ( const juce::String& mode )
{
	network.put ( "v1/configs/U64 Specific Settings/System Mode", { "value", mode } );
}
//-----------------------------------------------------------------------------

void GUI_ultraView::c64_forceJoystickSwapper ( const juce::String& mode )
{
	network.put ( "v1/configs/U64 Specific Settings/Joystick Swapper", { "value", mode } );
}
//-----------------------------------------------------------------------------

void GUI_ultraView::loadGamesDatabase ()
{
	gamesDatabase.clear ();

	const auto	csv = datasource::loadText ( "Data/Games.csv" ).toLowerCase ();
	const auto	lines = juce::StringArray::fromLines ( csv );

	const auto	columns = juce::StringArray::fromTokens ( lines[ 0 ], ",", "" );
	for ( auto i = 1; i < lines.size (); ++i )
	{
		auto	line = lines[ i ];
		auto	values = juce::StringArray::fromTokens ( line, ",", "" );
		if ( values.size () != columns.size () )
			continue;

		GameEntry	entry;

		for ( auto j = 0; j < columns.size (); ++j )
		{
			const auto&	column = columns[ j ].trim ();
			const auto&	value = values[ j ].trim ();

			if ( column == "name" )			entry.name			= value.toStdString ();
			else if ( column == "sys" )		entry.isNTSC		= value == "ntsc";
			else if ( column == "vic" )		entry.firstLuma		= value == "first";
			else if ( column == "joy" )		entry.firstJoyport	= value == "1";
		}

		gamesDatabase.push_back ( std::move ( entry ) );
	}
}
//-----------------------------------------------------------------------------

GUI_ultraView::GameEntry GUI_ultraView::findGameEntry ( juce::String filename ) const
{
	filename = filename.toLowerCase ();
	filename = filename.replaceCharacter ( '\\', '/' );
	filename = filename.fromLastOccurrenceOf ( "/", false, false );
	filename = filename.upToLastOccurrenceOf ( ".", false, false );
	filename = filename.trim ();

	const auto	hasJ1 = filename.containsIgnoreCase ( "(j1)" );
	filename = filename.replace ( " (J1)", "" );

	// Early out for games with () in them
	{
		const auto	normalizedName = filename.toStdString ();

		auto	it = std::ranges::find_if ( gamesDatabase, [ &normalizedName ] ( const GameEntry& entry ) {
			return normalizedName == entry.name;
		} );

		if ( it != gamesDatabase.end () )
			return *it;
	}

	// Not found, try some more aggressive filename cleanup and search again
	filename = filename.upToFirstOccurrenceOf ( "(", false, false );
	filename = filename.upToFirstOccurrenceOf ( "[", false, false );
	filename = filename.trim ();

	filename = filename.retainCharacters ( "abcdefghijklmnopqrstuvwxyz0123456789 " );
	while ( filename.contains ( "  " ) )
		filename = filename.replace ( "  ", " " );

	if ( filename.startsWith ( "the " ) )	filename = filename.substring ( 4 );
	if ( filename.endsWith ( " the" ) )	filename = filename.dropLastCharacters ( 4 );

	const auto	normalizedName = helpers::normalizeGamesString ( filename.toStdString () );

	auto	it = std::ranges::find_if ( gamesDatabase, [ &normalizedName ] ( const GameEntry& entry ) {
		return normalizedName == entry.name;
	} );

	if ( it != gamesDatabase.end () )
		return *it;

	// Game is not in list, use filename to make a best effort guess for joystick swap
	return { .firstJoyport = hasJ1 };
}
//-----------------------------------------------------------------------------
