#include "GUI_ultraView_logo.h"

#include "Globals/constants.h"

//-----------------------------------------------------------------------------

constexpr auto	hoverHueShift = -30.0f / 360.0f;

//-----------------------------------------------------------------------------

GUI_ultraView_logo::GUI_ultraView_logo ( const juce::String& name, const juce::String& resource )
	: juce::Button ( name )
{
	setMouseCursor ( juce::MouseCursor::PointingHandCursor );

	if ( resource.startsWith ( "text:" ) )
	{
		buildTextLogo ( resource.fromFirstOccurrenceOf ( "text:", false, false ) );
		return;
	}

	auto	[ logo, _ ] = UI::getSVG ( resource );

	if ( logo )
		collectPaths ( *logo, {} );
}
//-----------------------------------------------------------------------------

void GUI_ultraView_logo::buildTextLogo ( const juce::String& text )
{
	// The SVG logo's look from the app font (Figtree, OFL): black-weight
	// letters in the fill role over a thick rounded outline, which also drops
	// a little below them like the original's shadow edge
	constexpr auto	height = 100.0f;

	juce::GlyphArrangement	glyphs;
	glyphs.addLineOfText ( UI::font ( height, 900 ), text, 0.0f, 0.0f );

	juce::Path	letters;
	glyphs.createPath ( letters );

	const auto	strokeWidth = height * 0.16f;

	juce::Path	outline;
	juce::PathStrokeType ( strokeWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded ).createStrokedPath ( outline, letters );
	outline.addPath ( letters );

	// The drop: the same outline again, a third of its width lower
	juce::Path	drop ( outline );
	drop.applyTransform ( juce::AffineTransform::translation ( 0.0f, strokeWidth * 0.35f ) );
	outline.addPath ( drop );

	logoBounds = outline.getBounds ();

	// Outline first, the letters painted over it
	paths.push_back ( { UI::colors::logoOutline, std::move ( outline ) } );
	paths.push_back ( { UI::colors::logo, std::move ( letters ) } );
}
//-----------------------------------------------------------------------------

void GUI_ultraView_logo::paintButton ( juce::Graphics& g, const bool isHover, bool )
{
	auto	fit = juce::RectanglePlacement ( juce::RectanglePlacement::xLeft | juce::RectanglePlacement::yTop )
					.getTransformToFit ( logoBounds, getLocalBounds ().toFloat () );

	for ( const auto& p : paths )
	{
		auto	col = findColour ( p.roleId );

		if ( isHover )
			col = col.withRotatedHue ( hoverHueShift );

		g.setColour ( col );
		g.fillPath ( p.path, fit );
	}
}
//-----------------------------------------------------------------------------

void GUI_ultraView_logo::collectPaths ( const juce::Drawable& d, const juce::AffineTransform& parentTransform )
{
	if ( auto* shape = dynamic_cast<const juce::DrawableShape*> ( &d ) )
	{
		auto	p = shape->getOutlineAsPath ();
		p.applyTransform ( parentTransform );

		// The SVG's fill colors are role markers: red picks the outline role, everything else the logo fill
		const auto&	fill = shape->getFill ();
		const auto	outline = fill.isColour () && fill.colour == juce::Colours::red;

		logoBounds = logoBounds.getUnion ( p.getBounds () );

		paths.push_back ( { outline ? UI::colors::logoOutline : UI::colors::logo, std::move ( p ) } );
		return;
	}

	if ( auto* composite = dynamic_cast<const juce::DrawableComposite*> ( &d ) )
	{
		auto	t = composite->getDrawableTransform ().followedBy ( parentTransform );

		for ( auto i = 0; i < composite->getNumChildren (); ++i )
			collectPaths ( composite->getChild ( i ), t );
	}
}
//-----------------------------------------------------------------------------
