#pragma once
#include <JuceHeader.h>
#include <random>

/*  PresetManager — handles all save/load/browse operations for Artifact.

    Preset files use the .artifact extension and contain a full APVTS
    XML snapshot. Factory presets are read-only and bundled with the plugin.
    User presets are stored in the platform-appropriate AppData folder.

    Cross-platform preset folder locations:
      Windows : C:\Users\<user>\AppData\Roaming\Artifact\Presets\
      macOS   : ~/Library/Application Support/Artifact/Presets/
      Linux   : ~/.config/Artifact/Presets/
*/
class PresetManager
{
public:
    // File extension for preset files
    static constexpr const char* presetExtension = ".artifact";
    static constexpr const char* presetWildcard = "*.artifact"; 
    static constexpr const char* manufacturerName = "lickm";   // ← change to yours
    static constexpr const char* pluginName = "Artifact";

    //==========================================================================
    explicit PresetManager(juce::AudioProcessorValueTreeState& apvtsRef);

    //==========================================================================
    // Returns the user preset folder, creating it if it doesn't exist
    juce::File getUserPresetFolder() const;

    // Save current APVTS state to a named preset in the user folder
    // Returns true on success
    bool savePreset(const juce::String& presetName);

    // Load a preset from a file — restores full APVTS state
    // Returns true on success
    bool loadPreset(const juce::File& presetFile);

    // Load a preset by name from the user folder
    bool loadPresetByName(const juce::String& presetName);

    // Delete a preset file from the user folder
    bool deletePreset(const juce::File& presetFile);

    // Rename a preset file in the user folder
    bool renamePreset(const juce::File& presetFile, const juce::String& newName);

    //==========================================================================
    // Returns a list of all preset files in the user folder
    juce::Array<juce::File> getUserPresets() const;

    // Returns a list of all preset names (without extension) in the user folder
    juce::StringArray getUserPresetNames() const;

    //==========================================================================
    // Current preset tracking
    void setCurrentPresetName(const juce::String& name) { currentPresetName = name; }
    juce::String getCurrentPresetName() const { return currentPresetName; }

    // Navigate presets — loads prev/next preset in the user folder
    bool loadNextPreset();
    bool loadPreviousPreset();

    //==========================================================================
    // Serialisation helpers — used by PluginProcessor::getStateInformation
    // These are wrappers around APVTS XML read/write
    void saveStateToMemoryBlock(juce::MemoryBlock& destData) const;
    void loadStateFromMemoryBlock(const void* data, int sizeInBytes);

    // Installs factory presets to user folder if not already present.
    // Called once on first launch — never overwrites existing user presets.
    void installFactoryPresets();

    // ── Favourites ────────────────────────────────────────────────────────────
    void toggleFavourite(const juce::String& presetName);
    bool isFavourite(const juce::String& presetName) const;
    juce::StringArray getFavouriteNames() const;

    // ── Random preset ─────────────────────────────────────────────────────────
    // Loads a random preset from the user folder (excluding current)
    bool loadRandomPreset();

private:
    // Write a preset from a raw parameter map
    bool writeFactoryPreset(const juce::String& name,
        const std::map<juce::String, float>& params);

    void loadFavourites();
    void saveFavourites() const;
    juce::File getFavouritesFile() const;

    juce::StringArray favourites;
    std::mt19937 rng{ std::random_device{}() };



private:
    juce::AudioProcessorValueTreeState& apvts;
    juce::String currentPresetName{ "Default" };

    // Write APVTS state to a file as XML
    bool writePresetFile(const juce::File& file) const;

    // Read XML from a file and restore APVTS state
    bool readPresetFile(const juce::File& file);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetManager)
};