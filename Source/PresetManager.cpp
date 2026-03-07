#include "PresetManager.h"

//==============================================================================
PresetManager::PresetManager(juce::AudioProcessorValueTreeState& apvtsRef)
    : apvts(apvtsRef)
{
    getUserPresetFolder().createDirectory();
    installFactoryPresets();  // ← add this line
}

//==============================================================================
juce::File PresetManager::getUserPresetFolder() const
{
    // juce::File::getSpecialLocation handles cross-platform AppData resolution:
    //   Windows → AppData/Roaming
    //   macOS   → ~/Library/Application Support
    //   Linux   → ~/.config
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile(manufacturerName)
        .getChildFile(pluginName)
        .getChildFile("Presets");
}

//==============================================================================
bool PresetManager::savePreset(const juce::String& presetName)
{
    if (presetName.isEmpty())
        return false;

    // Sanitise filename — replace spaces with underscores, strip illegal chars
    const juce::String safeFileName = presetName.replaceCharacter(' ', '_')
        + presetExtension;

    const juce::File presetFile = getUserPresetFolder().getChildFile(safeFileName);

    if (writePresetFile(presetFile))
    {
        currentPresetName = presetName;
        return true;
    }

    return false;
}

//==============================================================================
bool PresetManager::loadPreset(const juce::File& presetFile)
{
    if (!presetFile.existsAsFile())
        return false;

    if (readPresetFile(presetFile))
    {
        // Display name: strip underscores and extension
        currentPresetName = presetFile.getFileNameWithoutExtension()
            .replaceCharacter('_', ' ');
        return true;
    }

    return false;
}

//==============================================================================
bool PresetManager::loadPresetByName(const juce::String& presetName)
{
    const juce::String safeFileName = presetName.replaceCharacter(' ', '_')
        + presetExtension;
    return loadPreset(getUserPresetFolder().getChildFile(safeFileName));
}

//==============================================================================
bool PresetManager::deletePreset(const juce::File& presetFile)
{
    if (!presetFile.existsAsFile())
        return false;

    return presetFile.moveToTrash();
}

//==============================================================================
bool PresetManager::renamePreset(const juce::File& presetFile,
    const juce::String& newName)
{
    if (!presetFile.existsAsFile() || newName.isEmpty())
        return false;

    const juce::String safeNewName = newName.replaceCharacter(' ', '_')
        + presetExtension;
    const juce::File newFile = presetFile.getParentDirectory()
        .getChildFile(safeNewName);

    return presetFile.moveFileTo(newFile);
}

//==============================================================================
juce::Array<juce::File> PresetManager::getUserPresets() const
{
    juce::Array<juce::File> presets;
    getUserPresetFolder().findChildFiles(presets,
        juce::File::findFiles,
        false,          // non-recursive
        presetWildcard);
    presets.sort();
    return presets;
}

//==============================================================================
juce::StringArray PresetManager::getUserPresetNames() const
{
    juce::StringArray names;
    for (const auto& file : getUserPresets())
        names.add(file.getFileNameWithoutExtension().replaceCharacter('_', ' '));
    return names;
}

//==============================================================================
bool PresetManager::loadNextPreset()
{
    const auto presets = getUserPresets();
    if (presets.isEmpty()) return false;

    // Find current preset index
    int currentIdx = -1;
    for (int i = 0; i < presets.size(); ++i)
    {
        if (presets[i].getFileNameWithoutExtension()
            .replaceCharacter('_', ' ') == currentPresetName)
        {
            currentIdx = i;
            break;
        }
    }

    // Wrap around to first preset if at end
    const int nextIdx = (currentIdx + 1) % presets.size();
    return loadPreset(presets[nextIdx]);
}

