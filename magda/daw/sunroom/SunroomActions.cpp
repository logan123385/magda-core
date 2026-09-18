#include "SunroomActions.hpp"

#include <array>
#include <memory>

#include "audio/AudioBridge.hpp"
#include "audio/plugins/DrumGridPlugin.hpp"
#include "audio/plugins/InternalPluginRegistry.hpp"
#include "audio/plugins/compiled/CompiledPluginRegistry.hpp"
#include "core/AppPaths.hpp"
#include "core/ChainNodePath.hpp"
#include "core/ClipManager.hpp"
#include "core/ClipTypes.hpp"
#include "core/RackInfo.hpp"
#include "core/SelectionManager.hpp"
#include "core/TrackCommands.hpp"
#include "core/TrackManager.hpp"
#include "core/UndoManager.hpp"
#include "core/ViewModeController.hpp"
#include "engine/AudioEngine.hpp"
#include "project/ProjectManager.hpp"
#include "ui/state/TimelineController.hpp"
#include "../../agents/dsl_interpreter.hpp"
#include "api/magda_api.hpp"

#include <juce_audio_formats/juce_audio_formats.h>

namespace magda::sunroom {
DeviceInfo makeDevice(const juce::String& id, const juce::String& name, bool instrument,
                      const std::vector<std::pair<int, float>>& params) {
    DeviceInfo d;
    d.pluginId = id;
    d.uniqueId = id;
    d.fileOrIdentifier = id;
    d.name = name;
    d.manufacturer = "MAGDA / SUNROOM";
    d.format = PluginFormat::Internal;
    d.isInstrument = instrument;
    d.deviceType = instrument ? DeviceType::Instrument : DeviceType::Effect;
    d.canReceiveMidi = instrument;
    // Presets use the same real/display values as the device controls. The
    // processor converts them to its native parameter range during creation.
    // A DeviceState document instead stores native values (often normalized
    // 0..1); putting dB or milliseconds in that document silently mutes voices.
    for (auto [index, value] : params) {
        ParameterInfo parameter;
        parameter.paramIndex = index;
        parameter.currentValue = value;
        d.parameters.push_back(std::move(parameter));
    }
    return d;
}
namespace {
void installSound(TrackId id, int layer, const Options& o) {
    auto& tm = TrackManager::getInstance();
    const auto& l = layers[static_cast<size_t>(layer)];
    std::vector<std::pair<int, float>> p;
    if (layer == 0)
        p = {{0, 3},     {1, -8},    {4, 1},
             {5, -18},   {7, 7},     {17, 650 + (1 - o.warmth) * 2200},
             {18, .15f}, {24, 1400}, {25, 1200},
             {26, .75f}, {27, 3600}, {43, -8}};
    if (layer == 1)
        p = {{0, 0},   {1, -3},   {4, 3},    {5, -24},  {17, 280}, {18, .1f},
             {24, 15}, {25, 300}, {26, .7f}, {27, 180}, {43, -7}};
    tm.addDeviceToTrack(id, makeDevice(l.plugin, l.name, true, p));
    if (layer == 2 || layer == 6 || layer == 3) {
        tm.addDeviceToTrack(id, makeDevice("magda_delay", "Orbit / echoes", false,
                                           {{0, static_cast<float>(45000 / o.tempo)},
                                            {3, .26f + o.space * .18f},
                                            {4, layer == 3 ? .14f : .23f},
                                            {5, -.3f},
                                            {6, .7f}}));
    }
    if (layer == 0 || layer == 2 || layer == 6) {
        tm.addDeviceToTrack(id, makeDevice("magda_reverb", "Horizon / space", false,
                                           {{0, 1},
                                            {1, .20f + o.space * .24f},
                                            {2, 35},
                                            {3, 40 + o.space * 35},
                                            {4, 50},
                                            {5, 180},
                                            {6, 5500},
                                            {7, 120},
                                            {8, -2}}));
    }
    tm.setTrackColour(id, juce::Colour(l.colour));
    tm.setTrackVolume(id, l.volume);
}
class AddInstrumentCommand final : public UndoableCommand {
  public:
    AddInstrumentCommand(int layer, Options options, std::shared_ptr<TrackId> createdId)
        : layer_(layer), options_(options), createdId_(std::move(createdId)) {}
    void execute() override {
        auto& tm = TrackManager::getInstance();
        if (captured_) {
            tm.restoreTrack(track_);
            if (createdId_)
                *createdId_ = track_.id;
            return;
        }
        auto id = tm.createTrack(layers[static_cast<size_t>(layer_)].name, TrackType::Audio);
        installSound(id, layer_, options_);
        track_ = *tm.getTrack(id);
        if (createdId_)
            *createdId_ = track_.id;
        captured_ = true;
    }
    void undo() override {
        TrackManager::getInstance().deleteTrack(track_.id);
    }
    juce::String getDescription() const override {
        return "Add " + track_.name;
    }

