#pragma once

#include <JuceHeader.h>

#include "Config/Settings.h"
#include "Remote/C64uFtp.h"

//-----------------------------------------------------------------------------

// Browser for the C64u's own storage (SD, USB, Flash, Temp), listed over FTP.
// Enter on a file runs it on the C64u (GUI_ultraView::c64_runRemoteFile),
// Enter on a folder opens it, Backspace goes up, Esc closes, typing jumps to
// the first entry starting with the typed letters. Ctrl+O toggles it.

class GUI_C64Files final
	: public juce::Component
	, private juce::ListBoxModel
{
public:
	GUI_C64Files ();

	void open ();
	void close ();
	[[ nodiscard ]] bool isOpen () const { return opened; }
	void restoreFocus () { list.grabKeyboardFocus (); }

	// juce::Component
	void paint ( juce::Graphics& g ) override;
	void resized () override;
	bool keyPressed ( const juce::KeyPress& key ) override;

	// The extensions c64_runRemoteFile knows
	[[ nodiscard ]] static bool isRunnable ( const juce::String& name );

private:
	// juce::ListBoxModel
	int getNumRows () override;
	void paintListBoxItem ( int row, juce::Graphics& g, int width, int height, bool selected ) override;
	void returnKeyPressed ( int row ) override;
	void deleteKeyPressed ( int row ) override;
	void listBoxItemDoubleClicked ( int row, const juce::MouseEvent& ) override;

	void activate ( int row );
	void navigate ( const juce::String& newPath, const juce::String& selectName = {} );
	void goUp ();
	void listed ( const juce::String& listedPath, const juce::String& error, std::vector<C64uFtp::Entry> entries, const juce::String& selectName );

	bool hasParentRow () const { return path != "/"; }
	const C64uFtp::Entry* entryForRow ( int row ) const;

	bool			opened = false;
	juce::String	path = "/";
	juce::String	status;
	bool			statusIsError = false;
	bool			loading = false;
	int				generation = 0;

	std::vector<C64uFtp::Entry>	entries;
	juce::ListBox				list { "c64-files-list", this };

	// Type-ahead
	juce::String	typed;
	juce::uint32	lastTypedMs = 0;

	juce::SharedResourcePointer<Settings>	settings;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_C64Files )
};
//-----------------------------------------------------------------------------
