#pragma once

#include <JuceHeader.h>

//-----------------------------------------------------------------------------

class GUI_ultraView_logo final : public juce::Button
{
public:
	// resource: an SVG (red fills = outline role, the rest = logo fill), or
	// "text:<word>" for a logo set in the app font (UltiView's own name)
	GUI_ultraView_logo ( const juce::String& name, const juce::String& resource );

	// juce::Button
	void paintButton ( juce::Graphics& g, bool isHover, bool isDown ) override;

private:
	struct SvgOutline
	{
		int			roleId;
		juce::Path	path;
	};

	void collectPaths ( const juce::Drawable& d, const juce::AffineTransform& parentTransform );
	void buildTextLogo ( const juce::String& text );

	std::vector<SvgOutline>	paths;
	juce::Rectangle<float>	logoBounds;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_ultraView_logo )
};
//-----------------------------------------------------------------------------
