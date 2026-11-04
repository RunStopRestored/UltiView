#include "GUI_C64Files.h"

#include "Globals/constants.h"

//-----------------------------------------------------------------------------

namespace
{
	// The stream-pause banner's look, in neutral grey
	const auto	panelFill = juce::Colours::black.withAlpha ( 0.82f );
	const auto	panelBorder = juce::Colour ( 0xff9aa0a8 );
	const auto	textBright = juce::Colour ( 0xffeceff2 );
	const auto	textDim = juce::Colour ( 0xff8a9099 );
	const auto	errorText = juce::Colour ( 0xffe8a33c );

	constexpr auto	headerH = 40;
	constexpr auto	footerH = 32;
	constexpr auto	margin = 12;
	constexpr auto	rowH = 26;

	juce::String formatSize ( const juce::int64 bytes )
	{
		// The C64 way, 254-byte blocks, next to the bytes
		const auto	blocks = ( bytes + 253 ) / 254;

		if ( bytes < 10000 )
			return juce::String ( bytes ) + " B  (" + juce::String ( blocks ) + " blk)";

		if ( bytes < 10 * 1024 * 1024 )
			return juce::String ( ( bytes + 512 ) / 1024 ) + " KB  (" + juce::String ( blocks ) + " blk)";

		return juce::String ( ( bytes + 512 * 1024 ) / ( 1024 * 1024 ) ) + " MB";
	}
}
//-----------------------------------------------------------------------------

GUI_C64Files::GUI_C64Files ()
	: juce::Component ( "c64-files" )
{
	list.setRowHeight ( rowH );
	list.setColour ( juce::ListBox::backgroundColourId, juce::Colours::transparentBlack );
	list.setColour ( juce::ListBox::outlineColourId, juce::Colours::transparentBlack );
	list.setOutlineThickness ( 0 );
	list.setWantsKeyboardFocus ( true );
	addAndMakeVisible ( list );

	path = settings->get<juce::String> ( "c64files/path" );
	if ( ! path.startsWithChar ( '/' ) )
		path = "/";
}
//-----------------------------------------------------------------------------

void GUI_C64Files::open ()
{
	opened = true;
	setVisible ( true );
	toFront ( false );
	list.grabKeyboardFocus ();

	navigate ( path );
}
//-----------------------------------------------------------------------------

void GUI_C64Files::close ()
{
	opened = false;
	setVisible ( false );

	// Hand the keyboard back to the app (and so to the C64): the nearest
	// focusable parent, keys bubble up from there
	for ( auto p = getParentComponent (); p != nullptr; p = p->getParentComponent () )
	{
		if ( p->getWantsKeyboardFocus () )
		{
			p->grabKeyboardFocus ();
			break;
		}
	}
}
//-----------------------------------------------------------------------------

bool GUI_C64Files::isRunnable ( const juce::String& name )
{
	const auto	ext = name.fromLastOccurrenceOf ( ".", false, false ).toLowerCase ();

	return juce::StringArray { "prg", "crt", "sid", "mod", "d64", "g64", "d71", "g71", "d81" }.contains ( ext );
}
//-----------------------------------------------------------------------------

void GUI_C64Files::paint ( juce::Graphics& g )
{
	const auto	area = getLocalBounds ().toFloat ().reduced ( 1.0f );

	g.setColour ( panelFill );
	g.fillRoundedRectangle ( area, 8.0f );

	g.setColour ( panelBorder );
	g.drawRoundedRectangle ( area, 8.0f, 2.0f );

	auto	bounds = getLocalBounds ().reduced ( margin, 0 );

	// Header: title and path
	{
		auto	header = bounds.removeFromTop ( headerH );

		g.setColour ( textBright );
		g.setFont ( juce::FontOptions ( 18.0f, juce::Font::bold ) );
		g.drawText ( "C64U FILES", header.removeFromLeft ( 120 ), juce::Justification::centredLeft );

		g.setColour ( textDim );
		g.setFont ( juce::FontOptions ( 15.0f ) );
		g.drawText ( path, header, juce::Justification::centredRight, true );

		g.setColour ( panelBorder.withAlpha ( 0.4f ) );
		g.fillRect ( bounds.getX (), headerH - 1, bounds.getWidth (), 1 );
	}

	// Footer: status or the keys
	{
		auto	footer = getLocalBounds ().reduced ( margin, 0 ).removeFromBottom ( footerH );

		g.setColour ( panelBorder.withAlpha ( 0.4f ) );
		g.fillRect ( footer.getX (), footer.getY (), footer.getWidth (), 1 );

		const auto	text = status.isNotEmpty () ? status
												: juce::String ( "Enter: run / open    Backspace: up    Esc: close" );

		g.setColour ( statusIsError ? errorText : textDim );
		g.setFont ( juce::FontOptions ( 14.0f ) );
		g.drawText ( text, footer, juce::Justification::centred, true );
	}
}
//-----------------------------------------------------------------------------

