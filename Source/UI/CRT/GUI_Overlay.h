#pragma once

#include <JuceHeader.h>

#include "UI/Components/GUI_IconButton.h"
#include "UI/CRT/GUI_C64Files.h"
#include "ultra-shared/UI/Components/GUI_SVG_Button.h"

#include "ultra-shared/Network/C64u_UDP_Receiver.h"

#include "Globals/constants.h"

//-----------------------------------------------------------------------------

class GUI_Overlay final
	: public lime::CRTEmulation
	, public juce::DragAndDropTarget
	, private juce::ComponentListener
{
public:
	GUI_Overlay ();
	~GUI_Overlay () override;

	// lime::CRTEmulation
	void newOpenGLContextCreated () override;
	void openGLContextClosing () override;

	// juce::DragAndDropTarget
	bool isInterestedInDragSource ( const SourceDetails& details ) override;
	void itemDropped ( const SourceDetails& details ) override;

	// this
	void setStreamAddress ( const juce::String& address );
	bool isStreamStalled () const { return c64uReceiver.isStalled (); }

	// Paused: the video port is closed for other listeners, the last frame
	// stays on screen under a banner (its text when given, else "paused")
	void setStreamPaused ( bool paused, const juce::String& bannerText = {} );

	// Browser for the C64u's SD / USB storage
	void toggleC64Files () { c64Files.isOpen () ? c64Files.close () : c64Files.open (); }

	GUI_SVG_Button	openBrowser { "browser", { "crt/browser_close", "crt/browser_open" } };
	GUI_SVG_Button	openSettings { "open", { "crt/settings_close", "crt/settings_open" } };

	std::atomic<bool>	isStreamNTSC = false;

private:
	// C64u UDP Receiver
	juce::CriticalSection	streamLock;
	juce::String			c64uStreamAddress;
	lime::openGL_Image		c64uBuffer[ 2 ] = { { 1, 384, 272 }, { 1, 384, 272 } };
	C64u_UDP_Receiver		c64uReceiver { C64u_UDP_Receiver::streamType::video };

	bool					streamPaused = false;

	void startVideoStream ();

	// "Stream paused" banner over the frozen frame
	struct PausedBanner final : juce::Component
	{
		PausedBanner () : juce::Component ( "stream-paused" ) { setInterceptsMouseClicks ( false, false ); }
		void paint ( juce::Graphics& g ) override;

		juce::String	text;
	};
	PausedBanner	pausedBanner;

	GUI_C64Files	c64Files;

	// lime's ShaderToyComponent shows all children on mouse movement and hides
	// them when it stops (the hover buttons); the banner and the files panel
	// follow their own state instead, put back here
	void componentVisibilityChanged ( juce::Component& component ) override;

	// Action buttons
	juce::Component		actionButtons { "action-buttons" };
		GUI_IconButton		actionMenu { "menu_button", UI::colors::actionOk };
		GUI_IconButton		actionPause { "pause", UI::colors::actionInfo };
		GUI_IconButton		actionResume { "resume", UI::colors::actionInfo };
		GUI_IconButton		actionReboot { "reboot", UI::colors::actionWarning };
		GUI_IconButton		actionPower { "poweroff", UI::colors::actionDanger };

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_Overlay )
};
//-----------------------------------------------------------------------------