  private:
    int layer_;
    Options options_;
    TrackInfo track_;
    std::shared_ptr<TrackId> createdId_;
    bool captured_ = false;
};
}  // namespace
TrackId addInstrument(int layer, const Options& options) {
    layer = juce::jlimit(0, 6, layer);
    // Publish the new track id through storage that outlives the command. When
    // undo history is capped at zero, executeCommand may destroy the command
    // before this function returns.
    auto createdId = std::make_shared<TrackId>(0);
    UndoManager::getInstance().executeCommand(
        std::make_unique<AddInstrumentCommand>(layer, sanitise(options), createdId));
    return *createdId;
}
CreateJourneyCommand::CreateJourneyCommand(Options options, AudioEngine* engine)
    : options_(sanitise(options)), engine_(engine) {}

void CreateJourneyCommand::applyProject(const ProjectInfo& info) {
    auto& pm = ProjectManager::getInstance();
    // Only the fields owned by this command. Preserve path/name and UI state
    // when undoing after a save or after the user changes their view.
    auto& live = pm.getMutableProjectInfo();
    live.keyRoot = info.keyRoot;
    live.keyQuality = info.keyQuality;
    live.sunroomMood = info.sunroomMood;
    live.sunroomGuide = info.sunroomGuide;
    live.markers = info.markers;
    pm.setTempo(info.tempo);
    pm.setTimeSignature(info.timeSignatureNumerator, info.timeSignatureDenominator);
    pm.setLoopSettings(info.loopEnabled, info.loopStartBeats, info.loopEndBeats);
    if (auto* timeline = TimelineController::getCurrent()) {
        timeline->dispatch(SetTempoEvent{info.tempo});
        timeline->dispatch(
            SetTimeSignatureEvent{info.timeSignatureNumerator, info.timeSignatureDenominator});
        if (info.loopEndBeats > info.loopStartBeats)
            timeline->dispatch(SetLoopRegionBeatsEvent{info.loopStartBeats, info.loopEndBeats});
        else
            timeline->dispatch(ClearLoopRegionEvent{});
        timeline->dispatch(SetLoopEnabledEvent{info.loopEnabled});
        std::vector<TimelineMarker> markers;
        for (const auto& marker : info.markers)
            markers.emplace_back(marker.id, marker.positionBeats, marker.name,
                                 juce::Colour(marker.colourArgb));
        timeline->dispatch(SetMarkersEvent{std::move(markers)});
    }
    if (engine_) {
        engine_->setTempo(info.tempo);
        engine_->setTimeSignature(info.timeSignatureNumerator, info.timeSignatureDenominator);
        engine_->setLoopRegionBeats(
            BeatRange{BeatPosition{info.loopStartBeats}, BeatPosition{info.loopEndBeats}});
        engine_->setLooping(info.loopEnabled);
    }
}
void CreateJourneyCommand::execute() {
    auto& tm = TrackManager::getInstance();
    auto& cm = ClipManager::getInstance();
    if (engine_)
        engine_->stop();
    ClipManager::BatchScope batch;
    if (captured_) {
        applyProject(after_);
        for (const auto& track : tracks_)
            tm.restoreTrack(track);
        for (const auto& clip : clips_)
            cm.restoreClip(clip);
        return;
    }
    before_ = ProjectManager::getInstance().getCurrentProjectInfo();
    after_ = before_;
    after_.tempo = options_.tempo;
    after_.timeSignatureNumerator = 4;
    after_.timeSignatureDenominator = 4;
    after_.keyRoot = options_.root;
    after_.keyQuality = options_.mood == 2 ? 0 : 1;
    after_.sunroomMood = options_.mood;
    after_.sunroomGuide = true;
    after_.loopEnabled = true;
    after_.loopStartBeats = 0;
    after_.loopEndBeats = options_.bars * 4;
    if (after_.markers.empty()) {
        const int sections = options_.bars == 8 ? 1 : 4;
        for (int i = 0; i < sections; ++i)
            after_.markers.push_back({i + 1, double(i * options_.bars * 4 / sections),
                                      sections == 1 ? "Loop" : sectionName(i),
                                      layers[static_cast<size_t>(i)].colour});
    }
    applyProject(after_);
    const auto song = compose(options_);
    for (size_t layer = 0; layer < layers.size(); ++layer) {
        if (song.phrases[layer].empty())
            continue;
        const auto id = tm.createTrack(layers[layer].name, TrackType::Audio);
        ids_.push_back(id);
        installSound(id, static_cast<int>(layer), options_);
        for (const auto& phrase : song.phrases[layer]) {
            const auto clip = cm.createMidiClipBeats(id, phrase.start, phrase.length);
            cm.setClipName(clip, juce::String(phrase.name) + " / " + layers[layer].name);
            cm.setClipColour(clip, juce::Colour(layers[layer].colour));
            for (const auto& n : phrase.notes) {
                MidiNote note;
                note.noteNumber = n.pitch;
                note.startBeat = n.beat;
                note.lengthBeats = n.length;
                note.velocity = n.velocity;
                cm.addMidiNote(clip, note);
            }
            if (const auto* c = cm.getClip(clip))
                clips_.push_back(*c);
        }
        if (const auto* t = tm.getTrack(id))
            tracks_.push_back(*t);
    }
    if (engine_)
        engine_->locate(0);
    captured_ = true;
}
void CreateJourneyCommand::undo() {
    if (engine_)
        engine_->stop();
    auto& cm = ClipManager::getInstance();
    ClipManager::BatchScope batch;
    for (const auto& clip : clips_)
        cm.deleteClip(clip.id);
    for (auto id : ids_)
        TrackManager::getInstance().deleteTrack(id);
    applyProject(before_);
}

namespace {
constexpr double kFixtureBars = 8.0;
constexpr double kFixtureBeats = kFixtureBars * 4.0;
constexpr double kFixtureTempo = 100.0;
constexpr int kFixtureKeyRoot = 9;  // A
constexpr int kKickPad = 0;
constexpr int kSnarePad = 1;
constexpr int kHatPad = 2;
constexpr int kKickNote = daw::audio::DrumGridPlugin::baseNote + kKickPad;
constexpr int kSnareNote = daw::audio::DrumGridPlugin::baseNote + kSnarePad;
constexpr int kHatNote = daw::audio::DrumGridPlugin::baseNote + kHatPad;

bool requireBundledDevice(const juce::String& id, juce::String& error) {
    if (daw::audio::findInternalPluginSpec(id) != nullptr ||
        daw::audio::compiled::findCompiledPluginSpec(id) != nullptr)
        return true;
    error = "Missing bundled device: " + id;
    return false;
}

daw::audio::DrumGridPlugin* waitForDrumGrid(AudioEngine* engine, TrackId trackId,
                                            DeviceId deviceId) {
    if (engine == nullptr)
        return nullptr;
    auto* bridge = engine->getAudioBridge();
    if (bridge == nullptr)
        return nullptr;
    const auto path = ChainNodePath::topLevelDevice(trackId, deviceId);
    for (int attempt = 0; attempt < 80; ++attempt) {
        if (auto plugin = bridge->getPlugin(path)) {
            if (auto* grid = dynamic_cast<daw::audio::DrumGridPlugin*>(plugin.get()))
                return grid;
        }
        if (auto* mm = juce::MessageManager::getInstanceWithoutCreating())
            mm->runDispatchLoopUntil(25);
        else
            juce::Thread::sleep(25);
    }
    return nullptr;
}

void addFixtureNote(ClipManager& cm, ClipId clip, int pitch, double beat, double length,
                    int velocity) {
    MidiNote note;
    note.noteNumber = pitch;
    note.startBeat = beat;
    note.lengthBeats = length;
    note.velocity = velocity;
    cm.addMidiNote(clip, note);
}
}  // namespace

CreateFixtureACommand::CreateFixtureACommand(AudioEngine* engine) : engine_(engine) {}

void CreateFixtureACommand::applyProject(const ProjectInfo& info) {
    auto& pm = ProjectManager::getInstance();
    auto& live = pm.getMutableProjectInfo();
    live.keyRoot = info.keyRoot;
    live.keyQuality = info.keyQuality;
    live.sunroomMood = info.sunroomMood;
    live.sunroomGuide = info.sunroomGuide;
    live.markers = info.markers;
    pm.setTempo(info.tempo);
    pm.setTimeSignature(info.timeSignatureNumerator, info.timeSignatureDenominator);
    pm.setLoopSettings(info.loopEnabled, info.loopStartBeats, info.loopEndBeats);
    if (auto* timeline = TimelineController::getCurrent()) {
        timeline->dispatch(SetTempoEvent{info.tempo});
        timeline->dispatch(
            SetTimeSignatureEvent{info.timeSignatureNumerator, info.timeSignatureDenominator});
        if (info.loopEndBeats > info.loopStartBeats)
            timeline->dispatch(SetLoopRegionBeatsEvent{info.loopStartBeats, info.loopEndBeats});
        else
            timeline->dispatch(ClearLoopRegionEvent{});
        timeline->dispatch(SetLoopEnabledEvent{info.loopEnabled});
        std::vector<TimelineMarker> markers;
        for (const auto& marker : info.markers)
            markers.emplace_back(marker.id, marker.positionBeats, marker.name,
                                 juce::Colour(marker.colourArgb));
        timeline->dispatch(SetMarkersEvent{std::move(markers)});
    }
    if (engine_) {
        engine_->setTempo(info.tempo);
        engine_->setTimeSignature(info.timeSignatureNumerator, info.timeSignatureDenominator);
        engine_->setLoopRegionBeats(
            BeatRange{BeatPosition{info.loopStartBeats}, BeatPosition{info.loopEndBeats}});
        engine_->setLooping(info.loopEnabled);
    }
}

void CreateFixtureACommand::rollbackPartial() {
    auto& cm = ClipManager::getInstance();
    ClipManager::BatchScope batch;
    for (const auto& clip : clips_)
        cm.deleteClip(clip.id);
    clips_.clear();
    for (auto id : ids_)
        TrackManager::getInstance().deleteTrack(id);
    ids_.clear();
    tracks_.clear();
    applyProject(before_);
}

void CreateFixtureACommand::execute() {
    auto& tm = TrackManager::getInstance();
    auto& cm = ClipManager::getInstance();
    if (engine_)
        engine_->stop();
    ClipManager::BatchScope batch;
    if (captured_) {
        applyProject(after_);
        for (const auto& track : tracks_)
            tm.restoreTrack(track);
        for (const auto& clip : clips_)
            cm.restoreClip(clip);
        return;
    }

    failed_ = false;
    failureReason_.clear();
    before_ = ProjectManager::getInstance().getCurrentProjectInfo();

    for (const char* id :
         {"drumgrid", "magda_kick", "magda_snare", "magda_hat", "magda_polysynth"}) {
        if (!requireBundledDevice(id, failureReason_)) {
            failed_ = true;
            return;
        }
    }

    after_ = before_;
    after_.tempo = kFixtureTempo;
    after_.timeSignatureNumerator = 4;
    after_.timeSignatureDenominator = 4;
    after_.keyRoot = kFixtureKeyRoot;
    after_.keyQuality = 1;  // minor
    after_.sunroomMood = 1;  // Deep space / natural minor (matches Fixture A harmony)
    after_.sunroomGuide = true;
    after_.loopEnabled = true;
    after_.loopStartBeats = 0.0;
    after_.loopEndBeats = kFixtureBeats;
    after_.markers = {{1, 0.0, "Fixture A", static_cast<juce::uint32>(0xff6b8cae)}};
    applyProject(after_);

    // Harmonic guide only — sounding chords live on the Chords instrument track.
    const auto chordBefore = tm.getChordTrackId();
    const auto chordId = tm.ensureChordTrack();
    if (chordBefore == INVALID_TRACK_ID && chordId != INVALID_TRACK_ID)
        ids_.push_back(chordId);

    const auto drumsId = tm.createTrack("Drums", TrackType::Audio);
    ids_.push_back(drumsId);
    const auto drumDevice =
        tm.addDeviceToTrack(drumsId, makeDevice("drumgrid", "Drum Grid", true));
    if (drumDevice == INVALID_DEVICE_ID) {
        failureReason_ = "Failed to add Drum Grid to the Drums track";
        failed_ = true;
        rollbackPartial();
        return;
    }

    auto* grid = waitForDrumGrid(engine_, drumsId, drumDevice);
    if (grid == nullptr) {
        failureReason_ = "Drum Grid plugin did not become ready in time";
        failed_ = true;
        rollbackPartial();
        return;
    }
    grid->loadInternalPluginToPad(kKickPad, "magda_kick");
    grid->loadInternalPluginToPad(kSnarePad, "magda_snare");
    grid->loadInternalPluginToPad(kHatPad, "magda_hat");
    if (engine_ != nullptr)
        if (auto* bridge = engine_->getAudioBridge())
            bridge->getPluginManager().capturePluginState(
                ChainNodePath::topLevelDevice(drumsId, drumDevice));

    tm.setTrackVolume(drumsId, 0.55f);
    const auto drumClip = cm.createMidiClipBeats(drumsId, 0.0, kFixtureBeats);
    cm.setClipName(drumClip, "Fixture A / Drums");
    cm.setClipColour(drumClip, juce::Colour(0xff6b8cae));
    for (int bar = 0; bar < static_cast<int>(kFixtureBars); ++bar) {
        const double barStart = bar * 4.0;
        addFixtureNote(cm, drumClip, kKickNote, barStart + 0.0, 0.12, 100);
        addFixtureNote(cm, drumClip, kKickNote, barStart + 2.0, 0.12, 100);
        addFixtureNote(cm, drumClip, kSnareNote, barStart + 1.0, 0.12, 100);
        addFixtureNote(cm, drumClip, kSnareNote, barStart + 3.0, 0.12, 100);
        for (int eighth = 0; eighth < 8; ++eighth)
            addFixtureNote(cm, drumClip, kHatNote, barStart + eighth * 0.5, 0.08, 70);
    }
    if (const auto* c = cm.getClip(drumClip))
        clips_.push_back(*c);
    if (const auto* t = tm.getTrack(drumsId))
        tracks_.push_back(*t);

    struct HarmonyRow {
        double startBeat;
        std::array<int, 3> chord;
        int bassRoot;
    };
    const HarmonyRow rows[] = {
        {0.0, {57, 60, 64}, 45},
        {8.0, {53, 57, 60}, 41},
        {16.0, {55, 60, 64}, 48},
        {24.0, {55, 59, 62}, 43},
    };

    const auto bassId = tm.createTrack("Bass", TrackType::Audio);
    ids_.push_back(bassId);
    tm.addDeviceToTrack(bassId, makeDevice("magda_polysynth", "Bass", true,
                                           {{0, 0},
                                            {1, -4},
                                            {4, 3},
                                            {5, -24},
                                            {17, 220},
                                            {18, .08f},
                                            {24, 12},
                                            {25, 260},
                                            {26, .65f},
                                            {27, 160},
                                            {43, -6}}));
    tm.setTrackVolume(bassId, 0.45f);
    const auto bassClip = cm.createMidiClipBeats(bassId, 0.0, kFixtureBeats);
    cm.setClipName(bassClip, "Fixture A / Bass");
    cm.setClipColour(bassClip, juce::Colour(0xff3d6b5a));
    for (const auto& row : rows) {
        for (int beat = 0; beat < 8; ++beat)
            addFixtureNote(cm, bassClip, row.bassRoot, row.startBeat + beat, 0.85, 90);
    }
    if (const auto* c = cm.getClip(bassClip))
        clips_.push_back(*c);
    if (const auto* t = tm.getTrack(bassId))
        tracks_.push_back(*t);

    const auto chordsId = tm.createTrack("Chords", TrackType::Audio);
    ids_.push_back(chordsId);
    tm.addDeviceToTrack(chordsId, makeDevice("magda_polysynth", "Chords", true,
                                             {{0, 2},
                                              {1, -8},
                                              {4, 1},
                                              {5, -16},
                                              {17, 900},
                                              {18, .12f},
                                              {24, 900},
                                              {25, 1100},
                                              {26, .7f},
                                              {27, 2800},
                                              {43, -8}}));
    tm.setTrackVolume(chordsId, 0.35f);
    const auto chordClip = cm.createMidiClipBeats(chordsId, 0.0, kFixtureBeats);
    cm.setClipName(chordClip, "Fixture A / Chords");
    cm.setClipColour(chordClip, juce::Colour(0xff8a6b4a));
    for (const auto& row : rows) {
        for (int pitch : row.chord)
            addFixtureNote(cm, chordClip, pitch, row.startBeat, 7.85, 80);
    }
    if (const auto* c = cm.getClip(chordClip))
        clips_.push_back(*c);
    if (const auto* t = tm.getTrack(chordsId))
        tracks_.push_back(*t);

    tracks_.clear();
    for (auto id : ids_) {
        if (const auto* t = tm.getTrack(id))
            tracks_.push_back(*t);
    }

    summary_ = "Added drums, bass and chords in A minor at 100 BPM";
    if (engine_)
        engine_->locate(0);
    captured_ = true;
}

void CreateFixtureACommand::undo() {
    if (engine_)
        engine_->stop();
    auto& cm = ClipManager::getInstance();
    ClipManager::BatchScope batch;
    for (const auto& clip : clips_)
        cm.deleteClip(clip.id);
    for (auto id : ids_)
        TrackManager::getInstance().deleteTrack(id);
    applyProject(before_);
}

namespace {
constexpr double kFixtureBBars = 32.0;
constexpr double kFixtureBBeats = kFixtureBBars * 4.0;
constexpr double kSectionBeats = 8.0 * 4.0;  // 8 bars

TrackId findNamedTrack(const juce::String& name) {
    auto& cm = ClipManager::getInstance();
    for (const auto& track : TrackManager::getInstance().getTracks()) {
        if (track.name != name)
            continue;
        for (const auto& clip : cm.getClips()) {
            if (clip.trackId == track.id && clip.name == "Fixture A / " + name)
                return track.id;
        }
    }
    return INVALID_TRACK_ID;
}

bool arrangementRangeOccupied(TrackId trackId, double startBeat, double lengthBeats) {
    const double endBeat = startBeat + lengthBeats;
    for (const auto& clip : ClipManager::getInstance().getClips()) {
        if (clip.trackId != trackId || clip.view != ClipView::Arrangement)
            continue;
        const double c0 = clip.placement.startBeat;
        const double c1 = c0 + clip.placement.lengthBeats;
        if (c0 < endBeat && c1 > startBeat)
            return true;
    }
    return false;
}

void fillDrumPattern(ClipManager& cm, ClipId clip, int bars, bool lightOnly, bool variation) {
    for (int bar = 0; bar < bars; ++bar) {
        const double barStart = bar * 4.0;
        if (!lightOnly) {
            addFixtureNote(cm, clip, kKickNote, barStart + 0.0, 0.12, 100);
            addFixtureNote(cm, clip, kKickNote, barStart + 2.0, 0.12, 100);
            if (variation)
                addFixtureNote(cm, clip, kKickNote, barStart + 1.5, 0.12, 85);
            addFixtureNote(cm, clip, kSnareNote, barStart + 1.0, 0.12, 100);
            addFixtureNote(cm, clip, kSnareNote, barStart + 3.0, 0.12, 100);
        }
        for (int eighth = 0; eighth < 8; ++eighth)
            addFixtureNote(cm, clip, kHatNote, barStart + eighth * 0.5, 0.08, lightOnly ? 55 : 70);
    }
}

void applyFixtureProject(AudioEngine* engine, const ProjectInfo& info) {
    auto& pm = ProjectManager::getInstance();
    auto& live = pm.getMutableProjectInfo();
    live.keyRoot = info.keyRoot;
    live.keyQuality = info.keyQuality;
    live.sunroomMood = info.sunroomMood;
    live.sunroomGuide = info.sunroomGuide;
    live.markers = info.markers;
    pm.setTempo(info.tempo);
    pm.setTimeSignature(info.timeSignatureNumerator, info.timeSignatureDenominator);
    pm.setLoopSettings(info.loopEnabled, info.loopStartBeats, info.loopEndBeats);
    if (auto* timeline = TimelineController::getCurrent()) {
        timeline->dispatch(SetTempoEvent{info.tempo});
        timeline->dispatch(
            SetTimeSignatureEvent{info.timeSignatureNumerator, info.timeSignatureDenominator});
        if (info.loopEndBeats > info.loopStartBeats)
            timeline->dispatch(SetLoopRegionBeatsEvent{info.loopStartBeats, info.loopEndBeats});
        else
            timeline->dispatch(ClearLoopRegionEvent{});
        timeline->dispatch(SetLoopEnabledEvent{info.loopEnabled});
        std::vector<TimelineMarker> markers;
        for (const auto& marker : info.markers)
            markers.emplace_back(marker.id, marker.positionBeats, marker.name,
                                 juce::Colour(marker.colourArgb));
        timeline->dispatch(SetMarkersEvent{std::move(markers)});
    }
    if (engine) {
        engine->setTempo(info.tempo);
        engine->setTimeSignature(info.timeSignatureNumerator, info.timeSignatureDenominator);
        engine->setLoopRegionBeats(
            BeatRange{BeatPosition{info.loopStartBeats}, BeatPosition{info.loopEndBeats}});
        engine->setLooping(info.loopEnabled);
    }
}
}  // namespace

CreateFixtureBCommand::CreateFixtureBCommand(AudioEngine* engine) : engine_(engine) {}

void CreateFixtureBCommand::applyProject(const ProjectInfo& info) {
    applyFixtureProject(engine_, info);
}

void CreateFixtureBCommand::execute() {
    auto& tm = TrackManager::getInstance();
    auto& cm = ClipManager::getInstance();
    if (engine_)
        engine_->stop();
    ClipManager::BatchScope batch;

    if (captured_) {
        applyProject(after_);
        for (const auto& clip : removedClips_)
            cm.deleteClip(clip.id);
        for (const auto& clip : createdClips_)
            cm.restoreClip(clip);
        return;
    }

    failed_ = false;
    failureReason_.clear();
    createdClips_.clear();
    removedClips_.clear();

    const auto drumsId = findNamedTrack("Drums");
    const auto bassId = findNamedTrack("Bass");
    const auto chordsId = findNamedTrack("Chords");
    if (drumsId == INVALID_TRACK_ID || bassId == INVALID_TRACK_ID || chordsId == INVALID_TRACK_ID) {
        failureReason_ =
            "Fixture B needs Fixture A tracks (Drums, Bass, Chords). Create and Play first.";
        failed_ = true;
        return;
    }

    before_ = ProjectManager::getInstance().getCurrentProjectInfo();
    after_ = before_;
    after_.loopEnabled = true;
    after_.loopStartBeats = 0.0;
    after_.loopEndBeats = kFixtureBBeats;
    after_.sunroomGuide = true;
    after_.markers = {
        {1, 0.0, "Intro", static_cast<juce::uint32>(0xff6b8cae)},
        {2, kSectionBeats, "Main", static_cast<juce::uint32>(0xff3d6b5a)},
        {3, kSectionBeats * 2.0, "Variation", static_cast<juce::uint32>(0xff8a6b4a)},
        {4, kSectionBeats * 3.0, "Ending", static_cast<juce::uint32>(0xff9387a8)},
    };
    applyProject(after_);

    for (const auto& clip : cm.getClips()) {
        if (clip.view != ClipView::Arrangement)
            continue;
        if ((clip.trackId == drumsId || clip.trackId == bassId || clip.trackId == chordsId) &&
            clip.name.startsWith("Fixture A /")) {
            removedClips_.push_back(clip);
            cm.deleteClip(clip.id);
        }
    }

    struct SectionSpec {
        const char* name;
        double startBeat;
        bool lightDrums;
        bool drumVariation;
        bool sparseBass;
        bool thinChords;
    };
    const SectionSpec sections[] = {
        {"Intro", 0.0, true, false, true, false},
        {"Main", kSectionBeats, false, false, false, false},
        {"Variation", kSectionBeats * 2.0, false, true, false, false},
        {"Ending", kSectionBeats * 3.0, true, false, true, true},
    };

    const int roots[] = {45, 41, 48, 43};
    const std::array<std::array<int, 3>, 4> chords{{
        {57, 60, 64},
        {53, 57, 60},
        {55, 60, 64},
        {55, 59, 62},
    }};

    auto remember = [&](ClipId id) {
        if (const auto* c = cm.getClip(id))
            createdClips_.push_back(*c);
    };

    for (int scene = 0; scene < 4; ++scene) {
        const auto& sec = sections[scene];

        auto drumSess = cm.createMidiClipBeats(drumsId, 0.0, kSectionBeats, ClipView::Session);
        cm.setClipSceneIndex(drumSess, scene);
        cm.setClipName(drumSess, juce::String("Scene ") + sec.name + " / Drums");
        fillDrumPattern(cm, drumSess, 8, sec.lightDrums, sec.drumVariation);
        remember(drumSess);

        auto bassSess = cm.createMidiClipBeats(bassId, 0.0, kSectionBeats, ClipView::Session);
        cm.setClipSceneIndex(bassSess, scene);
        cm.setClipName(bassSess, juce::String("Scene ") + sec.name + " / Bass");
        for (int cycle = 0; cycle < 4; ++cycle) {
            for (int beat = 0; beat < 8; ++beat) {
                if (sec.sparseBass && (beat % 2) != 0)
                    continue;
                addFixtureNote(cm, bassSess, roots[scene], cycle * 8.0 + beat, 0.85,
                               sec.sparseBass ? 70 : 90);
            }
        }
        remember(bassSess);

        auto chordSess = cm.createMidiClipBeats(chordsId, 0.0, kSectionBeats, ClipView::Session);
        cm.setClipSceneIndex(chordSess, scene);
        cm.setClipName(chordSess, juce::String("Scene ") + sec.name + " / Chords");
        const int count = sec.thinChords ? 1 : 3;
        for (int cycle = 0; cycle < 4; ++cycle) {
            for (int i = 0; i < count; ++i)
                addFixtureNote(cm, chordSess,
                               chords[static_cast<size_t>(scene)][static_cast<size_t>(i)],
                               cycle * 8.0, 7.85, sec.thinChords ? 60 : 80);
        }
        remember(chordSess);

        auto drumArr = cm.createMidiClipBeats(drumsId, sec.startBeat, kSectionBeats);
        cm.setClipName(drumArr, juce::String(sec.name) + " / Drums");
        fillDrumPattern(cm, drumArr, 8, sec.lightDrums, sec.drumVariation);
        remember(drumArr);

        auto bassArr = cm.createMidiClipBeats(bassId, sec.startBeat, kSectionBeats);
        cm.setClipName(bassArr, juce::String(sec.name) + " / Bass");
        for (int cycle = 0; cycle < 4; ++cycle) {
            for (int beat = 0; beat < 8; ++beat) {
                if (sec.sparseBass && (beat % 2) != 0)
                    continue;
                addFixtureNote(cm, bassArr, roots[scene], cycle * 8.0 + beat, 0.85,
                               sec.sparseBass ? 70 : 90);
            }
        }
        remember(bassArr);

        auto chordArr = cm.createMidiClipBeats(chordsId, sec.startBeat, kSectionBeats);
        cm.setClipName(chordArr, juce::String(sec.name) + " / Chords");
        for (int cycle = 0; cycle < 4; ++cycle) {
            for (int i = 0; i < count; ++i)
                addFixtureNote(cm, chordArr,
                               chords[static_cast<size_t>(scene)][static_cast<size_t>(i)],
                               cycle * 8.0, 7.85, sec.thinChords ? 60 : 80);
        }
        remember(chordArr);
    }

    summary_ = "Fixture B: Intro / Main / Variation / Ending (32 bars) with session scenes.";
    if (engine_)
        engine_->locate(0);
    captured_ = true;
}

void CreateFixtureBCommand::undo() {
    if (engine_)
        engine_->stop();
    auto& cm = ClipManager::getInstance();
    ClipManager::BatchScope batch;
    for (const auto& clip : createdClips_)
        cm.deleteClip(clip.id);
    for (const auto& clip : removedClips_)
        cm.restoreClip(clip);
    applyProject(before_);
}

PlaceSceneInArrangementCommand::PlaceSceneInArrangementCommand(int sceneIndex,
                                                               double destStartBeats)
    : sceneIndex_(sceneIndex), destStartBeats_(destStartBeats) {}

void PlaceSceneInArrangementCommand::execute() {
    auto& cm = ClipManager::getInstance();
    auto& tm = TrackManager::getInstance();
    ClipManager::BatchScope batch;

    if (captured_) {
        for (const auto& clip : createdClips_)
            cm.restoreClip(clip);
        return;
    }

    failed_ = false;
    failureReason_.clear();
    createdClips_.clear();

    if (sceneIndex_ < 0) {
        failureReason_ = "Scene index must be >= 0";
        failed_ = true;
        return;
    }

    std::vector<ClipId> sessionClips;
    for (const auto& track : tm.getTracks()) {
        if (track.type == TrackType::Chord)
            continue;
        const auto clipId = cm.getClipInSlot(track.id, sceneIndex_);
        if (clipId != INVALID_CLIP_ID)
            sessionClips.push_back(clipId);
    }
    if (sessionClips.empty()) {
        failureReason_ = "Scene " + juce::String(sceneIndex_) + " has no session clips to place.";
        failed_ = true;
        return;
    }

    for (auto sessionId : sessionClips) {
        const auto* session = cm.getClip(sessionId);
        if (session == nullptr)
            continue;
        const double length = session->placement.lengthBeats > 0.0 ? session->placement.lengthBeats
                                                                   : kSectionBeats;
        if (arrangementRangeOccupied(session->trackId, destStartBeats_, length)) {
            failureReason_ =
                "Arrangement already has clips at the destination - choose an empty range "
                "(no silent overwrite).";
            failed_ = true;
            for (const auto& clip : createdClips_)
                cm.deleteClip(clip.id);
            createdClips_.clear();
            return;
        }
    }

    for (auto sessionId : sessionClips) {
        const auto* session = cm.getClip(sessionId);
        if (session == nullptr)
            continue;
        const double length = session->placement.lengthBeats > 0.0 ? session->placement.lengthBeats
                                                                   : kSectionBeats;
        ClipId newId = INVALID_CLIP_ID;
        if (session->isAudio()) {
            juce::String path;
            if (!session->audio().events.empty())
                path = session->audio().events.front().sourceFilePath();
            if (path.isEmpty()) {
                failureReason_ = "Session audio clip has no source file. Nothing was placed.";
                failed_ = true;
                for (const auto& clip : createdClips_)
                    cm.deleteClip(clip.id);
                createdClips_.clear();
                return;
            }
            newId = cm.createAudioClipBeats(session->trackId, destStartBeats_, length, path,
                                            ClipView::Arrangement);
        } else {
            newId = cm.createMidiClipBeats(session->trackId, destStartBeats_, length,
                                           ClipView::Arrangement, ClipOverlapPolicy::PreserveExisting);
        }
        if (newId == INVALID_CLIP_ID) {
            failureReason_ = "Could not create arrangement clip (overlap or track error).";
            failed_ = true;
            for (const auto& clip : createdClips_)
                cm.deleteClip(clip.id);
            createdClips_.clear();
            return;
        }
        cm.setClipName(newId, session->name + " (placed)");
        cm.setClipColour(newId, session->colour);
        if (session->isMidi()) {
            for (const auto& note : session->midiNotes)
                cm.addMidiNote(newId, note);
        }
        if (const auto* c = cm.getClip(newId))
            createdClips_.push_back(*c);
    }

    summary_ = "Placed scene " + juce::String(sceneIndex_) + " at beat " +
               juce::String(destStartBeats_, 1) + " (" + juce::String((int)createdClips_.size()) +
               " clips). Deterministic place - not performance capture.";
    captured_ = true;
}

void PlaceSceneInArrangementCommand::undo() {
    auto& cm = ClipManager::getInstance();
    ClipManager::BatchScope batch;
    for (const auto& clip : createdClips_)
        cm.deleteClip(clip.id);
}

CreateFixtureCCommand::CreateFixtureCCommand(AudioEngine* engine) : engine_(engine) {}

void CreateFixtureCCommand::execute() {
    auto& tm = TrackManager::getInstance();
    auto& cm = ClipManager::getInstance();
    if (engine_)
        engine_->stop();
    ClipManager::BatchScope batch;

    if (captured_) {
        applyFixtureProject(engine_, after_);
        for (const auto& track : tracks_)
            tm.restoreTrack(track);
        for (const auto& clip : clips_)
            cm.restoreClip(clip);
        return;
    }

    failed_ = false;
    failureReason_.clear();
    before_ = ProjectManager::getInstance().getCurrentProjectInfo();

    for (const char* id : {"drumgrid", "magda_kick", "magda_snare", "magda_hat", "magda_polysynth"}) {
        if (!requireBundledDevice(id, failureReason_)) {
            failed_ = true;
            return;
        }
    }

    auto info = before_;
    info.tempo = kFixtureTempo;
    info.keyRoot = kFixtureKeyRoot;
    info.keyQuality = 1;
    info.sunroomGuide = true;
    info.loopEnabled = true;
    info.loopStartBeats = 0.0;
    info.loopEndBeats = kFixtureBeats;
    info.markers = {{1, 0.0, "Fixture C", static_cast<juce::uint32>(0xff6b8cae)}};
    after_ = info;
    applyFixtureProject(engine_, after_);

    const auto pulseId = tm.createTrack("Pulse", TrackType::Audio);
    ids_.push_back(pulseId);
    const auto drumDevice = tm.addDeviceToTrack(pulseId, makeDevice("drumgrid", "Drum Grid", true));
    if (drumDevice == INVALID_DEVICE_ID) {
        failureReason_ = "Failed to add Drum Grid for Fixture C";
        failed_ = true;
        rollbackPartial();
        return;
    }
    auto* grid = waitForDrumGrid(engine_, pulseId, drumDevice);
    if (grid == nullptr) {
        failureReason_ = "Drum Grid not ready for Fixture C";
        failed_ = true;
        rollbackPartial();
        return;
    }
    grid->loadInternalPluginToPad(kKickPad, "magda_kick");
    grid->loadInternalPluginToPad(kSnarePad, "magda_snare");
    grid->loadInternalPluginToPad(kHatPad, "magda_hat");
    if (engine_ != nullptr)
        if (auto* bridge = engine_->getAudioBridge())
            bridge->getPluginManager().capturePluginState(
                ChainNodePath::topLevelDevice(pulseId, drumDevice));

    // Sparse arrangement rhythm (kick on 1 only)
    auto arr = cm.createMidiClipBeats(pulseId, 0.0, kFixtureBeats);
    cm.setClipName(arr, "Fixture C / Arrangement Pulse");
    for (int bar = 0; bar < static_cast<int>(kFixtureBars); ++bar)
        addFixtureNote(cm, arr, kKickNote, bar * 4.0, 0.12, 90);

    // Different Session rhythm (full beat) in scene 0
    auto sess = cm.createMidiClipBeats(pulseId, 0.0, kFixtureBeats, ClipView::Session);
    cm.setClipSceneIndex(sess, 0);
    cm.setClipName(sess, "Fixture C / Session Pulse");
    fillDrumPattern(cm, sess, static_cast<int>(kFixtureBars), false, false);

    // Second track stays Arrangement-only
    const auto padId = tm.createTrack("Pad", TrackType::Audio);
    ids_.push_back(padId);
    tm.addDeviceToTrack(padId, makeDevice("magda_polysynth", "Pad", true,
                                          {{0, 2}, {1, -10}, {4, 1}, {5, -20}, {17, 800}, {43, -8}}));
    auto padClip = cm.createMidiClipBeats(padId, 0.0, kFixtureBeats);
    cm.setClipName(padClip, "Fixture C / Arrangement Pad");
    addFixtureNote(cm, padClip, 57, 0.0, kFixtureBeats - 0.1, 50);

    for (auto id : ids_)
        if (const auto* t = tm.getTrack(id))
            tracks_.push_back(*t);
    for (auto id : {arr, sess, padClip})
        if (const auto* c = cm.getClip(id))
            clips_.push_back(*c);

    summary_ = "Fixture C: Pulse has sparse Arrangement + different Session clip; Pad is Arrangement-only.";
    if (engine_)
        engine_->locate(0);
    captured_ = true;
}

void CreateFixtureCCommand::rollbackPartial() {
    auto& cm = ClipManager::getInstance();
    ClipManager::BatchScope batch;
    for (const auto& clip : clips_)
        cm.deleteClip(clip.id);
    clips_.clear();
    for (auto id : ids_)
        TrackManager::getInstance().deleteTrack(id);
    ids_.clear();
    tracks_.clear();
    applyFixtureProject(engine_, before_);
}

void CreateFixtureCCommand::undo() {
    if (engine_)
        engine_->stop();
    auto& cm = ClipManager::getInstance();
    ClipManager::BatchScope batch;
    for (const auto& clip : clips_)
        cm.deleteClip(clip.id);
    for (auto id : ids_)
        TrackManager::getInstance().deleteTrack(id);
    applyFixtureProject(engine_, before_);
}

namespace {
constexpr const char* kSharedSpaceTrackName = "Shared Space";

bool trackHostsMagdaReverb(const TrackInfo& track) {
    for (const auto& element : track.chain.fxChainElements) {
        if (!isDevice(element))
            continue;
        const auto& device = getDevice(element);
        if (device.pluginId == "magda_reverb" || device.uniqueId == "magda_reverb" ||
            device.fileOrIdentifier == "magda_reverb")
            return true;
    }
    return false;
}

TrackId findSharedSpatialAux() {
    TrackId named = INVALID_TRACK_ID;
    for (const auto& track : TrackManager::getInstance().getTracks()) {
        if (track.type != TrackType::Aux)
            continue;
        if (trackHostsMagdaReverb(track))
            return track.id;
        if (named == INVALID_TRACK_ID && track.name == kSharedSpaceTrackName)
            named = track.id;
    }
    return named;
}

bool isSpatialSendCandidate(const TrackInfo& track) {
    if (track.type == TrackType::Master || track.type == TrackType::Aux ||
        track.type == TrackType::Group || track.type == TrackType::Chord)
        return false;
    if (track.name == "Drums" || track.name == "Bass" || track.name == "Chords" ||
        track.name == "Pulse" || track.name == "Pad")
        return true;
    return track.type == TrackType::Audio;
}

DeviceInfo sharedSpaceReverbDevice() {
    return makeDevice("magda_reverb", "Shared Space", false,
                      {{0, 1},
                       {1, 1.0f},
                       {2, 35},
                       {3, 55},
                       {4, 50},
                       {5, 180},
                       {6, 5500},
                       {7, 120},
                       {8, -2}});
}
}  // namespace

ApplySharedSpatialReturnCommand::ApplySharedSpatialReturnCommand(float sendLevel)
    : sendLevel_(juce::jlimit(0.0f, 1.0f, sendLevel)) {}

void ApplySharedSpatialReturnCommand::execute() {
    auto& tm = TrackManager::getInstance();
    failed_ = false;
    failureReason_.clear();

    if (captured_) {
        if (didCreateAux_)
            tm.restoreTrack(createdAuxTrack_);
        else if (didAddReverb_)
            createdReverbId_ = tm.addDeviceToTrack(auxTrackId_, sharedSpaceReverbDevice());
        for (auto& snap : sends_) {
            if (snap.addedSend)
                tm.addSend(snap.sourceId, auxTrackId_);
            const auto* source = tm.getTrack(snap.sourceId);
            int bus = snap.busIndex;
            if (source != nullptr) {
                for (const auto& send : source->sends) {
                    if (send.destTrackId == auxTrackId_) {
                        bus = send.busIndex;
                        snap.busIndex = bus;
                        break;
                    }
                }
            }
            if (bus >= 0)
                tm.setSendLevel(snap.sourceId, bus, sendLevel_);
        }
        return;
    }

    sends_.clear();
    didCreateAux_ = false;
    didAddReverb_ = false;
    createdReverbId_ = INVALID_DEVICE_ID;
    auxTrackId_ = findSharedSpatialAux();

    if (auxTrackId_ == INVALID_TRACK_ID) {
        auxTrackId_ = tm.createTrack(kSharedSpaceTrackName, TrackType::Aux);
        if (auxTrackId_ == INVALID_TRACK_ID) {
            failureReason_ = "Could not create Shared Space return track.";
            failed_ = true;
            return;
        }
        createdReverbId_ = tm.addDeviceToTrack(auxTrackId_, sharedSpaceReverbDevice());
        if (const auto* aux = tm.getTrack(auxTrackId_)) {
            createdAuxTrack_ = *aux;
            didCreateAux_ = true;
        }
    } else if (const auto* aux = tm.getTrack(auxTrackId_);
               aux != nullptr && !trackHostsMagdaReverb(*aux)) {
        createdReverbId_ = tm.addDeviceToTrack(auxTrackId_, sharedSpaceReverbDevice());
        didAddReverb_ = createdReverbId_ != INVALID_DEVICE_ID;
    }

    int sendCount = 0;
    for (const auto& track : tm.getTracks()) {
        if (!isSpatialSendCandidate(track) || track.id == auxTrackId_)
            continue;

        SendSnapshot snap;
        snap.sourceId = track.id;
        for (const auto& send : track.sends) {
            if (send.destTrackId == auxTrackId_) {
                snap.hadSend = true;
                snap.busIndex = send.busIndex;
                snap.previousLevel = send.level;
                break;
            }
        }
        if (!snap.hadSend) {
            tm.addSend(track.id, auxTrackId_);
            snap.addedSend = true;
            if (const auto* updated = tm.getTrack(track.id)) {
                for (const auto& send : updated->sends) {
                    if (send.destTrackId == auxTrackId_) {
                        snap.busIndex = send.busIndex;
                        snap.previousLevel = send.level;
                        break;
                    }
                }
            }
        }
        if (snap.busIndex >= 0) {
            tm.setSendLevel(track.id, snap.busIndex, sendLevel_);
            ++sendCount;
        }
        sends_.push_back(snap);
    }

    if (sendCount == 0) {
        failureReason_ = "No instrument tracks to send to Shared Space. Create and Play first.";
        failed_ = true;
        if (didCreateAux_) {
            tm.deleteTrack(auxTrackId_);
            didCreateAux_ = false;
            auxTrackId_ = INVALID_TRACK_ID;
        } else if (didAddReverb_ && createdReverbId_ != INVALID_DEVICE_ID) {
            tm.removeDeviceFromTrack(auxTrackId_, createdReverbId_);
            didAddReverb_ = false;
        }
        sends_.clear();
        return;
    }

    summary_ = "Shared Space return ready (send amount " + juce::String(sendLevel_, 2) + " on " +
               juce::String(sendCount) +
               " tracks). This is send level to one Aux reverb, not insert wet/dry.";
    captured_ = true;
}

void ApplySharedSpatialReturnCommand::undo() {
    auto& tm = TrackManager::getInstance();
    for (auto it = sends_.rbegin(); it != sends_.rend(); ++it) {
        if (it->addedSend) {
            if (it->busIndex >= 0)
                tm.removeSend(it->sourceId, it->busIndex);
        } else if (it->hadSend && it->busIndex >= 0) {
            tm.setSendLevel(it->sourceId, it->busIndex, it->previousLevel);
        }
    }
    if (createdReverbId_ != INVALID_DEVICE_ID && !didCreateAux_ && auxTrackId_ != INVALID_TRACK_ID)
        tm.removeDeviceFromTrack(auxTrackId_, createdReverbId_);
    if (didCreateAux_ && auxTrackId_ != INVALID_TRACK_ID)
        tm.deleteTrack(auxTrackId_);
}

StagedDslProposal& pendingSlot() {
    static StagedDslProposal slot;
    return slot;
}

StagedDslProposal captureDslProposal(const juce::String& dsl, const juce::String& explanation) {
    static std::uint64_t nextId = 1;
    StagedDslProposal proposal;
    proposal.id = nextId++;
    proposal.projectPath = ProjectManager::getInstance().getCurrentProjectFile().getFullPathName();
    proposal.mutationRevision = ProjectManager::getInstance().mutationRevision();
    proposal.selectedTrack = SelectionManager::getInstance().getSelectedTrack();
    proposal.selectedClip = SelectionManager::getInstance().getSelectedClip();
    proposal.dsl = dsl;
    proposal.explanation = explanation;
    proposal.phase = ProposalPhase::Ready;
    pendingSlot() = proposal;
    return proposal;
}

juce::String extractCoachDsl(const juce::String& text) {
    const juce::String marker("SUNROOM_DSL:");
    const auto at = text.indexOf(marker);
    if (at < 0)
        return {};
    auto line = text.substring(at + marker.length()).trimStart();
    line = line.upToFirstOccurrenceOf("\n", false, false).trim();
    if (line.isEmpty() || line.length() > 400)
        return {};
    if (line.contains(";") || line.contains("{") || line.contains("}"))
        return {};
    return line;
}

const StagedDslProposal* pendingDslProposal() {
    if (pendingSlot().id == 0)
        return nullptr;
    return &pendingSlot();
}

juce::String applyPendingDslProposal(MagdaApi& api, bool cancelled) {
    auto& proposal = pendingSlot();
    if (proposal.id == 0 || proposal.phase == ProposalPhase::Rejected)
        return "Refused: no staged proposal";

    if (cancelled) {
        proposal.phase = ProposalPhase::Canceled;
        return "Refused: proposal canceled; no project change";
    }
    if (proposal.phase == ProposalPhase::Applied)
        return "Refused: proposal already applied";
    if (proposal.phase != ProposalPhase::Ready)
        return "Refused: proposal is not ready";

    const auto path = ProjectManager::getInstance().getCurrentProjectFile().getFullPathName();
    if (path != proposal.projectPath) {
        proposal.phase = ProposalPhase::Rejected;
        return "Refused: project changed; proposal was not applied";
    }
    if (ProjectManager::getInstance().mutationRevision() != proposal.mutationRevision) {
        proposal.phase = ProposalPhase::Stale;
        return "Refused: project changed since the proposal; review a new one (no rollback)";
    }
    if (SelectionManager::getInstance().getSelectedTrack() != proposal.selectedTrack ||
        SelectionManager::getInstance().getSelectedClip() != proposal.selectedClip) {
        proposal.phase = ProposalPhase::Rejected;
        return "Refused: selection changed; proposal was not retargeted";
    }

    proposal.phase = ProposalPhase::Applying;
    dsl::Interpreter interpreter(api);
    bool ok = false;
    juce::String results;
    juce::String error;
    auto& undo = UndoManager::getInstance();
    const auto depthBefore = undo.undoDepth();
    {
        CompoundOperationScope scope("Apply suggestion");
        ok = interpreter.execute(proposal.dsl.toRawUTF8());
        results = interpreter.getResults();
        error = interpreter.getError();
    }
    const bool pushed = undo.undoDepth() == depthBefore + 1 && undo.canUndo() &&
                        undo.getUndoDescription() == "Apply suggestion";
    if (!ok || results.contains("[!]")) {
        if (pushed)
            undo.undo();
        proposal.phase = ProposalPhase::Failed;
        const auto why = error.isNotEmpty() ? error : juce::String("DSL rejected");
        return "Refused: " + why;
    }
    proposal.phase = ProposalPhase::Applied;
    const auto detail = results.isNotEmpty() ? results : juce::String("OK");
    return "Applied: " + detail;
}

juce::File soundLibrary() {
#if JUCE_MAC
    auto dir = juce::File::getSpecialLocation(juce::File::currentApplicationFile)
                   .getChildFile("Contents/Resources/Sunroom/Sounds");
#else
    auto dir = paths::executableDir().getChildFile("Sunroom/Sounds");
#endif
    if (dir.isDirectory())
        return dir;
    return juce::File(SUNROOM_SOURCE_DIR).getChildFile("resources/sunroom/Sounds");
}

std::vector<StarterSound> loadStarterCatalog() {
    std::vector<StarterSound> sounds;
    const auto parsed = juce::JSON::parse(soundLibrary().getChildFile("manifest.json"));
    auto* rows = parsed["sounds"].getArray();
    if (rows == nullptr)
        return sounds;
    for (const auto& item : *rows) {
        StarterSound sound;
        sound.file = item["file"].toString();
        sound.kind = item["kind"].toString();
        if (sound.file.isEmpty())
            continue;
        sound.unpitched = !sound.kind.containsIgnoreCase("pitched");
        const auto bpm = item["bpm"];
        if (!bpm.isVoid() && (bpm.isDouble() || bpm.isInt() || bpm.isInt64()))
            sound.declaredBpm = static_cast<double>(bpm);
        const auto key = item["keyRoot"];
        if (!key.isVoid() && (key.isInt() || key.isInt64())) {
            const int root = static_cast<int>(key);
            if (root >= 0 && root <= 11)
                sound.declaredKeyRoot = root;
        }
        sounds.push_back(sound);
    }
    return sounds;
}

StarterQueryReport queryStarterCatalog(const StarterQuery& query) {
    StarterQueryReport report;
    report.note =
        "Offline starter catalog. Kind, BPM and key are declared only. Unknown BPM and key are "
        "not guessed. Drums are not pitch-matched. This is not Media Library semantic search. "
        "Time-stretch and transpose are not applied.";
    const bool bpmFilter = query.bpmMin.has_value() || query.bpmMax.has_value();
    const bool keyFilter = query.keyRoot.has_value();
    for (const auto& sound : loadStarterCatalog()) {
        if (query.text.isNotEmpty() && !sound.file.containsIgnoreCase(query.text))
            continue;
        if (query.kind.isNotEmpty() && !sound.kind.containsIgnoreCase(query.kind))
            continue;
        if (bpmFilter) {
            if (!sound.declaredBpm) {
                ++report.unknownBpm;
                continue;
            }
            if (query.bpmMin && *sound.declaredBpm < *query.bpmMin)
                continue;
            if (query.bpmMax && *sound.declaredBpm > *query.bpmMax)
                continue;
        }
        if (keyFilter) {
            if (sound.unpitched) {
                ++report.unpitchedSkipped;
                continue;
            }
            if (!sound.declaredKeyRoot) {
                ++report.unknownKey;
                continue;
            }
            if (*sound.declaredKeyRoot != *query.keyRoot)
                continue;
        }
        report.matches.push_back(sound);
    }
    return report;
}

juce::String formatStarterQuery(const StarterQueryReport& report) {
    juce::String out = report.note + "\n";
    out << "Matches " << static_cast<int>(report.matches.size()) << " unknown-bpm "
        << report.unknownBpm << " unknown-key " << report.unknownKey << " unpitched-skipped "
        << report.unpitchedSkipped << "\n";
    for (const auto& sound : report.matches) {
        out << sound.file << " kind=" << sound.kind
            << " pitched=" << (sound.unpitched ? "declared-no" : "declared-yes")
            << " bpm=" << (sound.declaredBpm ? juce::String(*sound.declaredBpm) : juce::String("unknown"))
            << " key="
            << (sound.declaredKeyRoot ? juce::String(*sound.declaredKeyRoot) : juce::String("unknown"))
            << " source=declared\n";
    }
    return out;
}

juce::File resolveStarterFile(const juce::String& fileName) {
    if (fileName.isEmpty() || fileName.contains("..") || fileName.contains("/") ||
        fileName.contains("\\"))
        return {};
    const auto root = soundLibrary();
    const auto file = root.getChildFile(fileName);
    if (!file.existsAsFile() || file.getParentDirectory() != root)
        return {};
    return file;
}

ImportStarterSampleCommand::ImportStarterSampleCommand(juce::String fileName)
    : fileName_(std::move(fileName)) {}

void ImportStarterSampleCommand::execute() {
    if (failed_)
        return;
    auto& tm = TrackManager::getInstance();
    auto& cm = ClipManager::getInstance();
    if (captured_) {
        tm.restoreTrack(track_);
        cm.restoreClip(clip_);
        return;
    }
    const auto file = resolveStarterFile(fileName_);
    if (!file.existsAsFile()) {
        failed_ = true;
        failureReason_ = "Starter sound was not found. Nothing was added.";
        return;
    }
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    if (reader == nullptr || reader->sampleRate <= 0.0 || reader->lengthInSamples <= 0) {
        failed_ = true;
        failureReason_ = "Could not read the starter sound. Nothing was added.";
        return;
    }
    const double seconds = static_cast<double>(reader->lengthInSamples) / reader->sampleRate;
    reader.reset();
    const auto id = tm.createTrack(file.getFileNameWithoutExtension(), TrackType::Audio);
    tm.setTrackVolume(id, 0.35f);
    const auto clipId = cm.createAudioClip(id, 0.0, seconds, file.getFullPathName());
    if (clipId == INVALID_CLIP_ID || cm.getClip(clipId) == nullptr) {
        tm.deleteTrack(id);
        failed_ = true;
        failureReason_ = "Could not add the starter sound. Nothing was added.";
        return;
    }
    cm.setClipName(clipId, file.getFileNameWithoutExtension());
    track_ = *tm.getTrack(id);
    clip_ = *cm.getClip(clipId);
    captured_ = true;
    tm.setSelectedTrack(id);
    summary_ = "Added " + file.getFileNameWithoutExtension() + " source=" + file.getFullPathName() +
               " stretch=not-applied";
}

void ImportStarterSampleCommand::undo() {
    if (!captured_)
        return;
    ClipManager::getInstance().deleteClip(clip_.id);
    TrackManager::getInstance().deleteTrack(track_.id);
}
}  // namespace magda::sunroom
