#pragma once

#include <JuceHeader.h>

//-----------------------------------------------------------------------------

// Runs a disk image from the C64u's own storage the way the Ultimate menu
// does: drive A set to the image's type and switched on, the image mounted,
// the machine reset, then at READY. the "dynamic keyboard": LOAD"*",8,1 is
// written onto the screen at the cursor row and the keyboard buffer gets
// RETURN plus RUN RETURN. The buffer survives the load, so RUN follows it.
// One run at a time; blocking REST calls on its own thread.

class C64DiskRunner final : private juce::Thread
{
public:
	C64DiskRunner ();
	~C64DiskRunner () override;

	// False when a run is still in progress. rebootFirst: a machine reboot
	// ahead of everything, it drops a cartridge a reset would restart
	bool run ( const juce::String& ip, const juce::String& password, const juce::String& imagePath, bool rebootFirst );

	// Called on the message thread when a run failed
	std::function<void ( const juce::String& error )>	onError;

	[[ nodiscard ]] static bool isDiskImage ( const juce::String& name );

private:
	void run () override;
	juce::String runSequence ();

	juce::String	ip, password, imagePath;
	bool			rebootFirst = false;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( C64DiskRunner )
};
//-----------------------------------------------------------------------------
