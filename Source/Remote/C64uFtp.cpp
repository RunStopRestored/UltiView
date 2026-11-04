#include "C64uFtp.h"

//-----------------------------------------------------------------------------

namespace
{

constexpr auto	timeoutMs = 3000;

// One FTP control connection; replies are "NNN text", multi-line ones open
// with "NNN-" and end at the "NNN " line
class Control
{
public:
	bool connect ( const juce::String& ip )
	{
		return socket.connect ( ip, 21, timeoutMs ) && reply () == 220;
	}

	int command ( const juce::String& line )
	{
		const auto	data = line + "\r\n";

		if ( socket.write ( data.toRawUTF8 (), static_cast<int> ( data.getNumBytesAsUTF8 () ) ) < 0 )
			return -1;

		return reply ();
	}

	// The next reply's code, -1 when the connection failed
	int reply ()
	{
		while ( true )
		{
			const auto	line = readLine ();
			if ( line.isEmpty () )
				return -1;

			lastLine = line;

			// The last line of a reply: three digits and a space
			if ( line.length () >= 4 && line.substring ( 0, 3 ).containsOnly ( "0123456789" ) && line[ 3 ] == ' ' )
				return line.substring ( 0, 3 ).getIntValue ();
		}
	}

	juce::StreamingSocket	socket;
	juce::String			lastLine;

private:
	juce::String readLine ()
	{
		juce::MemoryOutputStream	line;

		while ( true )
		{
			if ( socket.waitUntilReady ( true, timeoutMs ) != 1 )
				return {};

			char	c = 0;
			if ( socket.read ( &c, 1, true ) != 1 )
				return {};

			if ( c == '\n' )
				return line.toString ().trimEnd ();

			line.writeByte ( c );
		}
	}
};
//-----------------------------------------------------------------------------

// "227 Entering Passive Mode (h1,h2,h3,h4,p1,p2)" -> the port
int passivePort ( const juce::String& reply )
{
	const auto	numbers = juce::StringArray::fromTokens ( reply.fromFirstOccurrenceOf ( "(", false, false )
															.upToFirstOccurrenceOf ( ")", false, false ), ",", "" );
	if ( numbers.size () != 6 )
		return 0;

	return numbers[ 4 ].getIntValue () * 256 + numbers[ 5 ].getIntValue ();
}
//-----------------------------------------------------------------------------

// "type=dir;size=0;modify=19800101000000; name"
bool parseMlsd ( const juce::String& line, C64uFtp::Entry& entry )
{
	const auto	space = line.indexOfChar ( ' ' );
	if ( space < 0 )
		return false;

	entry.name = line.substring ( space + 1 );
	entry.isDir = false;
	entry.size = 0;

	for ( const auto& fact : juce::StringArray::fromTokens ( line.substring ( 0, space ), ";", "" ) )
	{
		const auto	key = fact.upToFirstOccurrenceOf ( "=", false, false ).toLowerCase ();
		const auto	value = fact.fromFirstOccurrenceOf ( "=", false, false );

		if ( key == "type" )
		{
			if ( value.equalsIgnoreCase ( "cdir" ) || value.equalsIgnoreCase ( "pdir" ) )
				return false;

			entry.isDir = value.equalsIgnoreCase ( "dir" );
		}
		else if ( key == "size" )
		{
			entry.size = value.getLargeIntValue ();
		}
	}

	return entry.name.isNotEmpty () && entry.name != "." && entry.name != "..";
}

}
//-----------------------------------------------------------------------------

juce::String C64uFtp::list ( const juce::String& ip, const juce::String& password,
							 const juce::String& path, std::vector<Entry>& entries )
{
	entries.clear ();

	Control	control;

	if ( ! control.connect ( ip ) )
		return "No FTP answer from " + ip;

	auto	code = control.command ( "USER anonymous" );
	if ( code == 331 )
		code = control.command ( "PASS " + password );

	if ( code != 230 )
		return "FTP login refused: " + control.lastLine;

	if ( control.command ( "PASV" ) != 227 )
		return "FTP passive mode refused: " + control.lastLine;

	const auto	port = passivePort ( control.lastLine );
	if ( port <= 0 )
		return "FTP passive reply not understood: " + control.lastLine;

	juce::StreamingSocket	data;
	if ( ! data.connect ( ip, port, timeoutMs ) )
		return "FTP data connection failed";

	// The C64u lists a directory's contents only with the trailing slash
	const auto	dirPath = path.endsWithChar ( '/' ) ? path : path + "/";

	code = control.command ( "MLSD " + dirPath );
	if ( code != 150 && code != 125 )
		return "Cannot list " + path + ": " + control.lastLine;

	juce::MemoryOutputStream	listing;
	char						buffer[ 2048 ];

	while ( data.waitUntilReady ( true, timeoutMs ) == 1 )
	{
		const auto	n = data.read ( buffer, sizeof ( buffer ), false );
		if ( n <= 0 )
			break;

		listing.write ( buffer, static_cast<size_t> ( n ) );
	}

	data.close ();

	code = control.reply ();
	control.command ( "QUIT" );

	if ( code != 226 && code != 250 )
		return "Cannot list " + path + ": " + control.lastLine;

	for ( const auto& line : juce::StringArray::fromLines ( listing.toUTF8 () ) )
	{
		Entry	entry;
		if ( parseMlsd ( line.trimEnd (), entry ) )
			entries.push_back ( std::move ( entry ) );
	}

	std::sort ( entries.begin (), entries.end (), [] ( const Entry& a, const Entry& b )
	{
		if ( a.isDir != b.isDir )
			return a.isDir;

		return a.name.compareNatural ( b.name ) < 0;
	} );

	return {};
}
//-----------------------------------------------------------------------------
