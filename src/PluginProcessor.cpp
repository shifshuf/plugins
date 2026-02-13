#include "PluginProcessor.h"
#include "PluginEditor.h"

TodoListNativeAudioProcessor::TodoListNativeAudioProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

TodoListNativeAudioProcessor::~TodoListNativeAudioProcessor() = default;

const juce::String TodoListNativeAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool TodoListNativeAudioProcessor::acceptsMidi() const { return false; }
bool TodoListNativeAudioProcessor::producesMidi() const { return false; }
bool TodoListNativeAudioProcessor::isMidiEffect() const { return false; }
double TodoListNativeAudioProcessor::getTailLengthSeconds() const { return 0.0; }

int TodoListNativeAudioProcessor::getNumPrograms() { return 1; }
int TodoListNativeAudioProcessor::getCurrentProgram() { return 0; }
void TodoListNativeAudioProcessor::setCurrentProgram (int) {}
const juce::String TodoListNativeAudioProcessor::getProgramName (int) { return {}; }
void TodoListNativeAudioProcessor::changeProgramName (int, const juce::String&) {}

void TodoListNativeAudioProcessor::prepareToPlay (double, int) {}
void TodoListNativeAudioProcessor::releaseResources() {}

bool TodoListNativeAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == layouts.getMainOutputChannelSet()
           && (layouts.getMainOutputChannelSet() == juce::AudioChannelSet::mono()
               || layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo());
}

void TodoListNativeAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    for (int channel = 0; channel < getTotalNumOutputChannels(); ++channel)
    {
        auto* in = buffer.getReadPointer (channel);
        auto* out = buffer.getWritePointer (channel);
        juce::FloatVectorOperations::copy (out, in, buffer.getNumSamples());
    }
}

bool TodoListNativeAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* TodoListNativeAudioProcessor::createEditor()
{
    return new TodoListNativeAudioProcessorEditor (*this);
}

int TodoListNativeAudioProcessor::getNumTasks() const
{
    const juce::ScopedLock sl (taskLock);
    return tasks.size();
}

TodoListNativeAudioProcessor::Task TodoListNativeAudioProcessor::getTask (int index) const
{
    const juce::ScopedLock sl (taskLock);
    if (! juce::isPositiveAndBelow (index, tasks.size()))
        return {};
    return tasks.getReference (index);
}

void TodoListNativeAudioProcessor::addTask (juce::String text)
{
    text = text.trim();
    if (text.isEmpty())
        return;

    {
        const juce::ScopedLock sl (taskLock);
        tasks.add ({ text, false });
    }
    if (onTasksChanged)
        onTasksChanged();
}

void TodoListNativeAudioProcessor::setTaskDone (int index, bool done)
{
    {
        const juce::ScopedLock sl (taskLock);
        if (! juce::isPositiveAndBelow (index, tasks.size()))
            return;
        tasks.getReference (index).done = done;
    }
    if (onTasksChanged)
        onTasksChanged();
}

void TodoListNativeAudioProcessor::removeTask (int index)
{
    {
        const juce::ScopedLock sl (taskLock);
        if (! juce::isPositiveAndBelow (index, tasks.size()))
            return;
        tasks.remove (index);
    }
    if (onTasksChanged)
        onTasksChanged();
}

void TodoListNativeAudioProcessor::moveTask (int from, int to)
{
    {
        const juce::ScopedLock sl (taskLock);
        if (! juce::isPositiveAndBelow (from, tasks.size()) || ! juce::isPositiveAndBelow (to, tasks.size()))
            return;
        if (from == to)
            return;
        tasks.move (from, to);
    }
    if (onTasksChanged)
        onTasksChanged();
}

bool TodoListNativeAudioProcessor::getCollapsed() const noexcept
{
    return collapsed;
}

void TodoListNativeAudioProcessor::setCollapsed (bool shouldCollapse)
{
    collapsed = shouldCollapse;
    if (onTasksChanged)
        onTasksChanged();
}

void TodoListNativeAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::Array<Task> copy;
    bool collapsedCopy = false;

    {
        const juce::ScopedLock sl (taskLock);
        copy = tasks;
        collapsedCopy = collapsed;
    }

    const auto json = tasksToJson (copy, collapsedCopy);
    juce::MemoryOutputStream stream (destData, false);
    stream.writeString (json);
}

void TodoListNativeAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto json = juce::String::fromUTF8 (static_cast<const char*> (data), sizeInBytes);

    juce::Array<Task> loadedTasks;
    bool loadedCollapsed = false;
    jsonToTasks (json, loadedTasks, loadedCollapsed);

    {
        const juce::ScopedLock sl (taskLock);
        tasks = loadedTasks;
        collapsed = loadedCollapsed;
    }

    if (onTasksChanged)
        onTasksChanged();
}

juce::String TodoListNativeAudioProcessor::tasksToJson (const juce::Array<Task>& source, bool isCollapsed)
{
    juce::DynamicObject::Ptr root (new juce::DynamicObject());
    root->setProperty ("collapsed", isCollapsed);

    juce::Array<juce::var> taskVars;
    for (const auto& task : source)
    {
        juce::DynamicObject::Ptr item (new juce::DynamicObject());
        item->setProperty ("text", task.text);
        item->setProperty ("done", task.done);
        taskVars.add (juce::var (item.get()));
    }

    root->setProperty ("tasks", juce::var (taskVars));
    return juce::JSON::toString (juce::var (root.get()));
}

void TodoListNativeAudioProcessor::jsonToTasks (const juce::String& jsonText, juce::Array<Task>& destTasks, bool& isCollapsed)
{
    destTasks.clear();
    isCollapsed = false;

    const auto parsed = juce::JSON::parse (jsonText);
    if (! parsed.isObject())
        return;

    if (auto* obj = parsed.getDynamicObject())
    {
        isCollapsed = static_cast<bool> (obj->getProperty ("collapsed"));
        const auto tasksVar = obj->getProperty ("tasks");
        if (tasksVar.isArray())
        {
            const auto* arr = tasksVar.getArray();
            for (const auto& item : *arr)
            {
                if (auto* taskObj = item.getDynamicObject())
                {
                    Task t;
                    t.text = taskObj->getProperty ("text").toString();
                    t.done = static_cast<bool> (taskObj->getProperty ("done"));
                    if (! t.text.isEmpty())
                        destTasks.add (t);
                }
            }
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TodoListNativeAudioProcessor();
}
