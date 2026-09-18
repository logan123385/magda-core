#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <atomic>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <vector>

#include "api/clip_api.hpp"
#include "api/magda_api.hpp"
#include "api/project_api.hpp"
#include "api/remote_api.hpp"
#include "api/track_api.hpp"
#include "audio/AudioBridge.hpp"
#include "core/TrackManager.hpp"
#include "core/ClipManager.hpp"
#include "core/RackInfo.hpp"
#include "core/SelectionManager.hpp"
#include "core/UndoManager.hpp"
#include "sunroom/SunroomActions.hpp"
#include "../../agents/sunroom_mlx_client.hpp"
#include "engine/AudioEngine.hpp"
#include "project/ProjectInfo.hpp"
#include "project/ProjectManager.hpp"
#include "version.hpp"

namespace {

void printUsage(std::ostream& out);

juce::File fileFromArg(const juce::String& path) {
    if (juce::File::isAbsolutePath(path))
        return juce::File(path);
    return juce::File::getCurrentWorkingDirectory().getChildFile(path);
}

juce::File defaultOutputFor(const juce::File& input) {
    auto parent = input.getParentDirectory();
    auto stem = input.getFileNameWithoutExtension() + "_roundtrip";
    return parent.getChildFile(stem + ".mgd");
}

class HeadlessEngineSession {
  public:
    bool initialize() {
        engine_ = magda::createDefaultAudioEngine({.headless = true});
        if (!engine_->initialize()) {
            error_ = "Failed to initialize MAGDA engine";
            engine_.reset();
            return false;
        }
        return true;
    }

    magda::AudioEngine& engine() {
        return *engine_;
    }

    const juce::String& error() const {
        return error_;
    }

  private:
    std::unique_ptr<magda::AudioEngine> engine_;
    juce::String error_;
};

bool restoreProjectTiming(magda::AudioEngine& engine, const magda::ProjectInfo& info) {
    engine.setTempo(info.tempo);
    engine.setTimeSignature(info.timeSignatureNumerator, info.timeSignatureDenominator);
    return true;
}

std::optional<double> parseDouble(const juce::String& text) {
    const auto input = text.trim().toStdString();
    if (input.empty())
        return std::nullopt;

    double value = 0.0;
    char trailing = 0;
    std::istringstream stream(input);
    if (!(stream >> value) || (stream >> trailing) || !std::isfinite(value))
        return std::nullopt;
    return value;
}

std::optional<int> parseInt(const juce::String& text) {
    const auto input = text.trim().toStdString();
    if (input.empty())
        return std::nullopt;

    int value = 0;
    char trailing = 0;
    std::istringstream stream(input);
    if (!(stream >> value) || (stream >> trailing))
        return std::nullopt;
    return value;
}

std::optional<int> parsePositiveInt(const juce::String& text) {
    auto value = parseInt(text);
    if (value && *value > 0)
        return value;
    return std::nullopt;
}

std::optional<magda::TrackType> parseTrackType(const juce::String& text) {
    auto normalized = text.trim().toLowerCase();
    if (normalized == "audio" || normalized == "midi")
        return magda::TrackType::Audio;
    if (normalized == "group")
        return magda::TrackType::Group;
    if (normalized == "aux")
        return magda::TrackType::Aux;
    if (normalized == "chord")
        return magda::TrackType::Chord;
    return std::nullopt;
}

juce::var clipToJson(const magda::ClipInfo& clip) {
    return magda::remote::toJson(magda::remote::makeClipDto(clip));
}

juce::var trackToJson(magda::MagdaApi& api, const magda::TrackInfo& track) {
    auto value = magda::remote::toJson(magda::remote::makeTrackDto(track));
    auto* obj = value.getDynamicObject();

    juce::Array<juce::var> clips;
    for (auto clipId : api.clips().getClipsOnTrack(track.id)) {
        if (auto* clip = api.clips().getClip(clipId))
            clips.add(clipToJson(*clip));
    }
    obj->setProperty("clips", clips);

    juce::Array<juce::var> sends;
    for (const auto& send : track.sends) {
        auto* sendObj = new juce::DynamicObject();
        sendObj->setProperty("busIndex", send.busIndex);
        sendObj->setProperty("level", send.level);
        sendObj->setProperty("destTrackId", static_cast<int>(send.destTrackId));
        sendObj->setProperty("preFader", send.preFader);
        sends.add(juce::var(sendObj));
    }
    obj->setProperty("sends", sends);
    obj->setProperty("auxBusIndex", track.auxBusIndex);

    juce::Array<juce::var> devices;
    for (const auto& element : track.chain.fxChainElements) {
        if (!magda::isDevice(element))
            continue;
        const auto& device = magda::getDevice(element);
        auto* deviceObj = new juce::DynamicObject();
        deviceObj->setProperty("pluginId", device.pluginId);
        deviceObj->setProperty("name", device.name);
        devices.add(juce::var(deviceObj));
    }
    obj->setProperty("devices", devices);
    return value;
}

juce::String dumpProjectJson(magda::MagdaApi& api) {
    const auto& project = api.project().getCurrentProjectInfo();
    auto value = magda::remote::toJson(magda::remote::makeProjectDto(project));
    auto* root = value.getDynamicObject();

    juce::Array<juce::var> tracks;
    for (const auto& track : api.tracks().getTracks())
        tracks.add(trackToJson(api, track));
    root->setProperty("tracks", tracks);

    return juce::JSON::toString(value, true);
}

struct CommandResult {
    bool ok = true;
    juce::String error;
};

class CommandDispatcher {
  public:
    explicit CommandDispatcher(magda::AudioEngine& engine) : engine_(engine) {}

    struct CommandSpec {
        const char* name;
        const char* usage;
        CommandResult (CommandDispatcher::*handler)(const juce::StringArray&, size_t&);
        bool showInUsage = true;
    };

