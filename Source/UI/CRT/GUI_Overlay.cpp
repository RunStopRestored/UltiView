#include "GUI_Overlay.h"

#include "ultra-shared/Config/DataSource.h"
#include "Globals/constants.h"

#include "ultra-shared/Video/VIC2_Render.h"

//-----------------------------------------------------------------------------

GUI_Overlay::GUI_Overlay ()
	: CRTEmulation ( true, 2000,
					 datasource::getCRTRoot (),
					 resolutions {	VIC2_Render::outerUnscaledWidth, VIC2_Render::outerUnscaledHeight,
									VIC2_Render::outerUnscaledWidth * 4, VIC2_Render::outerUnscaledHeight * 4 } )
{
	setName ( "CRT" );

	//
	// Settings button
	//
	{
		auto setButtonProps = [ this ] ( GUI_SVG_Button& button )
		{
			button.margin = 14.0f;
			button.bckAlpha[ 1 ] = 0.1f;
			button.bckMargin = 6.0f;
			button.setSize ( 48, 48 );
			button.setWantsKeyboardFocus ( false );
			addAndMakeVisible ( button );
		};

		setButtonProps ( openBrowser );
		setButtonProps ( openSettings );
	}

	// Action buttons
	{
		auto addActionButton = [ this ] ( GUI_IconButton& button )
		{
			button.setWantsKeyboardFocus ( false );
			actionButtons.addAndMakeVisible ( button );

			button.onClick = [ name = button.getName () ]
			{
				UI::sendGlobalMessage ( "c64action {}", name );
			};
		};

		addActionButton ( actionMenu );
		addActionButton ( actionPause );
		addActionButton ( actionResume );
		addActionButton ( actionReboot );
		addActionButton ( actionPower );

		addAndMakeVisible ( actionButtons );
	}

	addChildComponent ( pausedBanner );
	addChildComponent ( c64Files );

	pausedBanner.addComponentListener ( this );
	c64Files.addComponentListener ( this );

	//
	// Setup C64u UDP receiver callback for live preview
	//
	c64uReceiver.setVideoBuffers ( c64uBuffer[ 0 ].getData (), c64uBuffer[ 1 ].getData () );

	c64uReceiver.onVideoFrame = [ this ] ( int finishedBufferIndex, bool isNTSC )
	{
		if ( isNTSC )
		{
			constexpr auto	w = VIC2_Render::outerUnscaledWidth;
			constexpr auto	h = 240;
			constexpr auto	ntscOverscanTop = 15;
			constexpr auto	ntscOverscanBottom = VIC2_Render::outerUnscaledHeight - h - ntscOverscanTop;
			constexpr auto	ntscBottomStart = VIC2_Render::outerUnscaledHeight - ntscOverscanBottom;

			// Move the visible 240 lines down so the buffer stays centered
			std::memmove ( c64uBuffer[ finishedBufferIndex ].getLinePointer ( ntscOverscanTop ), c64uBuffer[ finishedBufferIndex ].getData (), w * h );

			// Clear top and bottom borders (NTSC overscan area)
			std::memset ( c64uBuffer[ finishedBufferIndex ].getData (), 0, ntscOverscanTop * w );
			std::memset ( c64uBuffer[ finishedBufferIndex ].getLinePointer ( ntscBottomStart ), 0, ntscOverscanBottom * w );
		}

		isStreamNTSC.store ( isNTSC, std::memory_order_relaxed );

		setIndexTextureSource ( c64uBuffer[ finishedBufferIndex ] );
	};

	c64uReceiver.onStatusChange = [ this ] ( bool receiving )
	{
		setSignalLost ( ! receiving );
	};

	// Snow until the first frame arrives
	setSignalLost ( true );
}
//-----------------------------------------------------------------------------

GUI_Overlay::~GUI_Overlay ()
{
	pausedBanner.removeComponentListener ( this );
	c64Files.removeComponentListener ( this );

	shutdownGL ();
}
//-----------------------------------------------------------------------------

void GUI_Overlay::newOpenGLContextCreated ()
{
	CRTEmulation::newOpenGLContextCreated ();

	const juce::ScopedLock lock ( streamLock );
	startVideoStream ();
}
//-----------------------------------------------------------------------------

void GUI_Overlay::openGLContextClosing ()
{
	{
		const juce::ScopedLock lock ( streamLock );
		c64uReceiver.stop ();
	}

	CRTEmulation::openGLContextClosing ();
}
//-----------------------------------------------------------------------------

void GUI_Overlay::setStreamAddress ( const juce::String& address )
{
	const juce::ScopedLock lock ( streamLock );

	c64uReceiver.stop ();
	c64uStreamAddress = address;

	if ( isReady () )
		startVideoStream ();
}
//-----------------------------------------------------------------------------

void GUI_Overlay::setStreamPaused ( const bool paused )
{
	{
		const juce::ScopedLock lock ( streamLock );

		streamPaused = paused;

		// Stopping the thread fires no status change: the last frame stays
		if ( paused )
			c64uReceiver.stop ();
	}

	pausedBanner.setVisible ( paused );

	if ( paused )
		return;

	// Snow until the first frame arrives again, as at startup; the address
	// comes back with setupNetworking
	setSignalLost ( true );
}
//-----------------------------------------------------------------------------

void GUI_Overlay::componentVisibilityChanged ( juce::Component& component )
{
	if ( &component == &pausedBanner && pausedBanner.isVisible () != streamPaused )
		pausedBanner.setVisible ( streamPaused );

	if ( &component == &c64Files && c64Files.isVisible () != c64Files.isOpen () )
	{
		c64Files.setVisible ( c64Files.isOpen () );

		// Hiding took the keyboard focus away; typing must not reach the C64
		if ( c64Files.isOpen () )
			c64Files.restoreFocus ();
	}
}
//-----------------------------------------------------------------------------

void GUI_Overlay::PausedBanner::paint ( juce::Graphics& g )
{
	const auto	area = getLocalBounds ().toFloat ().reduced ( 1.0f );

	g.setColour ( juce::Colours::black.withAlpha ( 0.75f ) );
	g.fillRoundedRectangle ( area, 8.0f );

	g.setColour ( juce::Colours::orange );
	g.drawRoundedRectangle ( area, 8.0f, 2.0f );

	g.setFont ( juce::FontOptions ( 18.0f, juce::Font::bold ) );
	g.drawText ( "STREAM PAUSED  -  Ctrl+S resumes", getLocalBounds (), juce::Justification::centred );
}
//-----------------------------------------------------------------------------

void GUI_Overlay::startVideoStream ()
{
	if ( streamPaused )
		return;

	if ( c64uStreamAddress.isNotEmpty () )
		UI::sendGlobalMessage ( "stream-status video {}", c64uReceiver.start ( c64uStreamAddress ).quoted () );
}
//-----------------------------------------------------------------------------

bool GUI_Overlay::isInterestedInDragSource ( const SourceDetails& /*details*/ )
{
	return true;
}
//-----------------------------------------------------------------------------

void GUI_Overlay::itemDropped ( const SourceDetails& details )
{
	UI::sendGlobalMessage ( "c64run {}", details.description.toString ().quoted () );
}
//-----------------------------------------------------------------------------
