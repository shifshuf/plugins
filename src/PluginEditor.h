#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class TaskListComponent final : public juce::Component
{
public:
    explicit TaskListComponent (TodoListNativeAudioProcessor& processorRef);

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& event) override;
    void mouseDrag (const juce::MouseEvent& event) override;
    void mouseUp (const juce::MouseEvent& event) override;

    int getPreferredHeight() const;
    void refreshSize();

private:
    enum class HitZone
    {
        None,
        Checkbox,
        Delete,
        Row
    };

    struct HitInfo
    {
        int index = -1;
        HitZone zone = HitZone::None;
    };

    TodoListNativeAudioProcessor& processor;
    int rowHeight = 32;
    int dragFrom = -1;
    int dragOver = -1;
    bool dragging = false;
    HitInfo pressed;

    HitInfo hitAt (juce::Point<float> p) const;
    juce::Rectangle<float> getCheckboxBounds (int row) const;
    juce::Rectangle<float> getDeleteBounds (int row) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TaskListComponent)
};

class TodoListNativeAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                                 private juce::Button::Listener
{
public:
    explicit TodoListNativeAudioProcessorEditor (TodoListNativeAudioProcessor&);
    ~TodoListNativeAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    TodoListNativeAudioProcessor& audioProcessor;
    TaskListComponent taskList;
    juce::Viewport viewport;
    juce::TextEditor input;
    juce::TextButton addButton { "add" };
    juce::TextButton collapseButton { "collapse" };
    juce::TextButton popoutButton { "pop out" };
    juce::Label stats;
    std::unique_ptr<juce::DocumentWindow> detachedWindow;
    bool mainPopOnlyMode = false;
    bool collapsedBeforePopout = false;

    void buttonClicked (juce::Button* button) override;
    void refreshFromState();
    void addFromInput();
    void updateCollapsedLayout();
    void updateStats();
    void updateMainWindowMode();
    void toggleDetachedWindow();
    void closeDetachedWindow();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TodoListNativeAudioProcessorEditor)
};
