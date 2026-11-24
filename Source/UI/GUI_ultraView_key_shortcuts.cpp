#include "GUI_ultraView.h"
#include "ultra-shared/UI/GUI_LookAndFeel.h"

//-----------------------------------------------------------------------------

// The user-facing keys are data (Data/UI/shortcuts.csv, verb per key) and go
// through actionListenerCallback like any other message. The modal Escape and
// the hidden developer combos stay ahead of the lookup, they are not in the
// list by design.

bool GUI_ultraView::keyPressed ( const juce::KeyPress& key )
{
	// Any key arms the focus ring
	focusRing.keyboardUsed ();

	// Tab with the focus on this component itself steps into the children
	if ( key.isKeyCode ( juce::KeyPress::tabKey ) && hasKeyboardFocus ( false ) )
	{
		if ( auto traverser = createKeyboardFocusTraverser () )
		{
			if ( const auto all = traverser->getAllComponents ( this ); ! all.empty () )
				( key.getModifiers ().isShiftDown () ? all.back () : all.front () )->grabKeyboardFocus ();
		}

		return true;
	}

	if ( key == juce::KeyPress ( juce::KeyPress::escapeKey, juce::ModifierKeys::noModifiers, 0 ) )
	{
		if ( aboutScreen.isVisible () )
			actionListenerCallback ( "closeAbout" );
		else if ( shortcutsScreen.isVisible () )
			actionListenerCallback ( "closeShortcuts" );
		else
			return sendC64Key ( key );		// RUN/STOP

		return true;
	}

	// Typing goes to the C64 ahead of the plain-key shortcuts ("?")
	if ( sendC64Key ( key ) )
		return true;

#if ULTRA_INSPECTOR
	if ( key == juce::KeyPress ( juce::KeyPress::F11Key, juce::ModifierKeys::ctrlModifier, 0 ) )
	{
		// Toggle inspector
		if ( ! inspector )
		{
			inspector = std::make_unique<melatonin::Inspector> ( *this );
			inspector->setRootFollowsComponentUnderMouse ( true );
			inspector->setVisible ( true );
			inspector->setAlwaysOnTop ( true );
			inspector->onClose = [ this ] { inspector = nullptr; };
		}
		else
		{
			inspector = nullptr;
		}
	}
	else
#endif
	if ( key == juce::KeyPress ( juce::KeyPress::F11Key, juce::ModifierKeys::shiftModifier, 0 ) )
	{
		auto&	laf = static_cast<GUI_LookAndFeel&> ( getLookAndFeel () );

		// Toggle log-window
		const lime::LoggerOptions opts {
			.name = "ultraView",
			.settingsFolder = "ultraView",
			.font = laf.monoFontWithHeight ( 16.0f ),
		};

		auto& lw = lime::Logger::getInstance ()->getLoggingWindow ( opts );
		lw.setVisible ( ! lw.isVisible () );
	}
	else if ( const auto verb = shortcuts->find ( key ); verb.isNotEmpty () )
	{
		actionListenerCallback ( verb );
	}
	else
		return false;

	return true;
}
//-----------------------------------------------------------------------------

bool GUI_ultraView::canSendC64Keys () const
{
	return c64Keys && ! aboutScreen.isVisible () && ! shortcutsScreen.isVisible ();
}
//-----------------------------------------------------------------------------

bool GUI_ultraView::sendC64Key ( const juce::KeyPress& key )
{
	if ( ! canSendC64Keys () )
		return false;

	// Alt shortcuts (Alt+Enter) and Alt+F4 stay ultraView's / Windows'
	if ( key.getModifiers ().isAltDown () )
	{
		if ( key.isKeyCode ( juce::KeyPress::F4Key ) || shortcuts->find ( key ).isNotEmpty () )
			return false;

		altAlone = false;		// C= was used with a key
	}

	const auto	stroke = C64Keyboard::strokeFor ( key );

	if ( stroke.isEmpty () )
		return false;

	sendC64Stroke ( stroke );
	return true;
}
//-----------------------------------------------------------------------------

void GUI_ultraView::sendC64Stroke ( const C64Keyboard::Stroke& stroke )
{
	c64Keyboard.send ( stroke, settings->get<juce::String> ( "network/last-ip" ).trim (),
					   settings->get<juce::String> ( "network/password" ) );
}
//-----------------------------------------------------------------------------

void GUI_ultraView::modifierKeysChanged ( const juce::ModifierKeys& mods )
{
	// Alt as the Commodore key (AltGr comes as Ctrl+Alt: not C=)
	const auto	alt = mods.isAltDown () && ! mods.isCtrlDown () && ! mods.isCommandDown ();

	// Not while typing into one of ultraView's own text fields
	const auto	typing = dynamic_cast<juce::TextEditor*> ( getCurrentlyFocusedComponent () ) != nullptr;
	const auto	active = canSendC64Keys () && ! typing;

	if ( alt && ! altHeld )
	{
		altHeld = true;
		altAlone = active;
		altShiftSent = false;
	}

	if ( altHeld && alt && mods.isShiftDown () && ! altShiftSent && active )
	{
		sendC64Stroke ( C64Keyboard::commodoreStroke ( true ) );
		altShiftSent = true;
		altAlone = false;
	}

	if ( altHeld && ! alt )
	{
		altHeld = false;

		// Ctrl joined in (an ultraView shortcut), or Alt really went up
		if ( altAlone && active && ! mods.isCtrlDown () && ! mods.isCommandDown () )
			sendC64Stroke ( C64Keyboard::commodoreStroke ( false ) );

		altAlone = false;
	}

	juce::Component::modifierKeysChanged ( mods );
}
//-----------------------------------------------------------------------------

void GUI_ultraView::showC64KeysState ()
{
	juce::String	title;

	{
		const juce::ScopedLock	sl ( titleLock );
		title = connectedTitle;
	}

	setTitleStatus ( title + ( c64Keys ? " - keyboard to C64" : " - keyboard to ultraView" ) );
}
//-----------------------------------------------------------------------------
