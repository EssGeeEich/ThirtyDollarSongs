QT -= gui

CONFIG += c++17 console
CONFIG -= app_bundle

defines += USE_STL=true

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
        Midi-Parser/lib/BaseChunk.cpp \
        Midi-Parser/lib/Event.cpp \
        Midi-Parser/lib/HeaderChunk.cpp \
        Midi-Parser/lib/MTrkEvent.cpp \
        Midi-Parser/lib/MetaEvent.cpp \
        Midi-Parser/lib/Midi.cpp \
        Midi-Parser/lib/MidiEvent.cpp \
        Midi-Parser/lib/ReadNumber.cpp \
        Midi-Parser/lib/SysExEvent.cpp \
        Midi-Parser/lib/TrackChunk.cpp \
        Midi-Parser/lib/VLQ.cpp \
        main.cpp

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

HEADERS += \
	Midi-Parser/lib/BaseChunk.h \
	Midi-Parser/lib/Event.h \
	Midi-Parser/lib/HeaderChunk.h \
	Midi-Parser/lib/MTrkEvent.h \
	Midi-Parser/lib/MetaEvent.h \
	Midi-Parser/lib/Midi.h \
	Midi-Parser/lib/MidiEvent.h \
	Midi-Parser/lib/Options.h \
	Midi-Parser/lib/ReadNumber.h \
	Midi-Parser/lib/SysExEvent.h \
	Midi-Parser/lib/TrackChunk.h \
	Midi-Parser/lib/VLQ.h \
	Midi-Parser/lib/list.h \
	Midi-Parser/lib/types.h