void GUI_C64Files::resized ()
{
	list.setBounds ( getLocalBounds ().reduced ( margin, 0 ).withTrimmedTop ( headerH + 4 ).withTrimmedBottom ( footerH + 4 ) );
}
//-----------------------------------------------------------------------------

bool GUI_C64Files::keyPressed ( const juce::KeyPress& key )
{
	const auto	mods = key.getModifiers ();

	// Shortcuts (Ctrl+O closes) stay the app's
	if ( mods.isCtrlDown () || mods.isAltDown () || mods.isCommandDown () )
		return false;

	if ( key.isKeyCode ( juce::KeyPress::escapeKey ) )
	{
		close ();
		return true;
	}

	if ( key.isKeyCode ( juce::KeyPress::backspaceKey ) || key.isKeyCode ( juce::KeyPress::leftKey ) )
	{
		goUp ();
		return true;
	}

	if ( key.isKeyCode ( juce::KeyPress::rightKey ) )
	{
		if ( const auto e = entryForRow ( list.getSelectedRow () ); e && e->isDir )
			activate ( list.getSelectedRow () );

		return true;
	}

	// Type-ahead: the first entry starting with the letters typed so far
	if ( const auto c = key.getTextCharacter (); c >= ' ' )
	{
		const auto	now = juce::Time::getMillisecondCounter ();
		if ( now - lastTypedMs > 1000 )
			typed.clear ();

		lastTypedMs = now;
		typed += juce::String::charToString ( c );

		for ( auto row = 0; row < getNumRows (); ++row )
		{
			if ( const auto e = entryForRow ( row ); e && e->name.startsWithIgnoreCase ( typed ) )
			{
				list.selectRow ( row );
				break;
			}
		}
	}

	// Nothing typed here reaches the C64
	return true;
}
//-----------------------------------------------------------------------------

int GUI_C64Files::getNumRows ()
{
	return static_cast<int> ( entries.size () ) + ( hasParentRow () ? 1 : 0 );
}
//-----------------------------------------------------------------------------

const C64uFtp::Entry* GUI_C64Files::entryForRow ( const int row ) const
{
	const auto	index = row - ( hasParentRow () ? 1 : 0 );

	if ( index < 0 || index >= static_cast<int> ( entries.size () ) )
		return nullptr;

	return &entries[ static_cast<size_t> ( index ) ];
}
//-----------------------------------------------------------------------------

void GUI_C64Files::paintListBoxItem ( const int row, juce::Graphics& g, const int width, const int height, const bool selected )
{
	if ( selected )
	{
		g.setColour ( juce::Colours::white.withAlpha ( 0.14f ) );
		g.fillRoundedRectangle ( 0.0f, 1.0f, static_cast<float> ( width ), static_cast<float> ( height - 2 ), 4.0f );
	}

	auto	area = juce::Rectangle<int> ( 0, 0, width, height ).reduced ( 8, 0 );

	if ( hasParentRow () && row == 0 )
	{
		g.setColour ( textDim );
		g.setFont ( juce::FontOptions ( 15.0f ) );
		g.drawText ( ".. (up)", area, juce::Justification::centredLeft );
		return;
	}

	const auto	e = entryForRow ( row );
	if ( ! e )
		return;

	if ( e->isDir )
	{
		g.setColour ( textBright );
		g.setFont ( juce::FontOptions ( 15.0f, juce::Font::bold ) );
		g.drawText ( e->name + "/", area, juce::Justification::centredLeft, true );
		return;
	}

	g.setFont ( juce::FontOptions ( 14.0f ) );
	g.setColour ( textDim );
	g.drawText ( formatSize ( e->size ), area.removeFromRight ( 150 ), juce::Justification::centredRight );

	g.setFont ( juce::FontOptions ( 15.0f ) );
	g.setColour ( isRunnable ( e->name ) ? textBright : textDim.withAlpha ( 0.7f ) );
	g.drawText ( e->name, area, juce::Justification::centredLeft, true );
}
//-----------------------------------------------------------------------------