//==============================================================================
bool PresetManager::loadPreviousPreset()
{
    const auto presets = getUserPresets();
    if (presets.isEmpty()) return false;

    int currentIdx = -1;
    for (int i = 0; i < presets.size(); ++i)
    {
        if (presets[i].getFileNameWithoutExtension()
            .replaceCharacter('_', ' ') == currentPresetName)
        {
            currentIdx = i;
            break;
        }
    }

    // Wrap around to last preset if at beginning
    const int prevIdx = (currentIdx <= 0) ? presets.size() - 1
        : currentIdx - 1;
    return loadPreset(presets[prevIdx]);
}

//==============================================================================
bool PresetManager::writePresetFile(const juce::File& file) const
{
    // Copy current APVTS state to an XML element and write to disk
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());

    if (xml == nullptr)
        return false;

    // Add metadata to the XML so we can identify the file format later
    xml->setAttribute("pluginName", pluginName);
    xml->setAttribute("version", "1.0.0");
    xml->setAttribute("presetName", currentPresetName);

    return xml->writeTo(file);
}

//==============================================================================
bool PresetManager::readPresetFile(const juce::File& file)
{
    // Parse XML from disk
    auto xml = juce::XmlDocument::parse(file);

    if (xml == nullptr)
        return false;

    // Validate it's an Artifact preset
    if (!xml->hasAttribute("pluginName"))
        return false;

    if (xml->getStringAttribute("pluginName") != pluginName)
        return false;

    // Restore APVTS state
    auto newState = juce::ValueTree::fromXml(*xml);

    if (!newState.isValid())
        return false;

    apvts.replaceState(newState);
    return true;
}

//==============================================================================
void PresetManager::saveStateToMemoryBlock(juce::MemoryBlock& destData) const
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    if (xml != nullptr)
        juce::AudioProcessor::copyXmlToBinary(*xml, destData);
}

