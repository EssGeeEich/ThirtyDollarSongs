#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <list>
#include <iomanip>
#include <map>

#include "Midi-Parser/lib/Midi.h"

// #define DEBUG_MIDI
// #define NO_LAG_MITIGATION

inline std::uint64_t readFromBE(std::uint8_t const* data, std::size_t size)
{
	std::uint64_t val = 0;
	for(std::size_t i = 0; i < size; ++i)
	{
		val = val | (data[i] << ((size - (i+1))*8));
	}
	return val;
}

std::basic_ostream<char>& thirtyArgSS(std::basic_ostream<char>& ss) {
	return ss;
}

template <typename T>
std::basic_ostream<char>& thirtyArgSS(std::basic_ostream<char>& ss, T&& arg) {
	return ss << "@" << std::forward<T>(arg);
}

template <typename T, typename ... Args>
std::basic_ostream<char>& thirtyArgSS(std::basic_ostream<char>& ss, T&& arg, Args&& ... args) {
	ss << "@" << std::forward<T>(arg);
	return thirtyArgSS(ss, std::forward<Args>(args)...);
}

template <typename ... Args>
std::string thirtyArg(char const* type, Args&& ... args) {
	std::ostringstream ss;
	
	ss << type;
	thirtyArgSS(ss, std::forward<Args>(args)...);
	return ss.str();
}

std::string convertToThirty(Midi const& midi, std::map<std::uint16_t, std::string> const& instrumentMap, std::map<std::string, int> const& availableInstruments) {
	HeaderChunk const& header = midi.getHeader();
	if(header.getDivision() & 0x8000)
		return std::string(); // Time division is FPS: will not parse.
	
	std::list<TrackChunk> const& tracks = midi.getTracks();
	
	std::uint64_t tempo = 120;
	
	TrackChunk const& typicalMetaChunk = tracks.front();
	for(MTrkEvent const& typicalMetaEvent : typicalMetaChunk.getEvents()) {
		Event const* event = typicalMetaEvent.getEvent();
		if(event->getType() != MidiType::EventType::MetaEvent)
			continue;
		
		MetaEvent const* metaEvent = static_cast<MetaEvent const*>(event);
		if(metaEvent->getStatus() == MidiType::MetaMessageStatus::SetTempo) {
			tempo = 60000000 / readFromBE(metaEvent->getData(), metaEvent->getLength());
		}
	}
	
	std::map<std::uint32_t, std::list<std::string>> actions;
	
	actions[0].push_back(thirtyArg("!looptarget"));
	actions[0].push_back(thirtyArg("!speed", static_cast<int>(tempo * header.getDivision())));
	
	int iTrackIx = -1;
	for(auto const& track : tracks) {
		++iTrackIx;
		std::string trackInstrument;
		int trackNoteOffset = 0;
		
		
		std::uint32_t iTime = 0;
		
		for(MTrkEvent const& trackEvent : track.getEvents())
		{
			iTime += trackEvent.getDeltaTime().getData();
			
			Event const* baseEvent = trackEvent.getEvent();
			if(baseEvent->getType() != MidiType::EventType::MidiEvent)
				continue;
			
			MidiEvent const* midiEvent = static_cast<MidiEvent const*>(baseEvent);
			if(midiEvent->getStatus() == MidiType::MidiMessageStatus::ProgramChange) {
				auto newInstrument = instrumentMap.find(midiEvent->getData());
				auto newMappedInstrument = availableInstruments.end();
				
				if(newInstrument != instrumentMap.end()) {
					newMappedInstrument = availableInstruments.find(newInstrument->second);
				}
				
				if(newMappedInstrument == availableInstruments.end()) {
					trackInstrument = std::string();
					trackNoteOffset = 0;
					
					std::cerr << "W: Unknown instrument 0x" << std::hex << midiEvent->getData() << std::dec
							  << " found on track " << iTrackIx << " at time " << (iTime / header.getDivision()) << std::endl;
				}
				else {
					trackInstrument = newMappedInstrument->first;
					trackNoteOffset = newMappedInstrument->second;
				}
			}
			
			if(trackInstrument.empty())
				continue;
			
			if(midiEvent->getStatus() == MidiType::MidiMessageStatus::NoteOn && midiEvent->getVelocity() > 0) {
				actions[iTime].push_back(thirtyArg(trackInstrument.c_str(), static_cast<int>(midiEvent->getNote() + trackNoteOffset)));
			}
		}
	}
	
	std::string output;
	std::size_t outputSize = 1;
	
	for(auto const& timeActions : actions) {
		std::size_t iCombines = 0;
		if(timeActions.second.size() > 1)
			iCombines = timeActions.second.size() - 1;
		
		for(auto const& action : timeActions.second) {
			outputSize += action.size() + 1;
			
			if(iCombines--)
				outputSize += 9; // |!combine
		}
	}
	
	output.reserve(outputSize + (actions.size() * 10)); // Guesstimate !stop's
	
	
	std::uint32_t lastActionTime = 0;
	
	for(auto const& timeActions : actions) {
		std::size_t iCombines = 0;
		if(timeActions.second.size() > 1)
			iCombines = timeActions.second.size() - 1;
	
		if(timeActions.first > 0) {
			std::size_t timeDelta = (timeActions.first - lastActionTime);
			
			lastActionTime = timeActions.first;
			
#ifndef NO_LAG_MITIGATION
			// Site lag mitigation
			timeDelta = timeDelta * 3 / 4;
#endif
			
			if(timeDelta > 1) {
				--timeDelta;
				if(output.size() > 0) {
					// My personal feeling tells me that !stop takes more time than _pause,
					// but _pause makes the website take much more time to load.
					// This should give us the best of both worlds.
					if(timeDelta > 5) {
						output.append("|!stop@");
						output.append(std::to_string(timeDelta));
					} else {
						output.append("|_pause=");
						output.append(std::to_string(timeDelta));
					}
				}
			}
		}
		
		for(auto const& action : timeActions.second) {
			if(!output.empty())
				output.append("|");
			output += action;
			if(iCombines--)
				output += "|!combine";
		}
	}
	
	return output;
}