    static const std::vector<CommandSpec>& commandSpecs() {
        static const std::vector<CommandSpec> specs = {
            {"sunroom-journey",
             "sunroom-journey <mood 0..3> <root 0..11> <bars 8/32/64> <bpm>",
             &CommandDispatcher::sunroomJourney},
            {"fixture-a", "fixture-a", &CommandDispatcher::fixtureA},
            {"fixture-b", "fixture-b", &CommandDispatcher::fixtureB},
            {"fixture-c", "fixture-c", &CommandDispatcher::fixtureC},
            {"place-scene", "place-scene <sceneIndex> <destStartBeats>",
             &CommandDispatcher::placeScene},
            {"space-return", "space-return <sendLevel 0..1>", &CommandDispatcher::spaceReturn},
            {"propose-dsl", "propose-dsl <dsl>", &CommandDispatcher::proposeDsl},
            {"apply-proposal", "apply-proposal [cancelled]", &CommandDispatcher::applyProposal},
            {"select-track", "select-track <track-id>", &CommandDispatcher::selectTrack},
            {"bump-revision", "bump-revision", &CommandDispatcher::bumpRevision},
            {"coach-status", "coach-status", &CommandDispatcher::coachStatus},
            {"coach-stage", "coach-stage <text>", &CommandDispatcher::coachStage},
            {"library-query",
             "library-query <kind|-> <text|-> <key|-> <bpmMin|-> <bpmMax|->",
             &CommandDispatcher::libraryQuery},
            {"add-sample", "add-sample <filename.wav>", &CommandDispatcher::addSample},
            {"create-and-play", "create-and-play", &CommandDispatcher::createAndPlay},
            {"create-song", "create-song", &CommandDispatcher::createSong},
            {"undo", "undo", &CommandDispatcher::undoLast},
            {"redo", "redo", &CommandDispatcher::redoLast},
            {"set-tempo", "set-tempo <bpm>", &CommandDispatcher::setTempo},
            {"add-track", "add-track <audio|group|aux|chord> [name]", &CommandDispatcher::addTrack},
            {"add-internal-instrument", "add-internal-instrument <track-id> <plugin-id> [name]",
             &CommandDispatcher::addInternalInstrument},
            {"delete-track", "delete-track <track-id>", &CommandDispatcher::deleteTrack},
            {"set-track-input", "set-track-input <track-id> <audio|midi> <track:N|device|all|none>",
             &CommandDispatcher::setTrackInput},
            {"group-track", "group-track <child-id> <group-id>", &CommandDispatcher::groupTrack},
            {"ungroup-track", "ungroup-track <child-id>", &CommandDispatcher::ungroupTrack},
            {"route-midi-to", "route-midi-to <source-id> <dest-id>",
             &CommandDispatcher::routeMidiTo},
            {"add-midi-clip", "add-midi-clip <track-id> <start-beats> <length-beats>",
             &CommandDispatcher::addMidiClip},
            {"add-clip", "add-midi-clip <track-id> <start-beats> <length-beats>",
             &CommandDispatcher::addMidiClip, false},
            {"delete-clip", "delete-clip <clip-id>", &CommandDispatcher::deleteClip},
            {"add-midi-note",
             "add-midi-note <clip-id> <start-beats> <note> <length-beats> "
             "[velocity]",
             &CommandDispatcher::addMidiNote},
            {"quantize-notes",
             "quantize-notes <clip-id> <grid-beats> <start|length|both> <all|note-index...>",
             &CommandDispatcher::quantizeNotes},
            {"slice-notes", "slice-notes <clip-id> <subdivisions> <all|note-index...>",
             &CommandDispatcher::sliceNotes},
            {"transpose-midi-clip", "transpose-midi-clip <clip-id> <semitones>",
             &CommandDispatcher::transposeMidiClip},
            {"dump", "dump --json", &CommandDispatcher::dump},
        };
        return specs;
    }

    CommandResult execute(const juce::StringArray& tokens, size_t& index) {
        if (index >= static_cast<size_t>(tokens.size()))
            return {};

        const auto command = tokens[static_cast<int>(index++)];
        for (const auto& spec : commandSpecs())
            if (command == spec.name)
                return (this->*spec.handler)(tokens, index);

        return fail("Unknown command: " + command);
    }

    void dumpJson() {
        std::cout << dumpProjectJson(engine_.getMagdaApi()) << "\n";
    }

  private:
    CommandResult sunroomJourney(const juce::StringArray& tokens, size_t& index) {
        if (index + 4 > static_cast<size_t>(tokens.size()))
            return fail("sunroom-journey needs mood, root, bars, bpm");

        auto mood = parseInt(tokens[static_cast<int>(index++)]);
        auto root = parseInt(tokens[static_cast<int>(index++)]);
        auto bars = parseInt(tokens[static_cast<int>(index++)]);
        auto tempo = parseDouble(tokens[static_cast<int>(index++)]);
        if (!mood || !root || !bars || !tempo || *mood < 0 || *mood > 3 || *root < 0 || *root > 11 ||
            (*bars != 8 && *bars != 32 && *bars != 64) || *tempo < 40 || *tempo > 180)
            return fail("Invalid SUNROOM musical settings");

        magda::sunroom::Options o;
        o.mood = *mood;
        o.root = *root;
        o.bars = *bars;
        o.tempo = *tempo;
        auto& tm = magda::TrackManager::getInstance();
        auto& cm = magda::ClipManager::getInstance();
        auto& undo = magda::UndoManager::getInstance();
        const auto trackCount = tm.getTracks().size(), clipCount = cm.getClips().size();
        // Route through UndoManager so starter insertion is one coherent history unit.
        undo.executeCommand(std::make_unique<magda::sunroom::CreateJourneyCommand>(o, &engine_));
        const auto afterTracks = tm.getTracks().size();
        const auto afterClips = cm.getClips().size();
        if (afterTracks <= trackCount)
            return fail("SUNROOM journey created no tracks");
        if (!undo.undo())
            return fail("SUNROOM undo failed");
        if (tm.getTracks().size() != trackCount || cm.getClips().size() != clipCount)
            return fail("SUNROOM undo invariant failed");
        if (!undo.redo())
            return fail("SUNROOM redo failed");
        if (tm.getTracks().size() != afterTracks || cm.getClips().size() != afterClips)
            return fail("SUNROOM redo invariant failed");
        std::cout << "SUNROOM journey: " << (afterTracks - trackCount) << " tracks, "
                  << (afterClips - clipCount)
                  << " clips; undo/redo identity and counts verified.\n";
        return {};
    }

    CommandResult fixtureA(const juce::StringArray&, size_t&) {
        auto& tm = magda::TrackManager::getInstance();
        auto& cm = magda::ClipManager::getInstance();
        auto& undo = magda::UndoManager::getInstance();
        const auto trackCount = tm.getTracks().size();
        const auto clipCount = cm.getClips().size();

        auto command = std::make_unique<magda::sunroom::CreateFixtureACommand>(&engine_);
        auto* raw = command.get();
        undo.executeCommand(std::move(command));
        if (raw->failed()) {
            const auto reason = raw->failureReason().isNotEmpty()
                                    ? raw->failureReason()
                                    : juce::String("Fixture A failed");
            // Failed commands must not remain undoable history.
            undo.discardLastCommand("Create beginner Fixture A");
            return fail(reason);
        }

        const auto afterTracks = tm.getTracks().size();
        const auto afterClips = cm.getClips().size();
        if (afterTracks <= trackCount || afterClips <= clipCount)
            return fail("Fixture A created no music");
        if (!undo.undo())
            return fail("Fixture A undo failed");
        if (tm.getTracks().size() != trackCount || cm.getClips().size() != clipCount)
            return fail("Fixture A undo invariant failed");
        if (!undo.redo())
            return fail("Fixture A redo failed");
        if (tm.getTracks().size() != afterTracks || cm.getClips().size() != afterClips)
            return fail("Fixture A redo invariant failed");

        std::cout << raw->summary() << " (" << (afterTracks - trackCount) << " tracks, "
                  << (afterClips - clipCount) << " clips); undo/redo verified.\n";
        return {};
    }

