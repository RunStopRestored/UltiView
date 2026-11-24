//-----------------------------------------------------------------------------
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

juce::StringArray C64Keyboard::matrixKeysFor ( const int petscii )
{
	const juce::String	shift = "left_shift";

	auto	ch = [] ( const int c )
	{
		return juce::String::charToString ( static_cast<juce::juce_wchar> ( c ) );
	};

	// Letters: unshifted $41-$5A, shifted $C1-$DA
	if ( petscii >= 0x41 && petscii <= 0x5A )	return { ch ( 'a' + petscii - 0x41 ) };
	if ( petscii >= 0xC1 && petscii <= 0xDA )	return { shift, ch ( 'a' + petscii - 0xC1 ) };

	if ( petscii >= 0x30 && petscii <= 0x39 )	return { ch ( petscii ) };

	// SHIFT + 1-9: ! " # $ % & ' ( )
	if ( petscii >= 0x21 && petscii <= 0x29 )	return { shift, ch ( '1' + petscii - 0x21 ) };

	switch ( petscii )
	{
		case 0x0D:	return { "return" };
		case 0x14:	return { "inst_del" };
		case 0x94:	return { shift, "inst_del" };
		case 0x13:	return { "clr_home" };
		case 0x93:	return { shift, "clr_home" };
		case 0x11:	return { "cursor_up_down" };
		case 0x91:	return { shift, "cursor_up_down" };
		case 0x1D:	return { "cursor_left_right" };
		case 0x9D:	return { shift, "cursor_left_right" };
		case 0x03:	return { "run_stop" };
		case 0x85:	return { "f1" };
		case 0x89:	return { shift, "f1" };
		case 0x86:	return { "f3" };
		case 0x8A:	return { shift, "f3" };
		case 0x87:	return { "f5" };
		case 0x8B:	return { shift, "f5" };
		case 0x88:	return { "f7" };
		case 0x8C:	return { shift, "f7" };
		case 0x20:	return { "space" };
		case 0x2A:	return { "star" };
		case 0x2B:	return { "plus" };
		case 0x2C:	return { "comma" };
		case 0x2D:	return { "minus" };
		case 0x2E:	return { "period" };
		case 0x2F:	return { "slash" };
		case 0x3A:	return { "colon" };
		case 0x3B:	return { "semicolon" };
		case 0x3C:	return { shift, "comma" };
		case 0x3D:	return { "equals" };
		case 0x3E:	return { shift, "period" };
		case 0x3F:	return { shift, "slash" };
		case 0x40:	return { "at" };
		case 0x5B:	return { shift, "colon" };
		case 0x5C:	return { "pound" };
		case 0x5D:	return { shift, "semicolon" };
		case 0x5E:	return { "arrow_up" };
		case 0x5F:	return { "arrow_left" };
		default:	return {};
	}
}
//-----------------------------------------------------------------------------

C64Keyboard::Stroke C64Keyboard::strokeFor ( const juce::KeyPress& key )
{
	const auto	mods = key.getModifiers ();

	if ( mods.isCtrlDown () || mods.isCommandDown () )
		return {};

	// RESTORE; with Alt (C=) the Ultimate menu, through its menu button
	if ( key.getKeyCode () == juce::KeyPress::pageUpKey )
	{
		if ( mods.isAltDown () )
			return { -1, {}, true };

		return { -1, { "restore" } };
	}

	if ( ! mods.isAltDown () )
	{
		const auto	petscii = petsciiFor ( key );
		return { petscii, matrixKeysFor ( petscii ) };
	}

	// Alt: the Commodore key held with the key. With Alt down there may be no
	// text character; the key code of a letter or digit is its character
	auto	c = key.getTextCharacter ();

	if ( c < 0x20 )
	{
		const auto	code = key.getKeyCode ();

		if ( code >= 0x20 && code < 0x7F )
			c = juce::CharacterFunctions::toLowerCase ( static_cast<juce::juce_wchar> ( code ) );
	}

	const juce::KeyPress	plain ( key.getKeyCode (), mods.withoutFlags ( juce::ModifierKeys::altModifier ), c );
	auto					keys = matrixKeysFor ( petsciiFor ( plain ) );

	if ( keys.isEmpty () )
		return {};

	keys.insert ( 0, "commodore" );
	return { -1, keys };
}
//-----------------------------------------------------------------------------

