#pragma once

#include <JuceHeader.h>

//-----------------------------------------------------------------------------

// One ICMP echo to the C64u, blocking up to timeoutMs. A C64u that refuses to
// start a stream to this machine ("Network Host Resolve Error") streams again
// after a ping from it, so the stream start pings first. True on a reply

[[ nodiscard ]] bool pingC64u ( const juce::String& ip, int timeoutMs );