    CommandResult fixtureB(const juce::StringArray&, size_t&) {
        auto hasFixture = [] {
            bool drums = false, bass = false, chords = false;
            auto& cm = magda::ClipManager::getInstance();
            for (const auto& track : magda::TrackManager::getInstance().getTracks()) {
                const char* role = track.name == "Drums"   ? "Drums"
                                   : track.name == "Bass"   ? "Bass"
                                   : track.name == "Chords" ? "Chords"
                                                            : nullptr;
                if (role == nullptr)
                    continue;
                for (const auto& clip : cm.getClips()) {
                    if (clip.trackId != track.id || clip.name != juce::String("Fixture A / ") + role)
                        continue;
                    if (track.name == "Drums")
                        drums = true;
                    else if (track.name == "Bass")
                        bass = true;
                    else
                        chords = true;
                }
            }
            return drums && bass && chords;
        };
        if (!hasFixture()) {
            auto a = std::make_unique<magda::sunroom::CreateFixtureACommand>(&engine_);
            auto* rawA = a.get();
            magda::UndoManager::getInstance().executeCommand(std::move(a));
            if (rawA->failed()) {
                const auto reason = rawA->failureReason().isNotEmpty()
                                        ? rawA->failureReason()
                                        : juce::String("Fixture A failed");
                magda::UndoManager::getInstance().discardLastCommand("Create beginner Fixture A");
                return fail(reason);
            }
        }
        auto command = std::make_unique<magda::sunroom::CreateFixtureBCommand>(&engine_);
        auto* raw = command.get();
        magda::UndoManager::getInstance().executeCommand(std::move(command));
        if (raw->failed()) {
            const auto reason = raw->failureReason().isNotEmpty() ? raw->failureReason()
                                                                 : juce::String("Fixture B failed");
            magda::UndoManager::getInstance().discardLastCommand(
                "Create beginner Fixture B sections");
            return fail(reason);
        }
        const auto& info = magda::ProjectManager::getInstance().getCurrentProjectInfo();
        if (info.loopEndBeats < 127.0)
            return fail("Fixture B loop should cover 32 bars");
        bool intro = false, main = false, variation = false, ending = false;
        for (const auto& m : info.markers) {
            intro = intro || m.name == "Intro";
            main = main || m.name == "Main";
            variation = variation || m.name == "Variation";
            ending = ending || m.name == "Ending";
        }
        if (!intro || !main || !variation || !ending)
            return fail("Fixture B missing section markers");
        std::cout << raw->summary() << "\n";
        return {};
    }

    CommandResult fixtureC(const juce::StringArray&, size_t&) {
        auto command = std::make_unique<magda::sunroom::CreateFixtureCCommand>(&engine_);
        auto* raw = command.get();
        magda::UndoManager::getInstance().executeCommand(std::move(command));
        if (raw->failed()) {
            const auto reason = raw->failureReason().isNotEmpty() ? raw->failureReason()
                                                                 : juce::String("Fixture C failed");
            magda::UndoManager::getInstance().discardLastCommand("Create beginner Fixture C");
            return fail(reason);
        }
        bool hasSession = false, hasArr = false, hasPad = false;
        for (const auto& clip : magda::ClipManager::getInstance().getClips()) {
            if (clip.name.contains("Session Pulse"))
                hasSession = clip.view == magda::ClipView::Session;
            if (clip.name.contains("Arrangement Pulse"))
                hasArr = clip.view == magda::ClipView::Arrangement;
            if (clip.name.contains("Arrangement Pad"))
                hasPad = true;
        }
        if (!hasSession || !hasArr || !hasPad)
            return fail("Fixture C missing distinct Session/Arrangement sources");
        std::cout << raw->summary() << "\n";
        return {};
    }

    CommandResult placeScene(const juce::StringArray& args, size_t& index) {
        if (index + 1 >= static_cast<size_t>(args.size()))
            return fail("place-scene requires <sceneIndex> <destStartBeats>");
        auto scene = parseInt(args[static_cast<int>(index++)]);
        auto dest = parseDouble(args[static_cast<int>(index++)]);
        if (!scene || !dest || *dest < 0.0)
            return fail("place-scene requires <sceneIndex> <destStartBeats>");
        auto command =
            std::make_unique<magda::sunroom::PlaceSceneInArrangementCommand>(*scene, *dest);
        auto* raw = command.get();
        magda::UndoManager::getInstance().executeCommand(std::move(command));
        if (raw->failed()) {
            const auto reason = raw->failureReason().isNotEmpty()
                                    ? raw->failureReason()
                                    : juce::String("Place Scene failed");
            magda::UndoManager::getInstance().discardLastCommand("Place Scene in Arrangement");
            return fail(reason);
        }
        std::cout << raw->summary() << "\n";
        return {};
    }

    CommandResult spaceReturn(const juce::StringArray& args, size_t& index) {
        if (index >= static_cast<size_t>(args.size()))
            return fail("space-return requires <sendLevel 0..1>");
        auto level = parseDouble(args[static_cast<int>(index++)]);
        if (!level || *level < 0.0 || *level > 1.0)
            return fail("space-return requires <sendLevel 0..1>");
        auto command =
            std::make_unique<magda::sunroom::ApplySharedSpatialReturnCommand>(static_cast<float>(*level));
        auto* raw = command.get();
        magda::UndoManager::getInstance().executeCommand(std::move(command));
        if (raw->failed()) {
            const auto reason = raw->failureReason().isNotEmpty()
                                    ? raw->failureReason()
                                    : juce::String("Shared Space failed");
            magda::UndoManager::getInstance().discardLastCommand("Apply shared spatial return");
            return fail(reason);
        }
        std::cout << raw->summary() << "\n";
        return {};
    }

    CommandResult proposeDsl(const juce::StringArray& args, size_t& index) {
        if (index >= static_cast<size_t>(args.size()))
            return fail("propose-dsl requires <dsl>");
        const auto dsl = args[static_cast<int>(index++)];
        const auto proposal = magda::sunroom::captureDslProposal(dsl, "mocked specialist");
        std::cout << "Proposal " << static_cast<unsigned long long>(proposal.id) << " revision "
                  << static_cast<unsigned long long>(proposal.mutationRevision) << "\n";
        return {};
    }

    CommandResult applyProposal(const juce::StringArray& args, size_t& index) {
        bool cancelled = false;
        if (index < static_cast<size_t>(args.size()) && args[static_cast<int>(index)] == "cancelled") {
            cancelled = true;
            ++index;
        }
        const auto line = magda::sunroom::applyPendingDslProposal(engine_.getMagdaApi(), cancelled);
        if (line.startsWith("Refused"))
            return fail(line);
        std::cout << line << "\n";
        return {};
    }

    CommandResult selectTrack(const juce::StringArray& args, size_t& index) {
        auto id = parseInt(index < static_cast<size_t>(args.size()) ? args[static_cast<int>(index)]
                                                                   : juce::String());
        if (!id) {
            return fail("select-track requires <track-id>");
        }
        ++index;
        magda::SelectionManager::getInstance().selectTrack(static_cast<magda::TrackId>(*id));
        std::cout << "Selected track " << *id << "\n";
        return {};
    }