int main(int argc, char *argv[])
{
	if(argc != 2 && argc != 3) {
		std::cerr << "Usage: " << argv[0] << " <input file> [output file]" << std::endl;
		return 1;
	}
	
	Midi f(argv[1]);
	
#ifdef DEBUG_MIDI
	auto& header = f.getHeader();
	auto& tracks = f.getTracks();
	
	std::cout << "File read" << std::endl;
	std::cout << "File contents:" << std::endl;
	std::cout << "	Header:" << std::endl;
	std::cout << "		File format: " << (int)header.getFormat() <<
		 "\n		Number of tracks: " << header.getNTracks() <<
		 "\n		Time division: " << header.getDivision() << std::endl;

	for (const auto& track : tracks) {
		std::cout << "	Track events:" << std::endl;
		auto& events = track.getEvents();

		for (const auto& trackEvent : events) {
			auto* event = trackEvent.getEvent();
			uint8_t* data;

			if (event->getType() == MidiType::EventType::MidiEvent) {
				auto* midiEvent = (MidiEvent*)event;

				auto status = midiEvent->getStatus();

				if (status == MidiType::MidiMessageStatus::NoteOn) {
					continue;
					//std::cout << "		Midi event:" << std::endl;
					//std::cout << "\t\t\t" << "Note " << (midiEvent->getVelocity()? "on " : "off ")
						 //<< (int)midiEvent->getNote()
						 //<< " on channel " << (int)midiEvent->getChannel() << std::endl;
				} else if (status == MidiType::MidiMessageStatus::NoteOff) {
					continue;
					//std::cout << "		Midi event:" << std::endl;
					//std::cout << "\t\t\t" << "Note off "
						 //<< (int)midiEvent->getNote()
						 //<< " on channel " << (int)midiEvent->getChannel() << std::endl;
				} else {
					std::cout << "		Midi event:" << std::endl;
					std::cout << "\t\t\tStatus: 0x" << std::hex << (int)midiEvent->getStatus() << std::endl
						 << "\t\t\tData: 0x" << midiEvent->getData() << std::dec << std::endl;
				}
			} else if (event->getType() == MidiType::EventType::SysExEvent) {
				std::cout << "\t\tSystem exclusive event:" << std::endl
					 << "\t\t\tStatus: 0x" << std::hex
					 << ((MetaEvent*)event)->getStatus() << std::endl
					 << "\t\t\tData: 0x";

				data = ((MetaEvent*)event)->getData();
				for (int i {0}; i < ((MetaEvent*)event)->getLength(); ++i) {
					std::cout << (int)data[i];
				}
				std::cout << std::dec << std::endl;
			} else {  // event->getType() == MidiType::EventType::MetaEvent
				std::cout << "\t\tMeta event:" << std::endl;
				std::cout << "\t\t\tMeta Length: " << std::dec << (int)((MetaEvent*)event)->getLength() << std::endl;
				std::cout << "\t\t\tStatus: 0x" << std::hex << ((MetaEvent*)event)->getStatus() << std::endl;
				
				if (((MetaEvent*)event)->getLength()) {
					std::cout << "\t\t\tData: 0x";

					data = ((MetaEvent*)event)->getData();
					for (int i {0}; i < ((MetaEvent*)event)->getLength(); ++i) {
						std::cout << std::hex << std::setfill('0') << std::setw(2) << std::right << (int)data[i];
					}
				}
				std::cout << std::dec << std::endl;
			}

			std::cout << "\t\t\tLength: " << trackEvent.getDeltaTime().getData() << std::endl;
		}
	}	
#endif
	
	// Todo: Complete and improve the pitch of the sounds
	std::map<std::string, int> availableInstruments = {
		{"🦴", -86},
		{"ultrainstinct", -78},
		{"smw_kick", -77},
		{"smw_coin", -76},
		{"smw_stomp", -76},
		{"yahoo", -76},
		{"toby", -76},
		{"slip", -73},
		{"tab_sounds", -72},
		{"smw_stomp2", -72},
		{"shaker", -72},
		{"bup", -69},
		{"op", -69},
		{"smw_spinjump", -68},
		{"ride2", -68},
		{"❗", -67},
		{"🔔", -67},
		{"smm_scream", -67},
		
		{"hoenn", -66},
		{"👽", -66},
		{"🤬", -66},
		{"buzzer", -66},
		{"🤬", -66},
		{"🦀", -66},
		{"🦢", -66},
		{"hitmarker", -66},
		{"👌", -66},
		{"gnome", -66},
		{"whipcrack", -66},
		{"oof", -66},
		{"yoda", -66},
		{"morshu", -66},
		{"mariopaint_mario", -66},
		{"mariopaint_luigi", -66},
		{"mariopaint_star", -66},
		{"mariopaint_flower", -66},
		{"mariopaint_swan", -66},
		{"mariopaint_plane", -66},
		{"mariopaint_car", -66},
		{"noteblock_harp", -66},
		{"noteblock_bass", -66},
		{"noteblock_snare", -66},
		{"noteblock_click", -66},
		{"noteblock_bell", -66},
		{"noteblock_chime", -66},
		{"noteblock_banjo", -66},
		{"noteblock_pling", -66},
		{"noteblock_xylophone", -66},
		{"noteblock_bit", -66},
		{"minecraft_explosion", -66},
		{"minecraft_bell", -66},
		{"fnf_left", -66},
		{"fnf_down", -66},
		{"fnf_up", -66},
		{"fnf_right", -66},
		{"🎺", -66},
		{"🌟", -66},
		{"choruskid", -66},
		{"ook", -66},
		{"samurai", -66},
		{"otto_on", -66},
		{"mariopaint_gameboy", -66},
		{"terraria_pot", -66},
		{"terraria_reforge", -66},
		{"flipnote", -66},
		
		{"explosion", -65},
		{"mariopaint_dog", -65},
		{"mariopaint_cat", -65},
		{"💨", -65},
		{"🏏", -64},
		{"amogus", -63},
		{"undertale_crack", -62},
		{"undertale_hit", -62},
		{"midspin", -62},
		{"tonk", -62},
		{"e", -61},
		{"sidestick", -61},
		{"amongdrip", -60},
		{"celeste_diamond", -60},
		{"🚫", -60},
		{"🥁", -58},
		{"hammer", -56},
		{"🪘", -51},
		{"bwomp", -42},
	};
	
	// Todo: Implement more midi instruments
	// This can be done by analyzing the stderr of this program.
	// It will output a warning when it can't find a mapped instrument.
	std::map<std::uint16_t, std::string> instrumentMap = {
		{0x003C, "hoenn"},
		{0x003A, "hoenn"},
		{0x0030, "hoenn"},
		{0x002E, "hoenn"},
		{0x0038, "hoenn"},
		{0x000E, "bwomp"},
		{0x0050, "mariopaint_gameboy"},
		{0x0004, "mariopaint_gameboy"},
		{0x0005, "tab_sounds"},
		{0x0051, "bwomp"},
		{0x002F, "🪘"},
		{0x0049, "🦴"},
	};
	
	
	
	std::string thirty = convertToThirty(f, instrumentMap, availableInstruments);
	if(thirty.empty())
		return 1;
	
	if(argc == 2) {
		std::cout << thirty << std::endl;
		return 0;
	}
	else if(argc == 3) {
		std::ofstream out(argv[2]);
		if(out) {
			out << thirty << std::endl;
			return 0;
		}
	}
	
	return 1;
}
