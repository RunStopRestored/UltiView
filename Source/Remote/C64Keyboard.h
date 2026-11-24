#pragma once

#include <JuceHeader.h>

#include <deque>

//-----------------------------------------------------------------------------

// Remote keyboard: keys typed in ultraView go to the C64 through the C64u REST
// API, in one of two ways, picked per C64u:
//
// - Keyboard matrix (newer firmware): POST machine:input with a JSON list of
//   "tap" events naming the C64 keys (["left_shift", "a"]). The firmware
//   presses them in the matrix, so every program sees them, the Ultimate's
//   own menu included (it no longer closes on the first key). Probed once per
//   C64u with GET machine:input: 200 = there, 404 = older firmware.
//
// - Keyboard buffer (fallback): each key waits until the KERNAL keyboard
//   buffer ($0277, count $C6) is empty, then writes the key and the count
//   with PUT machine:writemem, the bytes in the URL: no request body, so the
//   C64u keeps no temp file per key. Only programs that read the buffer see
//   these keys (BASIC, the KERNAL, Ultimidi).
//
// Alt is the Commodore key and Page Up is RESTORE (as in VICE). Neither has a
// PETSCII code, so they work with the keyboard matrix only.

class C64Keyboard final : private juce::Thread
{
public:
	C64Keyboard ();
	~C64Keyboard () override;

	// One C64 key press: the PETSCII code for the keyboard buffer (-1: none)
	// and the C64 keys (machine:input names) for the matrix (empty: none).
	// menuButton: C= + RESTORE, which presses the Ultimate's menu button
	// (PUT machine:menu_button) instead: the firmware takes an injected
	// C= + RESTORE but does not open its menu for it
	struct Stroke
	{
		int					petscii = -1;
		juce::StringArray	matrix;
		bool				menuButton = false;

		[[ nodiscard ]] bool isEmpty () const	{ return petscii < 0 && matrix.isEmpty () && ! menuButton; }
	};

	// What a PC key types on the C64; empty when nothing (Ctrl / Cmd
	// combinations stay ultraView's)
	[[ nodiscard ]] static Stroke strokeFor ( const juce::KeyPress& key );

	// The Commodore key alone, or with SHIFT (switches the character set)
	[[ nodiscard ]] static Stroke commodoreStroke ( bool withShift );

	// The PETSCII code a key types on the C64, -1 when it has none (or a
	// Ctrl / Alt / Cmd combination)
	[[ nodiscard ]] static int petsciiFor ( const juce::KeyPress& key );

	// Queue a key press for the C64u at ip
	void send ( const Stroke& stroke, const juce::String& ip, const juce::String& password );

	// One blocking REST request; the reply body into reply when given. True on
	// HTTP 200. Also used by C64DiskRunner
	static bool request ( const juce::String& ip, const juce::String& password, const juce::String& method,
						  const juce::String& endpoint, const juce::StringArray& params, juce::MemoryBlock* reply );

	// The same with an optional JSON body; the HTTP status, 0 when the C64u
	// did not answer
	static int requestStatus ( const juce::String& ip, const juce::String& password, const juce::String& method,
							   const juce::String& endpoint, const juce::StringArray& params, juce::MemoryBlock* reply,
							   const juce::String& jsonBody = {} );

	// The C64 keys (machine:input names) that type a PETSCII code; empty when
	// it has none
	[[ nodiscard ]] static juce::StringArray matrixKeysFor ( int petscii );

private:
	void run () override;

	enum class Mode { unknown, matrix, buffer };

	// Matrix or buffer for the C64u at ip (asks it the first time)
	Mode modeFor ( const juce::String& ip, const juce::String& password );

	void sendMatrix ( const juce::String& ip, const juce::String& password );
	void sendBuffer ( uint8_t code, const juce::String& ip, const juce::String& password );

	struct Key
	{
		Stroke			stroke;
		juce::String	ip;
		juce::String	password;
	};

	juce::CriticalSection	lock;
	std::deque<Key>			queue;
	juce::WaitableEvent		wakeUp;

	// The probed mode and the C64u it belongs to (the run thread only)
	juce::String			modeIp;
	Mode					mode = Mode::unknown;

	// Most tap events in one machine:input request (the firmware's limit)
	static constexpr int	maxEvents = 64;

	// Longest wait for the program to take the previous key; past it the
	// queued keys are dropped (it does not read the buffer)
	static constexpr juce::uint32	maxWaitMs = 1000;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( C64Keyboard )
};
//-----------------------------------------------------------------------------
