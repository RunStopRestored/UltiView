#include "GUI_ultraView.h"

#include "Globals/constants.h"
#include "Remote/C64Ping.h"
#include "ultra-shared/Helpers/PlatformHelper.h"

//-----------------------------------------------------------------------------

void GUI_ultraView::setTitleStatus ( const juce::String& status )
{
	juce::MessageManager::callAsync ( [ this, status ] {
		getTopLevelComponent ()->setName ( helpers::appTitle () + " - " + status );
	} );
}
//-----------------------------------------------------------------------------

void GUI_ultraView::findC64OnNetwork ()
{
	auto	lastIP = settings->get<juce::String> ( "network/last-ip" ).trim ();

	netScanning = true;
	setTitleStatus ( strings->get ( "network/searching" ) );

	c64uScanner.scan ( [ this ] ( const juce::String& ip, const bool passwordRequired )
	{
		netScanning = false;

		if ( passwordRequired )
		{
			// Remember the machine so the probe loop watches it; rescanning
			// can't help until the password matches
			settings->set ( "network/last-ip", ip );
			setTitleStatus ( strings->get ( "network/password-required" ) );
			return;
		}

		if ( ip.isEmpty () )
		{
			Z_ERR ( "No C64 found on the network" );
			return;
		}

		Z_LOG ( "Found C64 at " + ip );

		const auto	ipOnly = ip.upToFirstOccurrenceOf ( " ", false, false );

		{
			const juce::ScopedLock	sl ( titleLock );
			connectedTitle = ip;
		}
		setTitleStatus ( ip );

		settings->set ( "network/last-ip", ipOnly );
		network.setBaseAddress ( "http://" + ipOnly );
		network.setC64uPassword ( settings->get<juce::String> ( "network/password" ) );

		setupNetworking ();

	}, lastIP );
}
//-----------------------------------------------------------------------------

void GUI_ultraView::timerCallback ()
{
	reconnectTick ();
}
//-----------------------------------------------------------------------------

void GUI_ultraView::pauseStreams ( const juce::String& bannerText, const juce::String& titleStatus )
{
	streamsPaused = true;
	healWatchdog.stopTimer ();

	// Only stop what we started: a stream someone else already took over
	// stays theirs
	if ( streamsStarted.exchange ( false ) )
	{
		network.put ( "v1/streams/video:stop" );
		network.put ( "v1/streams/audio:stop" );
	}

	mainScreen.crt.setStreamPaused ( true, bannerText );
	c64uReceiver.stop ();
	netReceiving = false;

	juce::String	title;
	{
		const juce::ScopedLock	sl ( titleLock );
		title = connectedTitle;
	}
	setTitleStatus ( title + " - " + titleStatus );
}
//-----------------------------------------------------------------------------

void GUI_ultraView::streamStartFailed ( const juce::String& error )
{
	if ( streamsPaused )
		return;

	Z_ERR ( "The C64u did not start its stream: " << error );

	pauseStreams ( "STREAM NOT STARTED  -  Ctrl+S retries", "stream not started" );

	juce::NativeMessageBox::showAsync ( juce::MessageBoxOptions ()
											.withIconType ( juce::MessageBoxIconType::WarningIcon )
											.withTitle ( "Stream not started" )
											.withMessage ( "The C64 Ultimate did not start its stream to this computer:\n\n"
														   + error
														   + "\n\nAnother computer may be streaming from it. Stop the stream there"
															 " (or restart the C64 Ultimate), then retry.\n\n"
															 "ultraView keeps running; Ctrl+S retries too." )
											.withButton ( "Retry" )
											.withButton ( "Close" )
											.withAssociatedComponent ( this ),
										[ safe = juce::Component::SafePointer<GUI_ultraView> ( this ) ] ( const int result )
										{
											// Retry: resume, which starts both streams again
											if ( result == 0 && safe != nullptr && safe->streamsPaused )
												safe->toggleStreamPause ();
										} );
}
//-----------------------------------------------------------------------------