    CommandResult bumpRevision(const juce::StringArray&, size_t&) {
        magda::ProjectManager::getInstance().markDirty();
        std::cout << "Revision " << static_cast<unsigned long long>(
                                        magda::ProjectManager::getInstance().mutationRevision())
                  << "\n";
        return {};
    }

    CommandResult coachStatus(const juce::StringArray&, size_t&) {
        std::cout << magda::SunroomMlxClient::localModelStatus() << "\n";
        return {};
    }

    CommandResult coachStage(const juce::StringArray& args, size_t& index) {
        if (index >= static_cast<size_t>(args.size()))
            return fail("coach-stage requires <text>");
        const auto text = args[static_cast<int>(index++)];
        const auto dsl = magda::sunroom::extractCoachDsl(text);
        if (dsl.isEmpty())
            return fail("Refused: coach text has no SUNROOM_DSL action. Music is unchanged.");
        const auto proposal = magda::sunroom::captureDslProposal(
            dsl, "Staged from coach text. Not applied until apply-proposal.");
        std::cout << "Staged " << static_cast<unsigned long long>(proposal.id) << " " << dsl << "\n";
        return {};
    }

    CommandResult libraryQuery(const juce::StringArray& args, size_t& index) {
        if (index + 5 > static_cast<size_t>(args.size()))
            return fail("library-query needs <kind|-> <text|-> <key|-> <bpmMin|-> <bpmMax|->");
        const auto kind = args[static_cast<int>(index++)];
        const auto text = args[static_cast<int>(index++)];
        const auto key = args[static_cast<int>(index++)];
        const auto bpmMin = args[static_cast<int>(index++)];
        const auto bpmMax = args[static_cast<int>(index++)];
        magda::sunroom::StarterQuery query;
        if (kind != "-")
            query.kind = kind;
        if (text != "-")
            query.text = text;
        if (key != "-") {
            auto root = parseInt(key);
            if (!root || *root < 0 || *root > 11)
                return fail("library-query key must be 0..11 or -");
            query.keyRoot = *root;
        }
        if (bpmMin != "-") {
            auto value = parseDouble(bpmMin);
            if (!value)
                return fail("library-query bpmMin must be a number or -");
            query.bpmMin = *value;
        }
        if (bpmMax != "-") {
            auto value = parseDouble(bpmMax);
            if (!value)
                return fail("library-query bpmMax must be a number or -");
            query.bpmMax = *value;
        }
        std::cout << magda::sunroom::formatStarterQuery(
            magda::sunroom::queryStarterCatalog(query));
        return {};
    }

    CommandResult addSample(const juce::StringArray& args, size_t& index) {
        if (index >= static_cast<size_t>(args.size()))
            return fail("add-sample requires <filename.wav>");
        const auto name = args[static_cast<int>(index++)];
        if (!magda::sunroom::resolveStarterFile(name).existsAsFile())
            return fail("Starter sound was not found. Nothing was added.");
        auto command = std::make_unique<magda::sunroom::ImportStarterSampleCommand>(name);
        auto* raw = command.get();
        magda::UndoManager::getInstance().executeCommand(std::move(command));
        if (raw->failed()) {
            const auto reason = raw->failureReason();
            if (magda::UndoManager::getInstance().canUndo() &&
                magda::UndoManager::getInstance().getUndoDescription() == "Add starter sound")
                magda::UndoManager::getInstance().undo();
            return fail(reason);
        }
        std::cout << raw->summary() << "\n";
        return {};
    }

    CommandResult createSong(const juce::StringArray& args, size_t& index) {
        auto first = createAndPlay(args, index);
        if (!first.ok)
            return first;
        return fixtureB(args, index);
    }

    CommandResult createAndPlay(const juce::StringArray&, size_t&) {
        auto hasFixture = [] {
            bool drums = false, bass = false, chords = false;
            auto& cm = magda::ClipManager::getInstance();
            for (const auto& track : magda::TrackManager::getInstance().getTracks()) {
                const char* role = track.name == "Drums"   ? "Drums"
                                   : track.name == "Bass"   ? "Bass"
                                   : track.name == "Chords" ? "Chords"
                                                            : nullptr;
                if (role == nullptr)
                    continue;
                for (const auto& clip : cm.getClips()) {
                    if (clip.trackId != track.id || clip.name != juce::String("Fixture A / ") + role)
                        continue;
                    if (track.name == "Drums")
                        drums = true;
                    else if (track.name == "Bass")
                        bass = true;
                    else
                        chords = true;
                }
            }
            return drums && bass && chords;
        };

        if (hasFixture()) {
            std::cout << "Create and Play: starter already present; no duplicate insert.\n";
            return {};
        }

        auto command = std::make_unique<magda::sunroom::CreateFixtureACommand>(&engine_);
        auto* raw = command.get();
        magda::UndoManager::getInstance().executeCommand(std::move(command));
        if (raw->failed()) {
            const auto reason = raw->failureReason().isNotEmpty()
                                    ? raw->failureReason()
                                    : juce::String("Create and Play failed");
            magda::UndoManager::getInstance().discardLastCommand("Create beginner Fixture A");
            return fail(reason);
        }
        if (!hasFixture())
            return fail("Create and Play did not insert Fixture A tracks");
        std::cout << raw->summary() << " (create-and-play)\n";
        return {};
    }

    CommandResult undoLast(const juce::StringArray&, size_t&) {
        auto& undo = magda::UndoManager::getInstance();
        if (!undo.canUndo())
            return fail("Nothing to undo");
        if (!undo.undo())
            return fail("Undo failed");
        std::cout << "Undid: " << undo.getRedoDescription() << "\n";
        return {};
    }
    CommandResult redoLast(const juce::StringArray&, size_t&) {
        auto& undo = magda::UndoManager::getInstance();
        if (!undo.canRedo())
            return fail("Nothing to redo");
        if (!undo.redo())
            return fail("Redo failed");
        std::cout << "Redid: " << undo.getUndoDescription() << "\n";
        return {};
    }
    CommandResult setTempo(const juce::StringArray& tokens, size_t& index) {
        if (index >= static_cast<size_t>(tokens.size()))
            return fail("set-tempo requires <bpm>");
        auto bpm = parseDouble(tokens[static_cast<int>(index++)]);
        if (!bpm || *bpm <= 0.0)
            return fail("set-tempo requires a positive numeric bpm");

        engine_.getMagdaApi().project().setTempo(*bpm);
        return {};
    }

    CommandResult addTrack(const juce::StringArray& tokens, size_t& index) {
        if (index >= static_cast<size_t>(tokens.size()))
            return fail("add-track requires <audio|group|aux|chord>");

        const auto typeToken = tokens[static_cast<int>(index++)];
        auto type = parseTrackType(typeToken);
        if (!type)
            return fail("Unsupported track type: " + typeToken);

        juce::String name = juce::String(magda::getTrackTypeName(*type));
        if (index < static_cast<size_t>(tokens.size()) &&
            !isCommand(tokens[static_cast<int>(index)]))
            name = tokens[static_cast<int>(index++)];

        auto id = engine_.getMagdaApi().tracks().createTrack(name, *type);
        std::cout << "track " << id << "\n";
        return {};
    }

