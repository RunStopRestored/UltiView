#pragma once

#include <JuceHeader.h>

#include <vector>

//-----------------------------------------------------------------------------

// Directory listing of the C64u's file system (SD, USB0, Flash, Temp) over
// its FTP server; the REST API has no listing. Blocking: call it off the
// message thread. Passive mode, MLSD (the server announces it in FEAT).

namespace C64uFtp
{
	struct Entry
	{
		juce::String	name;
		bool			isDir = false;
		juce::int64		size = 0;
	};

	// Lists path ("/", "/SD/games"); directories first, then files, each
	// sorted by name. Returns an empty string or the error text
	[[ nodiscard ]] juce::String list ( const juce::String& ip, const juce::String& password,
										const juce::String& path, std::vector<Entry>& entries );
}
//-----------------------------------------------------------------------------