C64Keyboard::Stroke C64Keyboard::commodoreStroke ( const bool withShift )
{
	if ( withShift )
		return { -1, { "commodore", "left_shift" } };

	return { -1, { "commodore" } };
}
//-----------------------------------------------------------------------------

void C64Keyboard::send ( const Stroke& stroke, const juce::String& ip, const juce::String& password )
{
	if ( stroke.isEmpty () || stroke.petscii > 255 || ip.isEmpty () )
		return;

	{
		const juce::ScopedLock	sl ( lock );
		queue.push_back ( { stroke, ip, password } );
	}

	wakeUp.signal ();
}
//-----------------------------------------------------------------------------

bool C64Keyboard::request ( const juce::String& ip, const juce::String& password, const juce::String& method,
							const juce::String& endpoint, const juce::StringArray& params, juce::MemoryBlock* reply )
{
	return requestStatus ( ip, password, method, endpoint, params, reply ) == 200;
}
//-----------------------------------------------------------------------------

int C64Keyboard::requestStatus ( const juce::String& ip, const juce::String& password, const juce::String& method,
								 const juce::String& endpoint, const juce::StringArray& params, juce::MemoryBlock* reply,
								 const juce::String& jsonBody )
{
	// No URL escaping of the endpoint: the C64u does not decode %3A
	auto	url = juce::URL ( "http://" + ip ).getChildURL ( endpoint );

	for ( auto i = 0; i + 1 < params.size (); i += 2 )
		url = url.withParameter ( params[ i ], params[ i + 1 ] );

	const auto	hasBody = jsonBody.isNotEmpty ();

	if ( hasBody )
		url = url.withPOSTData ( jsonBody );

	juce::StringArray	headers;

	if ( password.isNotEmpty () )
		headers.add ( "X-Password: " + password );

	if ( hasBody )
		headers.add ( "Content-Type: application/json" );

	juce::WebInputStream	stream ( url, hasBody );

	stream.withExtraHeaders ( headers.joinIntoString ( "\r\n" ) )
		  .withCustomRequestCommand ( method )
		  .withConnectionTimeout ( 500 )
		  .withNumRedirectsToFollow ( 0 );

	if ( ! stream.connect ( nullptr ) )
		return 0;

	if ( reply != nullptr )
		stream.readIntoMemoryBlock ( *reply );

	return stream.getStatusCode ();
}
//-----------------------------------------------------------------------------

C64Keyboard::Mode C64Keyboard::modeFor ( const juce::String& ip, const juce::String& password )
{
	if ( ip != modeIp )
	{
		modeIp = ip;
		mode = Mode::unknown;
	}

	if ( mode != Mode::unknown )
		return mode;

	// Firmware with the matrix route answers 200 (501 on hardware that cannot
	// inject keys), older firmware 404
	const auto	status = requestStatus ( ip, password, "GET", "v1/machine:input", {}, nullptr );

	if ( status == 200 )
		mode = Mode::matrix;
	else if ( status != 0 )
		mode = Mode::buffer;

	if ( mode != Mode::unknown )
		Z_INFO ( "C64 keyboard: " << ( mode == Mode::matrix ? "keyboard matrix" : "keyboard buffer" )
				 << " (machine:input answered HTTP " << status << ")" );

	return mode;
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
				key = queue.front ();
		}

		if ( key.ip.isEmpty () )
		{
			wakeUp.wait ( -1 );
			continue;
		}

		// C= + RESTORE: the menu button, in order with the keys around it
		if ( key.stroke.menuButton )
		{
			{
				const juce::ScopedLock	sl ( lock );

				if ( ! queue.empty () )
					queue.pop_front ();
			}

			if ( ! request ( key.ip, key.password, "PUT", "v1/machine:menu_button", {}, nullptr ) )
				Z_ERR ( "C64 keyboard: the menu button press failed" );

			continue;
		}

		const auto	m = modeFor ( key.ip, key.password );

		if ( m == Mode::matrix )
		{
			sendMatrix ( key.ip, key.password );
			continue;
		}

		{
			const juce::ScopedLock	sl ( lock );

			if ( m == Mode::unknown )
			{
				// The C64u did not answer: drop what was typed
				queue.clear ();
				continue;
			}

			if ( ! queue.empty () )
				queue.pop_front ();
		}

		// C= and RESTORE presses have no PETSCII code: matrix only
		if ( key.stroke.petscii >= 0 )
			sendBuffer ( static_cast<uint8_t> ( key.stroke.petscii ), key.ip, key.password );
	}
}
//-----------------------------------------------------------------------------