    CommandResult deleteTrack(const juce::StringArray& tokens, size_t& index) {
        auto trackId = takeInt(tokens, index, "delete-track requires <track-id>");
        if (!trackId)
            return fail(lastParseError_);
        engine_.getMagdaApi().tracks().deleteTrack(*trackId);
        return {};
    }

    CommandResult setTrackInput(const juce::StringArray& tokens, size_t& index) {
        auto trackId = takeInt(tokens, index, "set-track-input requires <track-id>");
        if (!trackId)
            return fail(lastParseError_);
        if (index >= static_cast<size_t>(tokens.size()))
            return fail("set-track-input requires <audio|midi>");

        const auto kind = tokens[static_cast<int>(index++)].trim().toLowerCase();
        if (kind != "audio" && kind != "midi")
            return fail("set-track-input kind must be audio or midi");
        if (index >= static_cast<size_t>(tokens.size()))
            return fail("set-track-input requires <track:N|device|all|none>");

        auto value = tokens[static_cast<int>(index++)];
        if (value == "none")
            value = juce::String();

        // TrackApi has no input-routing setters yet, so reach TrackManager
        // directly (the same singleton path the CLI uses for ProjectManager).
        auto& trackManager = magda::TrackManager::getInstance();
        if (kind == "audio")
            trackManager.setTrackAudioInput(*trackId, value);
        else
            trackManager.setTrackMidiInput(*trackId, value);

        const auto* track = engine_.getMagdaApi().tracks().getTrack(*trackId);
        if (track == nullptr)
            return fail("set-track-input: unknown track " + juce::String(*trackId));

        const auto& applied = kind == "audio" ? track->audioInputDevice : track->midiInputDevice;
        if (applied != value)
            return fail("set-track-input rejected: track " + juce::String(*trackId) + " " + kind +
                        " input is \"" + applied + "\"");

        std::cout << "input " << *trackId << " " << kind << " "
                  << (value.isEmpty() ? juce::String("none") : value) << "\n";
        return {};
    }

    CommandResult groupTrack(const juce::StringArray& tokens, size_t& index) {
        auto childId = takeInt(tokens, index, "group-track requires <child-id>");
        if (!childId)
            return fail(lastParseError_);
        auto groupId = takeInt(tokens, index, "group-track requires <group-id>");
        if (!groupId)
            return fail(lastParseError_);

        // TrackApi has no grouping setters yet, so reach TrackManager directly
        // (same singleton path as set-track-input).
        auto& trackManager = magda::TrackManager::getInstance();
        if (trackManager.getTrack(*childId) == nullptr)
            return fail("group-track: unknown track " + juce::String(*childId));
        if (trackManager.getTrack(*groupId) == nullptr)
            return fail("group-track: unknown track " + juce::String(*groupId));

        trackManager.addTrackToGroup(*childId, *groupId);

        const auto* child = trackManager.getTrack(*childId);
        if (child == nullptr || child->parentId != *groupId)
            return fail("group-track rejected: track " + juce::String(*childId) +
                        " is not a child of track " + juce::String(*groupId));

        std::cout << "group " << *childId << " " << *groupId << "\n";
        return {};
    }

    CommandResult ungroupTrack(const juce::StringArray& tokens, size_t& index) {
        auto childId = takeInt(tokens, index, "ungroup-track requires <child-id>");
        if (!childId)
            return fail(lastParseError_);

        auto& trackManager = magda::TrackManager::getInstance();
        if (trackManager.getTrack(*childId) == nullptr)
            return fail("ungroup-track: unknown track " + juce::String(*childId));

        trackManager.removeTrackFromGroup(*childId);
        std::cout << "ungroup " << *childId << "\n";
        return {};
    }

    CommandResult routeMidiTo(const juce::StringArray& tokens, size_t& index) {
        auto sourceId = takeInt(tokens, index, "route-midi-to requires <source-id>");
        if (!sourceId)
            return fail(lastParseError_);
        auto destId = takeInt(tokens, index, "route-midi-to requires <dest-id>");
        if (!destId)
            return fail(lastParseError_);

        auto& trackManager = magda::TrackManager::getInstance();
        trackManager.routeMidiOutputToTrack(*sourceId, *destId);

        const auto* dest = trackManager.getTrack(*destId);
        if (dest == nullptr)
            return fail("route-midi-to: unknown track " + juce::String(*destId));

        // The model rejects self / unknown / cycle silently, so verify the
        // routing actually landed (mirrors the set-track-input convention).
        const auto expected = "track:" + juce::String(*sourceId);
        if (dest->midiInputDevice != expected)
            return fail("route-midi-to rejected: track " + juce::String(*destId) +
                        " midi input is \"" + dest->midiInputDevice + "\"");

        std::cout << "midi-to " << *sourceId << " " << *destId << "\n";
        return {};
    }

    CommandResult addInternalInstrument(const juce::StringArray& tokens, size_t& index) {
        auto trackId = takeInt(tokens, index, "add-internal-instrument requires <track-id>");
        if (!trackId)
            return fail(lastParseError_);
        if (index >= static_cast<size_t>(tokens.size()))
            return fail("add-internal-instrument requires <plugin-id>");

        const auto pluginId = tokens[static_cast<int>(index++)];
        auto name = pluginId;
        if (index < static_cast<size_t>(tokens.size()) &&
            !isCommand(tokens[static_cast<int>(index)]))
            name = tokens[static_cast<int>(index++)];

        magda::DeviceInfo device;
        device.name = name;
        device.manufacturer = "MAGDA";
        device.pluginId = pluginId;
        device.uniqueId = pluginId;
        device.fileOrIdentifier = pluginId;
        device.isInstrument = true;
        device.deviceType = magda::DeviceType::Instrument;
        device.format = magda::PluginFormat::Internal;

        const auto deviceId = engine_.getMagdaApi().tracks().addDeviceToTrack(*trackId, device);
        if (deviceId == magda::INVALID_DEVICE_ID)
            return fail("Failed to add internal instrument");
        std::cout << "device " << deviceId << "\n";
        return {};
    }

    CommandResult addMidiClip(const juce::StringArray& tokens, size_t& index) {
        auto trackId = takeInt(tokens, index, "add-midi-clip requires <track-id>");
        if (!trackId)
            return fail(lastParseError_);
        auto start = takeDouble(tokens, index, "add-midi-clip requires <start-beats>");
        if (!start)
            return fail(lastParseError_);
        auto length = takeDouble(tokens, index, "add-midi-clip requires <length-beats>");
        if (!length)
            return fail(lastParseError_);
        if (*length <= 0.0)
            return fail("add-midi-clip requires a positive length");

        auto id = engine_.getMagdaApi().clips().createMidiClipBeats(*trackId, *start, *length);
        std::cout << "clip " << id << "\n";
        return {};
    }