void GUI_C64Files::returnKeyPressed ( const int row )
{
	activate ( row );
}
//-----------------------------------------------------------------------------

void GUI_C64Files::deleteKeyPressed ( const int )
{
	goUp ();
}
//-----------------------------------------------------------------------------

void GUI_C64Files::listBoxItemDoubleClicked ( const int row, const juce::MouseEvent& )
{
	activate ( row );
}
//-----------------------------------------------------------------------------

void GUI_C64Files::activate ( const int row )
{
	if ( loading )
		return;

	if ( hasParentRow () && row == 0 )
		return goUp ();

	const auto	e = entryForRow ( row );
	if ( ! e )
		return;

	const auto	fullPath = ( path.endsWithChar ( '/' ) ? path : path + "/" ) + e->name;

	if ( e->isDir )
		return navigate ( fullPath );

	if ( ! isRunnable ( e->name ) )
	{
		status = "Cannot run " + e->name.fromLastOccurrenceOf ( ".", true, false ) + " files";
		statusIsError = true;
		repaint ();
		return;
	}

	UI::sendGlobalMessage ( "c64runremote {}", fullPath.quoted () );
	close ();
}
//-----------------------------------------------------------------------------

void GUI_C64Files::goUp ()
{
	if ( ! hasParentRow () || loading )
		return;

	const auto	trimmed = path.trimCharactersAtEnd ( "/" );
	const auto	parent = trimmed.upToLastOccurrenceOf ( "/", false, false );

	navigate ( parent.isEmpty () ? "/" : parent, trimmed.fromLastOccurrenceOf ( "/", false, false ) );
}
//-----------------------------------------------------------------------------

void GUI_C64Files::navigate ( const juce::String& newPath, const juce::String& selectName )
{
	const auto	ip = settings->get<juce::String> ( "network/last-ip" ).trim ();
	const auto	password = settings->get<juce::String> ( "network/password" );

	if ( ip.isEmpty () )
	{
		status = "No C64 Ultimate found yet";
		statusIsError = true;
		repaint ();
		return;
	}

	loading = true;
	status = "Loading " + newPath + " ...";
	statusIsError = false;
	repaint ();

	const auto	gen = ++generation;

	juce::Thread::launch ( [ safe = juce::Component::SafePointer<GUI_C64Files> ( this ), gen, ip, password, newPath, selectName ]
	{
		std::vector<C64uFtp::Entry>	result;
		const auto	error = C64uFtp::list ( ip, password, newPath, result );

		juce::MessageManager::callAsync ( [ safe, gen, newPath, error, result = std::move ( result ), selectName ] () mutable
		{
			if ( safe && safe->generation == gen )
				safe->listed ( newPath, error, std::move ( result ), selectName );
		} );
	} );
}
//-----------------------------------------------------------------------------

void GUI_C64Files::listed ( const juce::String& listedPath, const juce::String& error, std::vector<C64uFtp::Entry> newEntries, const juce::String& selectName )
{
	loading = false;

	if ( error.isNotEmpty () )
	{
		status = error;
		statusIsError = true;
		repaint ();

		// A folder that vanished (card pulled): fall back to the top
		if ( listedPath == path && path != "/" )
			path = "/";

		return;
	}

	path = listedPath;
	entries = std::move ( newEntries );
	status.clear ();
	statusIsError = false;

	settings->set ( "c64files/path", path );

	list.updateContent ();

	auto	selectRow = hasParentRow () ? 1 : 0;
	for ( auto row = 0; row < getNumRows () && selectName.isNotEmpty (); ++row )
	{
		if ( const auto e = entryForRow ( row ); e && e->name == selectName )
		{
			selectRow = row;
			break;
		}
	}

	if ( entries.empty () )
		selectRow = 0;

	list.selectRow ( selectRow );
	list.scrollToEnsureRowIsOnscreen ( selectRow );

	if ( entries.empty () )
		status = "Empty folder";

	repaint ();
}
//-----------------------------------------------------------------------------