void C64Keyboard::sendMatrix ( const juce::String& ip, const juce::String& password )
{
	// The queued keys for this C64u in one request: the firmware queues the
	// taps and plays them one after the other
	juce::Array<juce::var>	events;
	size_t					taken = 0;

	{
		const juce::ScopedLock	sl ( lock );

		for ( const auto& k : queue )
		{
			// The menu button goes after the keys before it (run () sends it)
			if ( k.ip != ip || events.size () >= maxEvents || k.stroke.menuButton )
				break;

			// RESTORE goes in a request of its own: the documentation wants
			// it alone, and a refusal must not take other keys with it
			const auto	restore = k.stroke.matrix.contains ( "restore" );

			if ( restore && ! events.isEmpty () )
				break;

			++taken;

			if ( k.stroke.matrix.isEmpty () )
				continue;

			juce::Array<juce::var>	inputs;

			for ( const auto& name : k.stroke.matrix )
				inputs.add ( name );

			auto*	ev = new juce::DynamicObject ();
			ev->setProperty ( "kind", "keyboard" );
			ev->setProperty ( "inputs", inputs );
			ev->setProperty ( "transition", "tap" );
			events.add ( juce::var ( ev ) );

			if ( restore )
				break;
		}

		queue.erase ( queue.begin (), queue.begin () + static_cast<std::ptrdiff_t> ( taken ) );
	}

	if ( events.isEmpty () )
		return;

	auto*	body = new juce::DynamicObject ();
	body->setProperty ( "events", events );

	juce::MemoryBlock	reply;
	const auto			status = requestStatus ( ip, password, "POST", "v1/machine:input", {}, &reply,
												 juce::JSON::toString ( juce::var ( body ), true ) );

	if ( status == 400 )
	{
		Z_ERR ( "C64 keyboard: machine:input refused the keys: " << reply.toString ().trim () );
		return;
	}

	if ( status == 404 || status == 501 )
	{
		// Other firmware at this address now: probe again on the next key
		Z_INFO ( "C64 keyboard: machine:input answered HTTP " << status );
		mode = Mode::unknown;
	}
}
//-----------------------------------------------------------------------------

void C64Keyboard::sendBuffer ( const uint8_t code, const juce::String& ip, const juce::String& password )
{
	// Wait until the program has taken the previous key ($C6 = 0)
	const auto	start = juce::Time::getMillisecondCounter ();
	auto		empty = false;

	while ( ! threadShouldExit () )
	{
		juce::MemoryBlock	count;

		if ( ! request ( ip, password, "GET", "v1/machine:readmem", { "address", "00C6", "length", "1" }, &count )
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
		return;
	}

	// The key first, then the count that makes the program see it
	request ( ip, password, "PUT", "v1/machine:writemem",
			  { "address", "0277", "data", juce::String::toHexString ( code ).paddedLeft ( '0', 2 ).toUpperCase () }, nullptr );
	request ( ip, password, "PUT", "v1/machine:writemem", { "address", "00C6", "data", "01" }, nullptr );
}
//-----------------------------------------------------------------------------