void GUI_ultraView::toggleStreamPause ()
{
	if ( ! streamsPaused )
	{
		pauseStreams ( {}, "stream paused" );
		Z_LOG ( "Streams paused, UDP ports released" );
		return;
	}

	streamsPaused = false;
	mainScreen.crt.setStreamPaused ( false );

	Z_LOG ( "Streams resumed" );

	// Probe and start the streams right away instead of waiting out the tick
	probeFailures = 0;
	reconnectTick ();
}
//-----------------------------------------------------------------------------

void GUI_ultraView::reconnectTick ()
{
	if ( streamsPaused || netReceiving || netScanning || netProbing )
		return;

	// A heal cycle is still judging its streams; restarting them here would
	// flip the targets back and forth forever
	if ( healWatchdog.isTimerRunning () )
		return;

	const auto	lastIP = settings->get<juce::String> ( "network/last-ip" ).trim ();

	// A few quick probes of the known address, then a full sweep in case the
	// C64u came back under a new one
	if ( lastIP.isEmpty () || probeFailures >= 4 )
	{
		probeFailures = 0;
		findC64OnNetwork ();
		return;
	}

	netProbing = true;
	setTitleStatus ( strings->get ( "network/waiting" ) );

	network.setBaseAddress ( "http://" + lastIP );
	network.setC64uPassword ( settings->get<juce::String> ( "network/password" ) );

	network.get ( "v1/info", {}, [ this, lastIP ] ( const juce::var& result, const int httpCode )
	{
		netProbing = false;

		// Paused while probing: keep the "stream paused" title
		if ( streamsPaused )
			return;

		// A refusal means the machine is there; keep watching it instead of
		// sweeping the network
		if ( httpCode == 403 )
		{
			setTitleStatus ( strings->get ( "network/password-required" ) );
			return;
		}

		if ( httpCode < 200 || httpCode >= 300 )
		{
			streamsStarted = false;
			++probeFailures;
			return;
		}

		probeFailures = 0;

		const auto	title = lastIP + " (" + result[ "hostname" ].toString () + ")";
		{
			const juce::ScopedLock	sl ( titleLock );
			connectedTitle = title;
		}
		setTitleStatus ( title );

		setupNetworking ();
	} );
}
//-----------------------------------------------------------------------------

// Streams aimed elsewhere get this long to deliver before healStreams pulls
// them over, counted from the last start answer
constexpr auto	healWatchdogMs = 750;

#if JUCE_WINDOWS
// The verdict pass needs no grace on Windows: the firewall query settles
// whether the silence is a saved block rule
constexpr auto	blockedVerdictMs = healWatchdogMs;
#else
// A healed stream gets this long before its silence counts as locally blocked;
// generous, so a user still facing the OS permission prompt can answer it
constexpr auto	blockedVerdictMs = 5000;
#endif

//-----------------------------------------------------------------------------

// Swaps the target's machine for our own, keeping an explicit port; the C64u's
// stored config stays untouched
static juce::String retargetToOwnIP ( const juce::String& address, const juce::String& ownIP )
{
	const auto	port = address.fromFirstOccurrenceOf ( ":", false, false );
	return port.isEmpty () ? ownIP : ownIP + ":" + port;
}
//-----------------------------------------------------------------------------

void GUI_ultraView::setupNetworking ()
{
	// A scan or probe that finished after the pause key must not take the
	// streams back
	if ( streamsPaused )
		return;

	// Start stream
	network.get ( "v1/configs/Data Streams", {}, [ this ] ( const juce::var& result, const int httpCode )
	{
		if ( httpCode < 200 || httpCode >= 300 || streamsPaused )
			return;

		auto	videoAddress = result[ "Data Streams" ][ "Stream VIC to" ].toString ();
		auto	audioAddress = result[ "Data Streams" ][ "Stream Audio to" ].toString ();

		// Multicast targets flood consumer routers, so the streams get aimed
		// at this machine; the stored targets only contribute their ports
		if ( const auto ownIP = C64uScanner::localAddressFor ( settings->get<juce::String> ( "network/last-ip" ) ); ownIP.isNotEmpty () )
		{
			videoAddress = retargetToOwnIP ( videoAddress, ownIP );
			audioAddress = retargetToOwnIP ( audioAddress, ownIP );
		}

		startStreams ( videoAddress, audioAddress, 1 );

		mainScreen.crt.setStreamAddress ( videoAddress );

		if ( streamsPaused )
			return;

		c64uReceiver.stop ();
		UI::sendGlobalMessage ( "stream-status audio {}", c64uReceiver.start ( audioAddress ).quoted () );
	} );
}
//-----------------------------------------------------------------------------

