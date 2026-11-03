#include "C64Keyboard.h"

//-----------------------------------------------------------------------------

C64Keyboard::C64Keyboard ()
	: juce::Thread ( "C64Keyboard" )
{
	startThread ( Priority::normal );
}
//-----------------------------------------------------------------------------

C64Keyboard::~C64Keyboard ()
{
	signalThreadShouldExit ();
	wakeUp.signal ();
	waitForThreadToExit ( 2000 );
}
//-----------------------------------------------------------------------------

int C64Keyboard::petsciiFor ( const juce::KeyPress& key )
{
	using KP = juce::KeyPress;

	const auto	mods = key.getModifiers ();

	if ( mods.isCtrlDown () || mods.isAltDown () || mods.isCommandDown () )
		return -1;

	const auto	shift = mods.isShiftDown ();
	const auto	code = key.getKeyCode ();

	if ( code == KP::returnKey )										return 0x0D;
	if ( code == KP::backspaceKey || code == KP::deleteKey )			return shift ? 0x94 : 0x14;	// INST / DEL
	if ( code == KP::insertKey )										return 0x94;
	if ( code == KP::homeKey )											return shift ? 0x93 : 0x13;	// CLR / HOME
	if ( code == KP::upKey )											return 0x91;
	if ( code == KP::downKey )											return 0x11;
	if ( code == KP::leftKey )											return 0x9D;
	if ( code == KP::rightKey )											return 0x1D;
	if ( code == KP::escapeKey )										return 0x03;	// RUN/STOP

	// F1-F8 as on the C64 (F2, F4, F6, F8 are its shifted F1, F3, F5, F7)
	const int	fkeys[] = { KP::F1Key, KP::F2Key, KP::F3Key, KP::F4Key, KP::F5Key, KP::F6Key, KP::F7Key, KP::F8Key };
	const int	fcodes[] = { 0x85, 0x89, 0x86, 0x8A, 0x87, 0x8B, 0x88, 0x8C };

	for ( auto i = 0; i < 8; ++i )
		if ( code == fkeys[ i ] )
			return fcodes[ i ];

	const auto	c = static_cast<int> ( key.getTextCharacter () );

	// Letters by the SHIFT key, not by case (Caps Lock would type graphics):
	// unshifted $41-$5A, shifted $C1-$DA
	if ( c >= 'a' && c <= 'z' )		return shift ? c - 'a' + 0xC1 : c - 'a' + 0x41;
	if ( c >= 'A' && c <= 'Z' )		return shift ? c - 'A' + 0xC1 : c - 'A' + 0x41;

	// Space, digits, punctuation and @ are the same in PETSCII
	if ( c >= 0x20 && c <= 0x40 )	return c;

	switch ( c )
	{
		case '[':		return 0x5B;
		case ']':		return 0x5D;
		case '\\':		return 0x5C;	// the pound sign's key
		case 0xA3:		return 0x5C;	// pound sign
		case '^':		return 0x5E;	// up arrow
		case '_':		return 0x5F;	// left arrow
		case '`':		return 0x5F;
		default:		return -1;
	}
}
//-----------------------------------------------------------------------------

void C64Keyboard::send ( const int petscii, const juce::String& ip, const juce::String& password )
{
	if ( petscii < 0 || petscii > 255 || ip.isEmpty () )
		return;

	{
		const juce::ScopedLock	sl ( lock );
		queue.push_back ( { static_cast<uint8_t> ( petscii ), ip, password } );
	}

	wakeUp.signal ();
}
//-----------------------------------------------------------------------------

bool C64Keyboard::request ( const juce::String& ip, const juce::String& password, const juce::String& method,
							const juce::String& endpoint, const juce::StringArray& params, juce::MemoryBlock* reply )
{
	// No URL escaping of the endpoint: the C64u does not decode %3A
	auto	url = juce::URL ( "http://" + ip ).getChildURL ( endpoint );

	for ( auto i = 0; i + 1 < params.size (); i += 2 )
		url = url.withParameter ( params[ i ], params[ i + 1 ] );

	juce::WebInputStream	stream ( url, false );

	stream.withExtraHeaders ( password.isNotEmpty () ? "X-Password: " + password : juce::String () )
		  .withCustomRequestCommand ( method )
		  .withConnectionTimeout ( 500 )
		  .withNumRedirectsToFollow ( 0 );

	if ( ! stream.connect ( nullptr ) )
		return false;

	if ( reply != nullptr )
		stream.readIntoMemoryBlock ( *reply );

	return stream.getStatusCode () == 200;
}
//-----------------------------------------------------------------------------

void C64Keyboard::run ()
{
	while ( ! threadShouldExit () )
	{
		Key		key;

		{
			const juce::ScopedLock	sl ( lock );

			if ( ! queue.empty () )
			{
				key = queue.front ();
				queue.pop_front ();
			}
		}

		if ( key.ip.isEmpty () )
		{
			wakeUp.wait ( -1 );
			continue;
		}

		// Wait until the program has taken the previous key ($C6 = 0)
		const auto	start = juce::Time::getMillisecondCounter ();
		auto		empty = false;

		while ( ! threadShouldExit () )
		{
			juce::MemoryBlock	count;

			if ( ! request ( key.ip, key.password, "GET", "v1/machine:readmem", { "address", "00C6", "length", "1" }, &count )
				 || count.getSize () < 1 )
				break;		// the C64u did not answer

			if ( static_cast<uint8_t> ( count[ 0 ] ) == 0 )
			{
				empty = true;
				break;
			}

			if ( juce::Time::getMillisecondCounter () - start > maxWaitMs )
				break;		// nothing reads the buffer

			wait ( 10 );
		}

		if ( ! empty )
		{
			const juce::ScopedLock	sl ( lock );
			queue.clear ();
			continue;
		}

		// The key first, then the count that makes the program see it
		request ( key.ip, key.password, "PUT", "v1/machine:writemem",
				  { "address", "0277", "data", juce::String::toHexString ( key.code ).paddedLeft ( '0', 2 ).toUpperCase () }, nullptr );
		request ( key.ip, key.password, "PUT", "v1/machine:writemem", { "address", "00C6", "data", "01" }, nullptr );
	}
}
//-----------------------------------------------------------------------------
