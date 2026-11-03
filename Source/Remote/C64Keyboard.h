#pragma once

#include <JuceHeader.h>

#include <deque>

//-----------------------------------------------------------------------------

// Remote keyboard: keys typed in ultraView go into the C64's KERNAL keyboard
// buffer ($0277, count $C6) through the C64u REST API. Each key waits until
// the buffer is empty (the running program took the last one), then writes
// the key and the count with PUT machine:writemem, the bytes in the URL: no
// request body, so the C64u keeps no temp file per key.
//
// Works wherever the running program reads the keyboard buffer (BASIC, the
// KERNAL, programs that poll $C6 such as Ultimidi). Programs that scan the
// keyboard matrix themselves do not see these keys.

class C64Keyboard final : private juce::Thread
{
public:
	C64Keyboard ();
	~C64Keyboard () override;

	// The PETSCII code a key types on the C64, -1 when it has none (or a
	// Ctrl / Alt / Cmd combination: those stay ultraView's)
	[[ nodiscard ]] static int petsciiFor ( const juce::KeyPress& key );

	// Queue a key for the C64u at ip
	void send ( int petscii, const juce::String& ip, const juce::String& password );

private:
	void run () override;

	// One REST request; the reply body into reply when given. True on HTTP 200
	bool request ( const juce::String& ip, const juce::String& password, const juce::String& method,
				   const juce::String& endpoint, const juce::StringArray& params, juce::MemoryBlock* reply );

	struct Key
	{
		uint8_t			code = 0;
		juce::String	ip;
		juce::String	password;
	};

	juce::CriticalSection	lock;
	std::deque<Key>			queue;
	juce::WaitableEvent		wakeUp;

	// Longest wait for the program to take the previous key; past it the
	// queued keys are dropped (it does not read the buffer)
	static constexpr juce::uint32	maxWaitMs = 1000;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( C64Keyboard )
};
//-----------------------------------------------------------------------------
