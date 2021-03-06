//=============================================================================
//  MuseScore
//  Music Composition & Notation
//
//  Copyright (C) 2020 MuseScore BVBA and others
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License version 2.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//=============================================================================

#ifndef MU_AUDIO_MIDIPLAYER_H
#define MU_AUDIO_MIDIPLAYER_H

#include <memory>
#include <vector>
#include <map>
#include <cstdint>
#include <functional>
#include <mutex>

#include "imidiplayer.h"
#include "modularity/ioc.h"
#include "async/asyncable.h"
#include "isynthesizersregister.h"
#include "midi/imidiportdatasender.h"

namespace mu::audio {
class MIDIPlayer : public IMIDIPlayer, public async::Asyncable
{
    INJECT(audio, synth::ISynthesizersRegister, synthesizersRegister)
    INJECT(audio, midi::IMidiPortDataSender, midiPortDataSender)

public:
    MIDIPlayer();
    ~MIDIPlayer() override;

    // IPlayer
    Status status() const override;
    async::Channel<Status> statusChanged() const override;

    bool isRunning() const override;

    void run() override;
    void seek(unsigned long milliseconds) override;
    void stop() override;
    void pause() override;

    unsigned long milliseconds() const override;
    void forwardTime(unsigned long milliseconds) override;

    // IMIDIPlayer
    void loadMIDI(const std::shared_ptr<midi::MidiStream>& stream) override;
    async::Channel<midi::tick_t> tickPlayed() const override;

    float playbackSpeed() const override;
    void setPlaybackSpeed(float speed) override;

    void setIsTrackMuted(midi::track_t trackIndex, bool mute) override;
    void setTrackVolume(midi::track_t trackIndex, float volume) override;
    void setTrackBalance(midi::track_t trackIndex, float balance) override;

private:

    void setStatus(const Status& status);

    void checkPosition();

    midi::tick_t validChunkTick(midi::tick_t fromTick, const midi::Chunks& chunks, midi::tick_t maxDistanceTick) const;
    bool sendEvents(midi::tick_t fromTick, midi::tick_t toTick);
    void sendClear();

    synth::ISynthesizerPtr determineSynthesizer(midi::channel_t ch, const synth::SynthMap& synthmap) const;
    synth::ISynthesizerPtr synth(midi::channel_t ch) const;

    void buildTempoMap();
    void setupChannels();

    void setCurrentMSec(uint64_t msec);
    midi::tick_t tick(uint64_t msec) const;

    bool hasTrack(midi::track_t num) const;

    void requestData(midi::tick_t tick);
    void onChunkReceived(const midi::Chunk& chunk);

    Status m_status = Status::Stoped;
    async::Channel<Status> m_statusChanged;

    std::mutex m_dataMutex;
    midi::MidiData m_midiData;
    std::shared_ptr<midi::MidiStream> m_midiStream = nullptr;
    std::map<uint8_t, midi::Event> m_noteCache = {};

    float m_playSpeed = 1.f;

    midi::msec_t m_prevMSec = 0;
    midi::msec_t m_curMSec = 0;

    bool m_isPlayTickSet = false;
    midi::tick_t m_playTick = 0;    //! NOTE First event tick

    struct TempoItem {
        midi::tempo_t tempo = 500000;
        midi::tick_t startTicks = 0;
        uint64_t startMsec = 0;
        double onetickMsec = 0.0;
    };
    std::map<uint64_t /*msec*/, TempoItem> m_tempoMap = {};

    struct StreamState {
        std::atomic<bool> requested{ false };
        void reset() { requested = false; }
    };
    StreamState m_streamState;

    struct ChanState {
        bool muted = false;
    };
    std::map<midi::channel_t, ChanState> m_chanStates;

    struct SynthState {
        std::set<midi::channel_t> channels;
        synth::ISynthesizerPtr synth;
        std::vector<float> buf;
    };
    std::vector<SynthState> m_synthStates = {};
    midi::tick_t m_lastSentTick = -1;
    async::Channel<midi::tick_t> m_onTickPlayed;
};
}

#endif // MU_AUDIO_MIDIPLAYER_H