void GUI_ultraView::startStreams ( const juce::String& videoAddress, const juce::String& audioAddress, const int retriesLeft )
{
	const auto	c64uIP = settings->get<juce::String> ( "network/last-ip" ).trim ();

	// A clean start: stop whatever the C64u still streams (another machine's
	// stream, or one stuck), then ping it. A C64u stuck refusing streams to
	// this machine ("Network Host Resolve Error") takes them again after a
	// ping from it. The queue runs in order and a callback finishes before
	// the next request, so the ping lands between the stops and the starts
	network.put ( "v1/streams/video:stop" );
	network.put ( "v1/streams/audio:stop", {}, [ c64uIP ] ( const juce::var&, const int )
	{
		if ( ! pingC64u ( c64uIP, 1000 ) )
			Z_LOG ( "The C64u did not answer a ping" );
	} );

	// Both answers decide; the video answer is in before the audio callback
	// reads it (same order rule)
	auto	videoError = std::make_shared<juce::String> ();

	// The C64u's own error text, else the HTTP status; empty on success
	auto	startError = [] ( const juce::String& stream, const juce::var& reply, const int code ) -> juce::String
	{
		if ( code >= 200 && code < 300 )
			return {};

		if ( code == 0 )
			return stream + ": no answer";

		juce::String	text;
		if ( auto* errors = reply[ "errors" ].getArray (); errors != nullptr && ! errors->isEmpty () )
			text = errors->getFirst ().toString ();

		return stream + ": " + ( text.isNotEmpty () ? text + " (HTTP " + juce::String ( code ) + ")" : "HTTP " + juce::String ( code ) );
	};

	network.put ( "v1/streams/video:start", { "ip", videoAddress }, [ videoError, startError ] ( const juce::var& reply, const int code )
	{
		*videoError = startError ( "Video", reply, code );
	} );

	network.put ( "v1/streams/audio:start", { "ip", audioAddress }, [ this, videoAddress, audioAddress, retriesLeft, videoError, startError ] ( const juce::var& reply, const int startCode )
	{
		const auto	audioError = startError ( "Audio", reply, startCode );

		// Either one running is ours to stop again
		streamsStarted = videoError->isEmpty () || audioError.isEmpty ();

		juce::StringArray	errors;
		errors.add ( *videoError );
		errors.add ( audioError );
		errors.removeEmptyStrings ();

		const auto	error = errors.joinIntoString ( "\n" );

		// Refused: once more (stop, ping, start) before telling the user
		if ( error.isNotEmpty () && retriesLeft > 0 && ! streamsPaused )
		{
			Z_LOG ( "Stream start refused, retrying: " << error.replace ( "\n", "; " ) );
			startStreams ( videoAddress, audioAddress, retriesLeft - 1 );
			return;
		}

		juce::MessageManager::callAsync ( [ this, videoAddress, audioAddress, error ]
		{
			if ( error.isNotEmpty () )
			{
				streamStartFailed ( error );
				return;
			}

			// Paused while the start was on its way: take it back
			if ( streamsPaused && streamsStarted.exchange ( false ) )
			{
				network.put ( "v1/streams/video:stop" );
				network.put ( "v1/streams/audio:stop" );
			}

			videoStreamTarget = videoAddress;
			audioStreamTarget = audioAddress;

			if ( ! streamsPaused )
				healWatchdog.startTimer ( healWatchdogMs );
		} );
	} );
}
//-----------------------------------------------------------------------------