void PresetManager::loadStateFromMemoryBlock(const void* data, int sizeInBytes)
{
    auto xml = juce::AudioProcessor::getXmlFromBinary(data, sizeInBytes);
    if (xml != nullptr && xml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

//==============================================================================
bool PresetManager::writeFactoryPreset(const juce::String& name,
    const std::map<juce::String, float>& params)
{
    const juce::String safeFileName = name.replaceCharacter(' ', '_') + presetExtension;
    const juce::File   presetFile = getUserPresetFolder().getChildFile(safeFileName);
    if (presetFile.existsAsFile()) return true;

    // Save the current live state so we can restore it after writing
    const auto savedState = apvts.copyState().createCopy();

    // Temporarily apply each factory parameter value to the live APVTS
    for (const auto& kv : params)
    {
        auto* param = apvts.getParameter(kv.first);
        if (param != nullptr)
            param->setValueNotifyingHost(param->convertTo0to1(kv.second));
    }

    // Write the live state to disk — guaranteed correct APVTS XML structure
    const bool success = writePresetFile(presetFile);

    // Restore the original state exactly as it was
    apvts.replaceState(savedState);

    return success;
}

//==============================================================================
void PresetManager::installFactoryPresets()
{
    getUserPresetFolder().createDirectory();

    // ── Tier 1: Subtle ────────────────────────────────────────────────────────
    // These sound like a slightly degraded codec — usable on nearly anything

    writeFactoryPreset("01 Warm Codec", {
        { "lossMode",        0 },   // Standard
        { "lossAmount",     18 },
        { "lossSpeed",      60 },
        { "lossGain",        0 },
        { "codecMode",       0 },   // Music
        { "magnitude",     100 },
        { "masterMix",      75 },
        { "noiseEnabled",    0 },
        { "verbAmount",      0 },
        { "filterMode",      0 },
        { "lowCutFreq",     20 },
        { "highCutFreq", 20000 },
        { "weighting",       0 },   // Perceptual
        { "stereoMode",      0 },   // Stereo
        { "preprocessGain",  0 },
        { "postprocessGain", 0 },
        { "autoGain",       40 },
        { "gateThreshold", -96 },
        { "masterBypass",    0 },
        });

    writeFactoryPreset("02 Phone Call", {
        { "lossMode",        0 },   // Standard
        { "lossAmount",     35 },
        { "lossSpeed",      55 },
        { "lossGain",        1 },
        { "codecMode",       1 },   // Voice
        { "magnitude",     100 },
        { "masterMix",     100 },
        { "noiseEnabled",    1 },
        { "noiseAmount",     8 },
        { "noiseColor",      0 },   // White
        { "noiseBias",      20 },
        { "verbAmount",      0 },
        { "filterMode",      0 },
        { "lowCutFreq",    300 },
        { "highCutFreq",  3400 },
        { "filterSlope",     1 },   // 24dB/oct
        { "weighting",       0 },
        { "stereoMode",      2 },   // Mono
        { "preprocessGain",  0 },
        { "postprocessGain", 0 },
        { "autoGain",       80 },
        { "gateThreshold", -96 },
        { "masterBypass",    0 },
        });

    writeFactoryPreset("03 AM Radio", {
        { "lossMode",        0 },   // Standard
        { "lossAmount",     28 },
        { "lossSpeed",      50 },
        { "lossGain",        0 },
        { "codecMode",       2 },   // Broadcast
        { "magnitude",     100 },
        { "masterMix",     100 },
        { "noiseEnabled",    1 },
        { "noiseAmount",    12 },
        { "noiseColor",      1 },   // Pink
        { "noiseBias",     -30 },
        { "verbAmount",      8 },
        { "verbDecay",      40 },
        { "verbPosition",    1 },   // Post
        { "filterMode",      0 },
        { "lowCutFreq",    150 },
        { "highCutFreq",  7500 },
        { "filterSlope",     1 },
        { "weighting",       0 },
        { "stereoMode",      2 },   // Mono
        { "preprocessGain",  0 },
        { "postprocessGain", 0 },
        { "autoGain",       60 },
        { "gateThreshold", -96 },
        { "masterBypass",    0 },
        });

    // ── Tier 2: Character ─────────────────────────────────────────────────────
    // These have a distinct sound — noticeably processed but musical

    writeFactoryPreset("04 Spectral Smear", {
        { "lossMode",        0 },   // Standard
        { "lossAmount",     52 },
        { "lossSpeed",      25 },
        { "lossGain",        0 },
        { "codecMode",       0 },   // Music
        { "magnitude",     100 },
        { "masterMix",      85 },
        { "noiseEnabled",    0 },
        { "verbAmount",      0 },
        { "filterMode",      0 },
        { "lowCutFreq",     20 },
        { "highCutFreq", 20000 },
        { "weighting",       0 },
        { "stereoMode",      0 },
        { "preprocessGain",  0 },
        { "postprocessGain", 0 },
        { "autoGain",       50 },
        { "gateThreshold", -96 },
        { "masterBypass",    0 },
        });

    writeFactoryPreset("05 Phase Ghost", {
        { "lossMode",        2 },   // Phase Jitter
        { "lossAmount",     45 },
        { "lossSpeed",      40 },
        { "lossGain",        0 },
        { "codecMode",       0 },
        { "magnitude",     100 },
        { "masterMix",      80 },
        { "noiseEnabled",    0 },
        { "verbAmount",     20 },
        { "verbDecay",      80 },
        { "verbPosition",    0 },   // Pre
        { "filterMode",      0 },
        { "lowCutFreq",     20 },
        { "highCutFreq", 20000 },
        { "weighting",       1 },   // Flat
        { "stereoMode",      0 },
        { "preprocessGain",  0 },
        { "postprocessGain", 0 },
        { "autoGain",       30 },
        { "gateThreshold", -96 },
        { "masterBypass",    0 },
        });

    writeFactoryPreset("06 Stutter Engine", {
        { "lossMode",        3 },   // Packet Repeat
        { "lossAmount",     48 },
        { "lossSpeed",      70 },
        { "lossGain",        0 },
        { "codecMode",       0 },
        { "magnitude",     100 },
        { "masterMix",      90 },
        { "noiseEnabled",    0 },
        { "verbAmount",      0 },
        { "filterMode",      0 },
        { "lowCutFreq",     20 },
        { "highCutFreq", 20000 },
        { "weighting",       0 },
        { "stereoMode",      1 },   // Joint Stereo
        { "preprocessGain",  0 },
        { "postprocessGain", 0 },
        { "autoGain",       60 },
        { "gateThreshold", -96 },
        { "masterBypass",    0 },
        });

    writeFactoryPreset("07 Hollow Inverse", {
        { "lossMode",        1 },   // Inverse
        { "lossAmount",     40 },
        { "lossSpeed",      35 },
        { "lossGain",        2 },
        { "codecMode",       0 },
        { "magnitude",     100 },
        { "masterMix",      70 },
        { "noiseEnabled",    1 },
        { "noiseAmount",    15 },
        { "noiseColor",      1 },   // Pink
        { "noiseBias",       0 },
        { "verbAmount",     25 },
        { "verbDecay",      90 },
        { "verbPosition",    1 },   // Post
        { "filterMode",      0 },
        { "lowCutFreq",     80 },
        { "highCutFreq", 12000 },
        { "filterSlope",     0 },
        { "weighting",       0 },
        { "stereoMode",      0 },
        { "preprocessGain",  0 },
        { "postprocessGain", 0 },
        { "autoGain",       40 },
        { "gateThreshold", -96 },
        { "masterBypass",    0 },
        });

    // ── Tier 3: Damaged ───────────────────────────────────────────────────────
    // Heavily processed — useful as an effect or send

    writeFactoryPreset("08 Deep Fry", {
        { "lossMode",        5 },   // Std + Packet Loss
        { "lossAmount",     68 },
        { "lossSpeed",      80 },
        { "lossGain",        0 },
        { "codecMode",       0 },
        { "magnitude",     100 },
        { "masterMix",     100 },
        { "noiseEnabled",    1 },
        { "noiseAmount",    25 },
        { "noiseColor",      0 },   // White
        { "noiseBias",      40 },
        { "verbAmount",      0 },
        { "filterMode",      0 },
        { "lowCutFreq",     20 },
        { "highCutFreq", 20000 },
        { "weighting",       1 },   // Flat
        { "stereoMode",      0 },
        { "preprocessGain",  0 },
        { "postprocessGain", 0 },
        { "autoGain",       70 },
        { "gateThreshold", -60 },
        { "masterBypass",    0 },
        });

    writeFactoryPreset("09 Disorder Field", {
        { "lossMode",        7 },   // Packet Disorder
        { "lossAmount",     62 },
        { "lossSpeed",      55 },
        { "lossGain",        0 },
        { "codecMode",       0 },
        { "magnitude",     100 },
        { "masterMix",     100 },
        { "noiseEnabled",    0 },
        { "verbAmount",     35 },
        { "verbDecay",     120 },
        { "verbPosition",    0 },   // Pre
        { "filterMode",      0 },
        { "lowCutFreq",     20 },
        { "highCutFreq", 20000 },
        { "weighting",       0 },
        { "stereoMode",      0 },
        { "preprocessGain",  0 },
        { "postprocessGain", 0 },
        { "autoGain",       50 },
        { "gateThreshold", -96 },
        { "masterBypass",    0 },
        });

    writeFactoryPreset("10 Brown Rumble", {
        { "lossMode",        0 },   // Standard
        { "lossAmount",     55 },
        { "lossSpeed",      15 },
        { "lossGain",        0 },
        { "codecMode",       0 },
        { "magnitude",     100 },
        { "masterMix",      85 },
        { "noiseEnabled",    1 },
        { "noiseAmount",    35 },
        { "noiseColor",      2 },   // Brown
        { "noiseBias",     -60 },
        { "verbAmount",     40 },
        { "verbDecay",     150 },
        { "verbPosition",    0 },   // Pre
        { "filterMode",      0 },
        { "lowCutFreq",     20 },
        { "highCutFreq",  6000 },
        { "filterSlope",     1 },
        { "weighting",       0 },
        { "stereoMode",      0 },
        { "preprocessGain",  0 },
        { "postprocessGain", 0 },
        { "autoGain",       40 },
        { "gateThreshold", -96 },
        { "masterBypass",    0 },
        });

    // ── Tier 4: Destroyed ─────────────────────────────────────────────────────
    // Maximum degradation — sound design territory

    writeFactoryPreset("11 Full Dissolution", {
        { "lossMode",        8 },   // Disorder + Standard
        { "lossAmount",     85 },
        { "lossSpeed",      90 },
        { "lossGain",        0 },
        { "codecMode",       0 },
        { "magnitude",     100 },
        { "masterMix",     100 },
        { "noiseEnabled",    1 },
        { "noiseAmount",    40 },
        { "noiseColor",      1 },   // Pink
        { "noiseBias",       0 },
        { "verbAmount",     50 },
        { "verbDecay",     180 },
        { "verbPosition",    0 },   // Pre
        { "filterMode",      0 },
        { "lowCutFreq",     20 },
        { "highCutFreq", 20000 },
        { "weighting",       1 },
        { "stereoMode",      0 },
        { "preprocessGain",  0 },
        { "postprocessGain", 0 },
        { "autoGain",       60 },
        { "gateThreshold", -96 },
        { "masterBypass",    0 },
        });

    writeFactoryPreset("12 Signal Death", {
        { "lossMode",        4 },   // Packet Loss
        { "lossAmount",     78 },
        { "lossSpeed",      85 },
        { "lossGain",        0 },
        { "codecMode",       2 },   // Broadcast
        { "magnitude",     100 },
        { "masterMix",     100 },
        { "noiseEnabled",    1 },
        { "noiseAmount",    50 },
        { "noiseColor",      2 },   // Brown
        { "noiseBias",     -40 },
        { "verbAmount",     60 },
        { "verbDecay",     200 },
        { "verbPosition",    0 },   // Pre
        { "filterMode",      1 },   // Inverted
        { "lowCutFreq",    500 },
        { "highCutFreq",  4000 },
        { "filterSlope",     2 },   // 36dB/oct
        { "weighting",       1 },
        { "stereoMode",      1 },   // Joint Stereo
        { "preprocessGain",  0 },
        { "postprocessGain", 0 },
        { "autoGain",       80 },
        { "gateThreshold", -48 },
        { "masterBypass",    0 },
        });
}

//==============================================================================
juce::File PresetManager::getFavouritesFile() const
{
    return getUserPresetFolder().getChildFile("favourites.txt");
}

void PresetManager::loadFavourites()
{
    favourites.clear();
    const auto file = getFavouritesFile();
    if (file.existsAsFile())
        favourites.addTokens(file.loadFileAsString(), "\n", "");
    favourites.removeEmptyStrings();
}

void PresetManager::saveFavourites() const
{
    getFavouritesFile().replaceWithText(favourites.joinIntoString("\n"));
}

void PresetManager::toggleFavourite(const juce::String& presetName)
{
    loadFavourites();
    if (favourites.contains(presetName))
        favourites.removeString(presetName);
    else
        favourites.add(presetName);
    saveFavourites();
}

bool PresetManager::isFavourite(const juce::String& presetName) const
{
    return favourites.contains(presetName);
}

juce::StringArray PresetManager::getFavouriteNames() const
{
    return favourites;
}

//==============================================================================
bool PresetManager::loadRandomPreset()
{
    const auto presets = getUserPresets();
    if (presets.size() <= 1) return false;

    std::uniform_int_distribution<int> dist(0, presets.size() - 1);
    int idx = dist(const_cast<std::mt19937&> (rng));

    // Avoid reloading the current preset if possible
    const auto currentFile = getUserPresetFolder()
        .getChildFile(currentPresetName.replaceCharacter(' ', '_') + presetExtension);
    if (presets[idx] == currentFile && presets.size() > 1)
        idx = (idx + 1) % presets.size();

    return loadPreset(presets[idx]);
}