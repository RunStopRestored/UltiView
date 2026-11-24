//-----------------------------------------------------------------------------
#if defined ( _WIN32 )
 #include <winsock2.h>
 #include <ws2tcpip.h>
 #include <iphlpapi.h>
 #include <icmpapi.h>

 #pragma comment ( lib, "iphlpapi.lib" )
 #pragma comment ( lib, "ws2_32.lib" )
#endif

#include "C64Ping.h"

//-----------------------------------------------------------------------------

bool pingC64u ( const juce::String& ip, const int timeoutMs )
{
	if ( ip.isEmpty () )
		return false;

#if JUCE_WINDOWS
	// IcmpSendEcho needs no admin rights and no console window
	IN_ADDR	address {};
	if ( InetPtonA ( AF_INET, ip.toRawUTF8 (), &address ) != 1 )
		return false;

	const auto	handle = IcmpCreateFile ();
	if ( handle == INVALID_HANDLE_VALUE )
		return false;

	char	payload[ 8 ] = "ultra";
	char	reply[ sizeof ( ICMP_ECHO_REPLY ) + sizeof ( payload ) + 8 + 64 ] = {};

	const auto	replies = IcmpSendEcho ( handle, address.S_un.S_addr, payload, sizeof ( payload ), nullptr,
										 reply, sizeof ( reply ), static_cast<DWORD> ( timeoutMs ) );
	IcmpCloseHandle ( handle );

	return replies > 0;
#else
	// The ping tool: -W is milliseconds on macOS, seconds on Linux
	juce::ChildProcess	ping;

	#if JUCE_MAC
	const auto	wait = juce::String ( timeoutMs );
	#else
	const auto	wait = juce::String ( std::max ( 1, timeoutMs / 1000 ) );
	#endif

	if ( ! ping.start ( juce::StringArray { "ping", "-c", "1", "-W", wait, ip }, 0 ) )
		return false;

	ping.waitForProcessToFinish ( timeoutMs + 1000 );
	return ping.getExitCode () == 0;
#endif
}
//-----------------------------------------------------------------------------