    CommandResult deleteClip(const juce::StringArray& tokens, size_t& index) {
        auto clipId = takeInt(tokens, index, "delete-clip requires <clip-id>");
        if (!clipId)
            return fail(lastParseError_);
        engine_.getMagdaApi().clips().deleteClip(*clipId);
        return {};
    }

    CommandResult addMidiNote(const juce::StringArray& tokens, size_t& index) {
        auto clipId = takeInt(tokens, index, "add-midi-note requires <clip-id>");
        if (!clipId)
            return fail(lastParseError_);
        auto start = takeDouble(tokens, index, "add-midi-note requires <start-beats>");
        if (!start)
            return fail(lastParseError_);
        auto noteNumber = takeInt(tokens, index, "add-midi-note requires <note>");
        if (!noteNumber)
            return fail(lastParseError_);
        auto length = takeDouble(tokens, index, "add-midi-note requires <length-beats>");
        if (!length)
            return fail(lastParseError_);

        int velocity = 100;
        if (index < static_cast<size_t>(tokens.size()) &&
            !isCommand(tokens[static_cast<int>(index)])) {
            auto parsedVelocity = parseInt(tokens[static_cast<int>(index)]);
            if (!parsedVelocity)
                return fail("add-midi-note velocity must be an integer");
            velocity = juce::jlimit(0, 127, *parsedVelocity);
            ++index;
        }

        if (!engine_.getMagdaApi().clips().addMidiNote(*clipId, *start, *noteNumber, *length,
                                                       velocity))
            return fail("Failed to add MIDI note");
        std::cout << "note\n";
        return {};
    }

    CommandResult quantizeNotes(const juce::StringArray& tokens, size_t& index) {
        auto clipId = takeInt(tokens, index, "quantize-notes requires <clip-id>");
        if (!clipId)
            return fail(lastParseError_);
        auto grid = takeDouble(tokens, index, "quantize-notes requires <grid-beats>");
        if (!grid)
            return fail(lastParseError_);
        if (index >= static_cast<size_t>(tokens.size()))
            return fail("quantize-notes requires <start|length|both>");

        auto mode = parseQuantizeMode(tokens[static_cast<int>(index++)]);
        if (!mode)
            return fail("quantize-notes mode must be start, length, or both");

        auto indices = takeNoteIndices(tokens, index, *clipId, "quantize-notes");
        if (indices.empty())
            return fail("quantize-notes requires at least one note index or all");

        if (!engine_.getMagdaApi().clips().quantizeMidiNotes(*clipId, indices, *grid, *mode))
            return fail("Failed to quantize MIDI notes");
        return {};
    }

    CommandResult sliceNotes(const juce::StringArray& tokens, size_t& index) {
        auto clipId = takeInt(tokens, index, "slice-notes requires <clip-id>");
        if (!clipId)
            return fail(lastParseError_);
        auto subdivisions = takeInt(tokens, index, "slice-notes requires <subdivisions>");
        if (!subdivisions)
            return fail(lastParseError_);

        auto indices = takeNoteIndices(tokens, index, *clipId, "slice-notes");
        if (indices.empty())
            return fail("slice-notes requires at least one note index or all");

        if (!engine_.getMagdaApi().clips().sliceMidiNotes(*clipId, indices, *subdivisions))
            return fail("Failed to slice MIDI notes");
        return {};
    }

    CommandResult transposeMidiClip(const juce::StringArray& tokens, size_t& index) {
        auto clipId = takeInt(tokens, index, "transpose-midi-clip requires <clip-id>");
        if (!clipId)
            return fail(lastParseError_);
        auto semitones = takeInt(tokens, index, "transpose-midi-clip requires <semitones>");
        if (!semitones)
            return fail(lastParseError_);

        if (!engine_.getMagdaApi().clips().transposeMidiClip(*clipId, *semitones))
            return fail("Failed to transpose MIDI clip");
        return {};
    }

    CommandResult dump(const juce::StringArray& tokens, size_t& index) {
        if (index >= static_cast<size_t>(tokens.size()) ||
            tokens[static_cast<int>(index)] != "--json")
            return fail("dump requires --json");
        ++index;
        dumpJson();
        return {};
    }

    static bool isCommand(const juce::String& token) {
        for (const auto& spec : commandSpecs())
            if (token == spec.name)
                return true;
        return token.startsWith("--");
    }

    static std::optional<magda::MidiNoteQuantizeMode> parseQuantizeMode(const juce::String& token) {
        auto normalized = token.trim().toLowerCase();
        if (normalized == "start")
            return magda::MidiNoteQuantizeMode::StartOnly;
        if (normalized == "length")
            return magda::MidiNoteQuantizeMode::LengthOnly;
        if (normalized == "both" || normalized == "start-and-length")
            return magda::MidiNoteQuantizeMode::StartAndLength;
        return std::nullopt;
    }

    std::vector<size_t> takeNoteIndices(const juce::StringArray& tokens, size_t& index,
                                        magda::ClipId clipId, const juce::String& commandName) {
        std::vector<size_t> indices;
        if (index >= static_cast<size_t>(tokens.size()) ||
            isCommand(tokens[static_cast<int>(index)])) {
            lastParseError_ = commandName + " requires <all|note-index...>";
            return indices;
        }

        if (tokens[static_cast<int>(index)] == "all") {
            ++index;
            if (auto* clip = engine_.getMagdaApi().clips().getClip(clipId)) {
                indices.reserve(clip->midiNotes.size());
                for (size_t i = 0; i < clip->midiNotes.size(); ++i)
                    indices.push_back(i);
            }
            return indices;
        }

        while (index < static_cast<size_t>(tokens.size()) &&
               !isCommand(tokens[static_cast<int>(index)])) {
            auto parsed = parseInt(tokens[static_cast<int>(index)]);
            if (!parsed || *parsed < 0) {
                lastParseError_ = commandName + " note indices must be non-negative integers";
                indices.clear();
                return indices;
            }
            indices.push_back(static_cast<size_t>(*parsed));
            ++index;
        }
        return indices;
    }

    std::optional<int> takeInt(const juce::StringArray& tokens, size_t& index,
                               const juce::String& missingError) {
        if (index >= static_cast<size_t>(tokens.size())) {
            lastParseError_ = missingError;
            return std::nullopt;
        }
        auto value = parseInt(tokens[static_cast<int>(index++)]);
        if (!value)
            lastParseError_ = "Expected integer argument";
        return value;
    }

    std::optional<double> takeDouble(const juce::StringArray& tokens, size_t& index,
                                     const juce::String& missingError) {
        if (index >= static_cast<size_t>(tokens.size())) {
            lastParseError_ = missingError;
            return std::nullopt;
        }
        auto value = parseDouble(tokens[static_cast<int>(index++)]);
        if (!value)
            lastParseError_ = "Expected numeric argument";
        return value;
    }

    static CommandResult fail(const juce::String& error) {
        return {false, error};
    }