void GUI_ultraView::healStreams ()
{
	healWatchdog.stopTimer ();

	if ( streamsPaused )
		return;

	const auto	ownIP = C64uScanner::localAddressFor ( settings->get<juce::String> ( "network/last-ip" ) );
	if ( ownIP.isEmpty () )
		return;

	// A target pointing at this machine has nothing left to heal; anything
	// else (multicast group, another machine) gets pulled over
	auto	healed = false;		// pulled over, judge again next round
	auto	blocked = false;	// aimed at this machine yet silent: dropped locally

	if ( mainScreen.crt.isStreamStalled () )
	{
		if ( videoStreamTarget.upToFirstOccurrenceOf ( ":", false, false ) != ownIP )
		{
			videoStreamTarget = retargetToOwnIP ( videoStreamTarget, ownIP );
			Z_LOG ( "Video stream is silent, retargeting it to " + videoStreamTarget );

			network.put ( "v1/streams/video:stop" );
			network.put ( "v1/streams/video:start", { "ip", videoStreamTarget } );

			mainScreen.crt.setStreamAddress ( videoStreamTarget );
			healed = true;
		}
		else
		{
			blocked = true;
		}
	}

	if ( c64uReceiver.isStalled () )
	{
		if ( audioStreamTarget.upToFirstOccurrenceOf ( ":", false, false ) != ownIP )
		{
			audioStreamTarget = retargetToOwnIP ( audioStreamTarget, ownIP );
			Z_LOG ( "Audio stream is silent, retargeting it to " + audioStreamTarget );

			network.put ( "v1/streams/audio:stop" );
			network.put ( "v1/streams/audio:start", { "ip", audioStreamTarget } );

			c64uReceiver.stop ();
			UI::sendGlobalMessage ( "stream-status audio {}", c64uReceiver.start ( audioStreamTarget ).quoted () );
			healed = true;
		}
		else
		{
			blocked = true;
		}
	}

	if ( healed )
		healWatchdog.startTimer ( blockedVerdictMs );
	else if ( blocked )
	{
#if JUCE_WINDOWS
		// The firewall answers with certainty: no block rule means the user
		// simply hasn't decided yet, stay quiet
		if ( firewallBlocksThisApp () )
			showFirewallNotice ();
#else
		showFirewallNotice ();
#endif
	}
}
//-----------------------------------------------------------------------------

void GUI_ultraView::showFirewallNotice ()
{
	if ( std::exchange ( firewallNoticeShown, true ) )
		return;

	Z_ERR ( "Inbound stream data is being blocked on this machine" );

#if JUCE_WINDOWS || JUCE_MAC
 #if JUCE_WINDOWS
	const auto	messageKey = "network/blocked-windows";
	const auto	buttonKey = "network/blocked-open-windows";
 #else
	const auto	messageKey = "network/blocked-mac";
	const auto	buttonKey = "network/blocked-open-mac";
 #endif

	juce::NativeMessageBox::showAsync ( juce::MessageBoxOptions ()
											.withIconType ( juce::MessageBoxIconType::WarningIcon )
											.withTitle ( strings->get ( "network/blocked-title" ) )
											.withMessage ( strings->get ( messageKey ) )
											.withButton ( strings->get ( buttonKey ) )
											.withButton ( strings->get ( "network/blocked-close" ) )
											.withAssociatedComponent ( this ),
										[] ( const int result )
										{
											if ( result != 0 )
												return;

 #if JUCE_WINDOWS
											juce::ChildProcess ().start ( "control /name Microsoft.WindowsFirewall /page pageConfigureApps" );
 #else
											juce::URL ( "x-apple.systempreferences:com.apple.preference.security?Privacy_LocalNetwork" ).launchInDefaultBrowser ();
 #endif
										} );
#endif
}
//-----------------------------------------------------------------------------
