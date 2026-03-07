#include "PresetManager.h"

//==============================================================================
PresetManager::PresetManager(juce::AudioProcessorValueTreeState& apvtsRef)
    : apvts(apvtsRef)
{
    // Create user preset folder on first launch if it doesn't exist
    getUserPresetFolder().createDirectory();
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