    magda::AudioEngine& engine_;
    juce::String lastParseError_;
};

void printUsage(std::ostream& out) {
    out << "magda-cli " << MAGDA_VERSION << "\n"
        << "\n"
        << "Usage:\n"
        << "  magda-cli boot\n"
        << "  magda-cli init <out.mgd>\n"
        << "  magda-cli run <project.mgd> [--out <out.mgd>]\n"
        << "  magda-cli run <project.mgd> --cmds <cmds.txt> [--out <out.mgd>] [--dump-json]\n"
        << "  magda-cli exec <project.mgd> <commands...> [--out <out.mgd>] [--dump-json]\n"
        << "  magda-cli render <project.mgd> --wav <out.wav> [--from <time>] [--to <time>]\n"
        << "\n"
        << "Commands:\n";

    for (const auto& spec : CommandDispatcher::commandSpecs())
        if (spec.showInUsage)
            out << "  " << spec.usage << "\n";
}

struct RunOptions {
    juce::File input;
    juce::File output;
    juce::File commandFile;
    bool hasCommandFile = false;
    bool dumpJson = false;
    juce::StringArray execTokens;
};

struct RenderOptions {
    juce::File input;
    juce::File wavOutput;
    std::optional<double> fromSeconds;
    std::optional<double> toSeconds;
    std::optional<double> sampleRate;
    std::optional<int> bitDepth;
};

bool loadProjectForCli(const juce::File& input, HeadlessEngineSession& session) {
    auto& projectManager = magda::ProjectManager::getInstance();
    if (!projectManager.loadProject(input, [&session](const magda::ProjectInfo& info) {
            restoreProjectTiming(session.engine(), info);
        })) {
        std::cerr << "Failed to load project: " << projectManager.getLastError() << "\n";
        return false;
    }
    return true;
}

std::optional<double> parseRenderTime(const juce::String& text, const magda::ProjectInfo& info) {
    auto token = text.trim().toLowerCase();
    if (token.endsWith("bars")) {
        auto bars = parseDouble(token.dropLastCharacters(4));
        if (!bars)
            return std::nullopt;
        const double beats = *bars * info.timeSignatureNumerator;
        return beats * 60.0 / info.tempo;
    }
    if (token.endsWith("bar")) {
        auto bars = parseDouble(token.dropLastCharacters(3));
        if (!bars)
            return std::nullopt;
        const double beats = *bars * info.timeSignatureNumerator;
        return beats * 60.0 / info.tempo;
    }
    if (token.endsWith("beats")) {
        auto beats = parseDouble(token.dropLastCharacters(5));
        if (!beats)
            return std::nullopt;
        return *beats * 60.0 / info.tempo;
    }
    if (token.endsWith("beat")) {
        auto beats = parseDouble(token.dropLastCharacters(4));
        if (!beats)
            return std::nullopt;
        return *beats * 60.0 / info.tempo;
    }
    if (token.endsWith("s"))
        token = token.dropLastCharacters(1);
    return parseDouble(token);
}

double defaultRenderEndSeconds(magda::AudioEngine& engine, const magda::ProjectInfo& info) {
    const auto editLength = engine.getEditLengthBeats();
    if (editLength.value > 0.0)
        if (const auto* tempoMap = engine.tempoMap())
            return tempoMap->beatToTime(editLength.value);

    const double beats = static_cast<double>(info.timelineLengthBars) * info.timeSignatureNumerator;
    const double seconds = beats * 60.0 / info.tempo;
    return juce::jmax(1.0, seconds);
}

bool prepareOutputFile(const juce::File& file) {
    if (!file.getParentDirectory().createDirectory()) {
        std::cerr << "Failed to create output directory: "
                  << file.getParentDirectory().getFullPathName() << "\n";
        return false;
    }

    if (file.exists() && !file.deleteFile()) {
        std::cerr << "Failed to replace output file: " << file.getFullPathName() << "\n";
        return false;
    }

    return true;
}

bool renderWav(magda::AudioEngine& engine, const RenderOptions& options) {
    if (!engine.hasActiveEdit()) {
        std::cerr << "No edit is loaded for rendering\n";
        return false;
    }

    if (!prepareOutputFile(options.wavOutput))
        return false;

    const auto& projectInfo = magda::ProjectManager::getInstance().getCurrentProjectInfo();
    const double startSeconds = options.fromSeconds.value_or(0.0);
    const double endSeconds =
        options.toSeconds.value_or(defaultRenderEndSeconds(engine, projectInfo));
    if (startSeconds < 0.0 || endSeconds <= startSeconds) {
        std::cerr << "Render range must have --to greater than --from\n";
        return false;
    }

    auto session = engine.createOfflineRenderSession(false);
    auto task = session ? session->createTask({
                              .destination = options.wavOutput,
                              .format = magda::OfflineRenderFormat::Wav,
                              .bitDepth = options.bitDepth.value_or(projectInfo.renderBitDepth),
                              .sampleRate = options.sampleRate.value_or(projectInfo.sampleRate),
                              .blockSize = 512,
                              .shouldNormalise = false,
                              .useMasterPlugins = true,
                              .usePlugins = true,
                              .checkNodesForAudio = false,
                              .realTimeRender = false,
                              .range = {{startSeconds}, {endSeconds}, {}},
                          })
                        : nullptr;
    if (task == nullptr) {
        std::cerr << "Audio engine does not support offline rendering\n";
        return false;
    }

    const auto result = task->run();
    if (!result.success) {
        std::cerr << "Render failed: " << result.error << "\n";
        return false;
    }
    if (!options.wavOutput.existsAsFile() || options.wavOutput.getSize() <= 0) {
        std::cerr << "Render did not produce a WAV file: " << options.wavOutput.getFullPathName()
                  << "\n";
        return false;
    }

    std::cout << "Rendered " << options.wavOutput.getFullPathName() << "\n";
    return true;
}

bool saveProjectForCli(const juce::File& output) {
    auto& projectManager = magda::ProjectManager::getInstance();
    if (!projectManager.saveProjectAs(output)) {
        std::cerr << "Failed to save project: " << projectManager.getLastError() << "\n";
        return false;
    }
    std::cout << "Saved " << projectManager.getCurrentProjectFile().getFullPathName() << "\n"
              << std::flush;
    return true;
}

bool executeCommandTokens(CommandDispatcher& dispatcher, const juce::StringArray& tokens) {
    size_t index = 0;
    while (index < static_cast<size_t>(tokens.size())) {
        auto result = dispatcher.execute(tokens, index);
        if (!result.ok) {
            std::cerr << result.error << "\n";
            return false;
        }
    }
    return true;
}

bool executeCommandFile(CommandDispatcher& dispatcher, const juce::File& file) {
    if (!file.existsAsFile()) {
        std::cerr << "Command file does not exist: " << file.getFullPathName() << "\n";
        return false;
    }

    juce::StringArray lines;
    lines.addLines(file.loadFileAsString());
    for (auto line : lines) {
        line = line.trim();
        if (line.isEmpty() || line.startsWith("#"))
            continue;

        juce::StringArray tokens;
        tokens.addTokens(line, true);
        if (!executeCommandTokens(dispatcher, tokens))
            return false;
    }
    return true;
}

int runCli(const RunOptions& options) {
    HeadlessEngineSession session;
    if (!session.initialize()) {
        std::cerr << session.error() << "\n";
        return 1;
    }

    if (!loadProjectForCli(options.input, session))
        return 1;

    CommandDispatcher dispatcher(session.engine());
    if (options.hasCommandFile && !executeCommandFile(dispatcher, options.commandFile))
        return 1;
    if (!options.execTokens.isEmpty() && !executeCommandTokens(dispatcher, options.execTokens))
        return 1;
    if (options.dumpJson)
        dispatcher.dumpJson();

    if (!saveProjectForCli(options.output))
        return 1;

    // Skip JUCE/Tracktion static teardown in this headless CLI process after a
    // successful durable save (destructor path currently SIGSEGVs on exit).
    std::cout.flush();
    std::_Exit(0);
}

int initProject(const juce::StringArray& args) {
    if (args.size() != 2) {
        printUsage(std::cerr);
        return 2;
    }

    HeadlessEngineSession session;
    if (!session.initialize()) {
        std::cerr << session.error() << "\n";
        return 1;
    }

    if (!magda::ProjectManager::getInstance().newProject()) {
        std::cerr << "Failed to create project: "
                  << magda::ProjectManager::getInstance().getLastError() << "\n";
        return 1;
    }

    if (!saveProjectForCli(fileFromArg(args[1])))
        return 1;

    // Avoid Tracktion/JUCE teardown crashes in this headless CLI build when the
    // process exits after a successful save. The project file is already durable.
    std::cout.flush();
    std::_Exit(0);
}

int runRoundTrip(const juce::StringArray& args) {
    if (args.size() < 2) {
        printUsage(std::cerr);
        return 2;
    }

    RunOptions options;
    options.input = fileFromArg(args[1]);
    options.output = defaultOutputFor(options.input);

    for (int i = 2; i < args.size(); ++i) {
        if (args[i] == "--out") {
            if (++i >= args.size()) {
                printUsage(std::cerr);
                return 2;
            }
            options.output = fileFromArg(args[i]);
        } else if (args[i] == "--cmds") {
            if (++i >= args.size()) {
                printUsage(std::cerr);
                return 2;
            }
            options.commandFile = fileFromArg(args[i]);
            options.hasCommandFile = true;
        } else if (args[i] == "--dump-json") {
            options.dumpJson = true;
        } else {
            printUsage(std::cerr);
            return 2;
        }
    }

    return runCli(options);
}

int renderProject(const juce::StringArray& args) {
    if (args.size() < 4) {
        printUsage(std::cerr);
        return 2;
    }

    RenderOptions options;
    options.input = fileFromArg(args[1]);

    juce::String fromToken;
    juce::String toToken;
    for (int i = 2; i < args.size(); ++i) {
        if (args[i] == "--wav") {
            if (++i >= args.size()) {
                printUsage(std::cerr);
                return 2;
            }
            options.wavOutput = fileFromArg(args[i]);
        } else if (args[i] == "--from") {
            if (++i >= args.size()) {
                printUsage(std::cerr);
                return 2;
            }
            fromToken = args[i];
        } else if (args[i] == "--to") {
            if (++i >= args.size()) {
                printUsage(std::cerr);
                return 2;
            }
            toToken = args[i];
        } else if (args[i] == "--sample-rate") {
            if (++i >= args.size()) {
                printUsage(std::cerr);
                return 2;
            }
            options.sampleRate = parseDouble(args[i]);
            if (!options.sampleRate || *options.sampleRate <= 0.0) {
                std::cerr << "--sample-rate requires a positive number\n";
                return 2;
            }
        } else if (args[i] == "--bit-depth") {
            if (++i >= args.size()) {
                printUsage(std::cerr);
                return 2;
            }
            options.bitDepth = parsePositiveInt(args[i]);
            if (!options.bitDepth) {
                std::cerr << "--bit-depth requires a positive integer\n";
                return 2;
            }
        } else {
            printUsage(std::cerr);
            return 2;
        }
    }

    if (options.wavOutput.getFullPathName().isEmpty()) {
        std::cerr << "render requires --wav <out.wav>\n";
        return 2;
    }

    HeadlessEngineSession session;
    if (!session.initialize()) {
        std::cerr << session.error() << "\n";
        return 1;
    }

    if (!loadProjectForCli(options.input, session))
        return 1;

    const auto& info = magda::ProjectManager::getInstance().getCurrentProjectInfo();
    if (fromToken.isNotEmpty()) {
        options.fromSeconds = parseRenderTime(fromToken, info);
        if (!options.fromSeconds) {
            std::cerr << "Invalid --from time: " << fromToken << "\n";
            return 2;
        }
    }
    if (toToken.isNotEmpty()) {
        options.toSeconds = parseRenderTime(toToken, info);
        if (!options.toSeconds) {
            std::cerr << "Invalid --to time: " << toToken << "\n";
            return 2;
        }
    }

    return renderWav(session.engine(), options) ? 0 : 1;
}

int execCommands(const juce::StringArray& args) {
    if (args.size() < 3) {
        printUsage(std::cerr);
        return 2;
    }

    RunOptions options;
    options.input = fileFromArg(args[1]);
    options.output = defaultOutputFor(options.input);

    for (int i = 2; i < args.size(); ++i) {
        if (args[i] == "--out") {
            if (++i >= args.size()) {
                printUsage(std::cerr);
                return 2;
            }
            options.output = fileFromArg(args[i]);
        } else if (args[i] == "--dump-json") {
            options.dumpJson = true;
        } else {
            options.execTokens.add(args[i]);
        }
    }

    if (options.execTokens.isEmpty() && !options.dumpJson) {
        printUsage(std::cerr);
        return 2;
    }

    return runCli(options);
}

int bootOnly() {
    HeadlessEngineSession session;
    if (!session.initialize()) {
        std::cerr << session.error() << "\n";
        return 1;
    }

    std::cout << "MAGDA engine booted headless\n";
    std::cout.flush();
    std::_Exit(0);
}

}  // namespace

int main(int argc, char* argv[]) {
    juce::ScopedJuceInitialiser_GUI juceInit;

    juce::StringArray args;
    for (int i = 0; i < argc; ++i)
        args.add(juce::String(argv[i]));

    if (args.size() < 2 || args[1] == "--help" || args[1] == "-h") {
        printUsage(args.size() < 2 ? std::cerr : std::cout);
        return args.size() < 2 ? 2 : 0;
    }

    const auto command = args[1];
    args.remove(0);

    if (command == "boot")
        return bootOnly();
    if (command == "init")
        return initProject(args);
    if (command == "run")
        return runRoundTrip(args);
    if (command == "exec")
        return execCommands(args);
    if (command == "render")
        return renderProject(args);

    std::cerr << "Unknown command: " << command << "\n";
    printUsage(std::cerr);
    return 2;
}
