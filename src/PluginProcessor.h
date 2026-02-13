#pragma once

#include <JuceHeader.h>

class TodoListNativeAudioProcessor final : public juce::AudioProcessor
{
public:
    struct Task
    {
        juce::String text;
        bool done = false;
    };

    TodoListNativeAudioProcessor();
    ~TodoListNativeAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    int getNumTasks() const;
    Task getTask (int index) const;
    void addTask (juce::String text);
    void setTaskDone (int index, bool done);
    void removeTask (int index);
    void moveTask (int from, int to);
    bool getCollapsed() const noexcept;
    void setCollapsed (bool shouldCollapse);

    std::function<void()> onTasksChanged;

private:
    juce::Array<Task> tasks;
    bool collapsed = false;
    mutable juce::CriticalSection taskLock;

    static juce::String tasksToJson (const juce::Array<Task>& source, bool isCollapsed);
    static void jsonToTasks (const juce::String& jsonText, juce::Array<Task>& destTasks, bool& isCollapsed);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TodoListNativeAudioProcessor)
};
