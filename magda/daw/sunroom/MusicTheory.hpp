#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

// SUNROOM's guide is deliberately independent of the audio engine. The same
// rules drive the note garden, composition, piano-roll colours and tests.
namespace magda::sunroom {

struct Mood {
    const char* name;
    const char* description;
    const char* scaleName;
    std::array<int, 7> intervals;
    int suggestedRoot;
    int suggestedTempo;
};
inline constexpr std::array<Mood, 4> moods{{
    {"Floating", "Weightless, open, quietly hopeful", "Dorian", {0, 2, 3, 5, 7, 9, 10}, 2, 84},
    {"Deep space",
     "Dreamy, inward, a little mysterious",
     "Natural minor",
     {0, 2, 3, 5, 7, 8, 10},
     9,
     72},
    {"Golden hour", "Warm light with a curious shimmer", "Lydian", {0, 2, 4, 6, 7, 9, 11}, 5, 90},
    {"Earth ritual",
     "Grounded pulse, shadow and wonder",
     "Phrygian",
     {0, 1, 3, 5, 7, 8, 10},
     4,
     96},
}};
inline int pitchClass(int note) {
    return (note % 12 + 12) % 12;
}
inline const Mood& moodAt(int index) {
    return moods[static_cast<size_t>(std::clamp(index, 0, 3))];
}
inline const char* noteName(int note) {
    constexpr const char* names[]{"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    return names[pitchClass(note)];
}
inline bool inScale(int note, int root, int mood) {
    const auto& intervals = moodAt(mood).intervals;
    return std::find(intervals.begin(), intervals.end(), pitchClass(note - root)) !=
           intervals.end();
}
/** Snap a new pitched note onto the nearest in-scale pitch. Prefer upward on ties.
 *  Existing out-of-scale notes are never rewritten by this helper — callers apply
 *  it only at note-creation time. Drum lanes must not call it. */
inline int snapToScale(int note, int root, int mood) {
    note = std::clamp(note, 0, 127);
    if (inScale(note, root, mood))
        return note;
    for (int distance = 1; distance <= 6; ++distance) {
        const int up = note + distance;
        if (up <= 127 && inScale(up, root, mood))
            return up;
        const int down = note - distance;
        if (down >= 0 && inScale(down, root, mood))
            return down;
    }
    return note;
}
// A scale degree can cross an octave. Never modulo-wrap it down unexpectedly.
inline int degreeNote(int degree, int root, int mood, int octaveBase = 48) {
    int octave = static_cast<int>(std::floor(degree / 7.0));
    int index = (degree % 7 + 7) % 7;
    return octaveBase + pitchClass(root) + moodAt(mood).intervals[static_cast<size_t>(index)] +
           octave * 12;
}
enum class Feeling { Home, Warmth, Anchor, Float, Pull, Spice };
struct NoteFeeling {
    Feeling kind;
    const char* label;
    const char* analogy;
    uint32_t colour;
};
inline NoteFeeling feeling(int note, int root, int mood) {
    if (!inScale(note, root, mood))
        return {Feeling::Spice, "Spice", "A surprise colour. Try a short visit, then return home.",
                0xff9387a8};
    switch (pitchClass(note - root)) {
        case 0:
            return {Feeling::Home, "Home", "Your feet on the ground. A restful place to finish.",
                    0xffffad66};
        case 3:
        case 4:
            return {Feeling::Warmth, "Warmth",
                    "The emotional light: softer or sunnier beside home.", 0xffed91bd};
        case 7:
            return {Feeling::Anchor, "Anchor",
                    "A steady friend for home. Open, strong and spacious.", 0xffc5a1ff};
        case 2:
        case 5:
        case 9:
            return {Feeling::Float, "Float", "A little air around the chord. Room to drift.",
                    0xff8bd5c5};
        default:
            return {Feeling::Pull, "Pull",
                    "Leaning forward. Let the next note release the feeling.", 0xffffd584};
    }
}
inline std::string pairFeeling(int first, int second) {
    const int interval = pitchClass(second - first);
    if (interval == 0)
        return "Together: the same home in another voice. Very settled.";
    if (interval == 7 || interval == 5)
        return "Together: open sky. These notes leave space between them.";
    if (interval == 3 || interval == 4 || interval == 8 || interval == 9)
        return "Together: a warm conversation. A useful chord-building pair.";
    if (interval == 1 || interval == 11)
        return "Together: close neighbours rubbing shoulders. Strong tension; resolve by a step.";
    if (interval == 6)
        return "Together: a question mark. Mysterious tension asking where to go next.";
    return "Together: a floating question. Try adding home underneath to hear the context.";
}

struct Note {
    int pitch;
    double beat;
    double length;
    int velocity;
};
struct Phrase {
    double start;
    double length;
    std::string name;
    std::vector<Note> notes;
};
struct Layer {
    const char* name;
    const char* purpose;
    const char* plugin;
    uint32_t colour;
    float volume;
    bool melodic;
};
inline constexpr std::array<Layer, 7> layers{{
    {"Velvet sky", "Slow chords / the atmosphere", "magda_polysynth", 0xffb995fa, 0.30f, true},
    {"Deep current", "Sub bass / the ground below", "magda_polysynth", 0xffe3a779, 0.46f, true},
    {"Light droplets", "Marimba / a small conversation", "magda_marimba", 0xff92d2c7, 0.27f, true},
    {"Earth circles", "Djembe / a gently turning rhythm", "magda_djembe", 0xffec92b4, 0.24f, false},
    {"Soft heartbeat", "Kick / a calm, steady pulse", "magda_kick", 0xffffad66, 0.43f, false},
    {"Dust in sunlight", "Hats / a little forward motion", "magda_hat", 0xffb9b0e8, 0.15f, false},
    {"Distant stars", "Bells / space between the phrases", "magda_bell", 0xffffd584, 0.18f, true},
}};
struct Options {
    int mood = 0;
    int root = 2;
    int bars = 8;  // Short loop first so beginners hear a fuller start quickly.
    double tempo = 84;
    float motion = 0.45f;
    float space = 0.6f;
    float warmth = 0.65f;
    uint32_t seed = 73;
    std::array<bool, 7> enabled{{true, true, true, true, true, true, true}};
    bool operator==(const Options&) const = default;
};
inline Options sanitise(Options o) {
    o.mood = std::clamp(o.mood, 0, 3);
    o.root = pitchClass(o.root);
    o.bars = o.bars <= 8 ? 8 : (o.bars <= 32 ? 32 : 64);
    o.tempo = std::isfinite(o.tempo) ? std::clamp(o.tempo, 40.0, 180.0) : 84;
    auto unit = [](float x) { return std::isfinite(x) ? std::clamp(x, 0.0f, 1.0f) : 0.5f; };
    o.motion = unit(o.motion);
    o.space = unit(o.space);
    o.warmth = unit(o.warmth);
    return o;
}
struct Journey {
    Options options;
    std::array<std::vector<Phrase>, 7> phrases;
};
inline const char* sectionName(int section) {
    constexpr const char* names[]{"Arrive", "Drift", "Bloom", "Dissolve"};
    return names[std::clamp(section, 0, 3)];
}
inline Journey compose(Options raw) {
    Journey song;
    song.options = sanitise(raw);
    const auto& o = song.options;
    std::mt19937 random(o.seed);
    auto chance = [&](float p) { return (random() % 10000) < static_cast<uint32_t>(p * 10000); };
    const int sections = o.bars == 8 ? 1 : 4;
    const int barsPerSection = o.bars / sections;
    for (int section = 0; section < sections; ++section) {
        const bool intro = sections > 1 && section == 0;
        const bool outro = sections > 1 && section == 3;
        for (int li = 0; li < 7; ++li) {
            if (!o.enabled[static_cast<size_t>(li)])
                continue;
            if (intro && (li == 1 || li == 4 || li == 5))
                continue;
            if (outro && (li == 3 || li == 4 || li == 5))
                continue;
            for (int localBar = 0; localBar < barsPerSection; localBar += 4) {
                Phrase phrase{double((section * barsPerSection + localBar) * 4),
                              16,
                              sectionName(section),
                              {}};
                auto add = [&](int pitch, double beat, double len, int vel) {
                    if (beat >= phrase.length)
                        return;
                    phrase.notes.push_back({std::clamp(pitch, 0, 127), beat,
                                            std::min(len, phrase.length - beat),
                                            std::clamp(vel, 1, 110)});
                };
                // Modal movement, with extensions and slow harmonic rhythm.
                const int chordDegree = (section + localBar / 4) % 2 == 0 ? 0 : 3;
                if (li == 0) {
                    for (int d : {chordDegree, chordDegree + 2, chordDegree + 4, chordDegree + 8})
                        add(degreeNote(d, o.root, o.mood, 48), 0, outro ? 12 : 15.8,
                            intro ? 56 : 65);
                } else if (li == 1) {
                    for (int bar = 0; bar < 4; ++bar) {
                        add(degreeNote(chordDegree, o.root, o.mood, 24), bar * 4.0, 2.65, 78);
                        if (chance(o.motion))
                            add(degreeNote(chordDegree + 4, o.root, o.mood, 24), bar * 4.0 + 3,
                                0.75, 60);
                    }
                } else if (li == 2) {
                    constexpr int motif[]{0, 4, 8, 4, 2, 7, 4, 1};
                    for (int step = 0; step < 16; ++step) {
                        if (step % 4 != 0 && !chance(0.2f + o.motion * 0.65f))
                            continue;
                        int degree = chordDegree + motif[(step + o.seed % 8) % 8];
                        double swing = step % 2 ? 0.10 * o.motion : 0;
                        add(degreeNote(degree, o.root, o.mood, 60), step + swing, 0.65,
                            52 + int(random() % 24));
                    }
                } else if (li == 3) {
                    // Euclidean 5-in-16 over a half-bar subdivision. Its rotating
                    // accents interlock with the straight heartbeat underneath.
                    for (int s = 0; s < 32; ++s)
                        if ((s * 5) % 16 < 5 && chance(intro ? 0.5f : 0.85f))
                            add(s % 3 == 0 ? 48 : 55, s * 0.5 + (s % 2 ? 0.04 : 0), 0.22,
                                46 + int(random() % 25));
                } else if (li == 4) {
                    for (int bar = 0; bar < 4; ++bar) {
                        add(36, bar * 4.0, 0.28, 84);
                        add(36, bar * 4.0 + 2.5, 0.25, 64);
                        if (chance(o.motion * 0.35f))
                            add(36, bar * 4.0 + 3.5, 0.2, 46);
                    }
                } else if (li == 5) {
                    for (int s = 1; s < 32; s += 2)
                        if (chance(0.4f + o.motion * 0.5f))
                            add(42, s * 0.5 + 0.04, 0.12, 32 + int(random() % 22));
                } else {
                    add(degreeNote(chordDegree + 4, o.root, o.mood, 72), intro ? 3 : 0, 1.8, 48);
                    if (!outro)
                        add(degreeNote(chordDegree + 8, o.root, o.mood, 72), 10.5, 1.5, 42);
                }
                if (!phrase.notes.empty())
                    song.phrases[static_cast<size_t>(li)].push_back(std::move(phrase));
            }
        }
    }
    return song;
}
}  // namespace magda::sunroom
