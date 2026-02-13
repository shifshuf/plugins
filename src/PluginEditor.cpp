#include "PluginEditor.h"

namespace
{
const auto kBg = juce::Colour::fromRGB (28, 30, 34);
const auto kPanel = juce::Colour::fromRGB (38, 41, 47);
const auto kBorder = juce::Colour::fromRGB (74, 78, 87);
const auto kText = juce::Colour::fromRGB (230, 233, 238);
const auto kMuted = juce::Colour::fromRGB (145, 151, 162);
const auto kAccent = juce::Colour::fromRGB (87, 176, 235);
const auto kDanger = juce::Colour::fromRGB (179, 84, 98);
} // namespace

class DetachedTodoComponent final : public juce::Component,
                                    private juce::Button::Listener,
                                    private juce::Timer
{
public:
    explicit DetachedTodoComponent (TodoListNativeAudioProcessor& p)
        : processor (p), taskList (p)
    {
        addAndMakeVisible (viewport);
        viewport.setViewedComponent (&taskList, false);
        viewport.setScrollBarsShown (true, false);

        addAndMakeVisible (input);
        input.setTextToShowWhenEmpty ("type task and press enter", kMuted);
        input.onReturnKey = [this] { addFromInput(); };
        input.setColour (juce::TextEditor::backgroundColourId, kPanel.darker (0.2f));
        input.setColour (juce::TextEditor::textColourId, kText);
        input.setColour (juce::TextEditor::outlineColourId, kBorder);

        addAndMakeVisible (addButton);
        addButton.setColour (juce::TextButton::buttonColourId, kAccent.darker (0.25f));
        addButton.addListener (this);

        addAndMakeVisible (stats);
        stats.setColour (juce::Label::textColourId, kMuted);
        stats.setJustificationType (juce::Justification::centredRight);

        addAndMakeVisible (collapseButton);
        collapseButton.addListener (this);

        setSize (430, 330);
        updateCollapsedUi();
        startTimerHz (12);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (kBg);
        g.setColour (kBorder);
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 8.0f, 1.0f);
        g.setColour (kText);
        g.setFont (juce::Font (juce::FontOptions (14.0f, juce::Font::bold)));
        g.drawText ("todo list", 12, 8, 180, 24, juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (10);
        auto header = area.removeFromTop (28);
        collapseButton.setBounds (header.removeFromRight (100));
        stats.setBounds (header.removeFromRight (170));

        if (isCollapsed)
            return;

        auto inputRow = area.removeFromBottom (34);
        addButton.setBounds (inputRow.removeFromRight (68));
        input.setBounds (inputRow.reduced (0, 2));
        viewport.setBounds (area.reduced (0, 4));
        taskList.setSize (viewport.getWidth() - 8, taskList.getPreferredHeight());
    }

private:
    TodoListNativeAudioProcessor& processor;
    TaskListComponent taskList;
    juce::Viewport viewport;
    juce::TextEditor input;
    juce::TextButton addButton { "add" };
    juce::TextButton collapseButton { "Collapse" };
    juce::Label stats;

    int lastTaskCount = -1;
    int lastDoneCount = -1;
    bool isCollapsed = false;

    void buttonClicked (juce::Button* button) override
    {
        if (button == &addButton)
        {
            addFromInput();
            return;
        }

        if (button == &collapseButton)
        {
            isCollapsed = ! isCollapsed;
            updateCollapsedUi();
        }
    }

    void addFromInput()
    {
        processor.addTask (input.getText());
        input.clear();
    }

    void timerCallback() override
    {
        const auto total = processor.getNumTasks();
        int done = 0;
        for (int i = 0; i < total; ++i)
            if (processor.getTask (i).done)
                ++done;

        if (total != lastTaskCount || done != lastDoneCount)
        {
            lastTaskCount = total;
            lastDoneCount = done;
            stats.setText (juce::String (total - done) + " active | " + juce::String (done) + " done",
                           juce::dontSendNotification);
            taskList.refreshSize();
            resized();
            repaint();
        }

    }

    void updateCollapsedUi()
    {
        collapseButton.setButtonText (isCollapsed ? "expand" : "collapse");
        viewport.setVisible (! isCollapsed);
        input.setVisible (! isCollapsed);
        addButton.setVisible (! isCollapsed);
        const int targetHeight = isCollapsed ? 56 : 330;
        if (auto* w = findParentComponentOfClass<juce::DocumentWindow>())
        {
            w->setResizeLimits (360, 56, 860, 900);
            const int frameExtra = w->getHeight() - w->getContentComponent()->getHeight();
            auto bounds = w->getBounds();
            bounds.setHeight (targetHeight + juce::jmax (0, frameExtra));
            w->setBounds (bounds);
            w->setContentComponentSize (430, targetHeight);
        }
        else
        {
            setSize (430, targetHeight);
        }
        resized();
    }
};

TaskListComponent::TaskListComponent (TodoListNativeAudioProcessor& processorRef)
    : processor (processorRef)
{
    refreshSize();
}

void TaskListComponent::paint (juce::Graphics& g)
{
    g.fillAll (kBg.darker (0.08f));
    const auto num = processor.getNumTasks();

    for (int i = 0; i < num; ++i)
    {
        const auto row = juce::Rectangle<float> (0.0f, (float) (i * rowHeight), (float) getWidth(), (float) rowHeight);
        const bool isDragRow = (dragging && i == dragFrom);

        g.setColour (isDragRow ? kPanel.brighter (0.1f) : kPanel);
        g.fillRoundedRectangle (row.reduced (4.0f, 2.0f), 6.0f);
        g.setColour (kBorder);
        g.drawRoundedRectangle (row.reduced (4.0f, 2.0f), 6.0f, 1.0f);

        if (dragging && dragOver == i && dragOver != dragFrom)
        {
            g.setColour (kAccent);
            g.fillRect (juce::Rectangle<float> (8.0f, row.getY() + 1.0f, (float) getWidth() - 16.0f, 2.0f));
        }

        const auto task = processor.getTask (i);
        auto cb = getCheckboxBounds (i);
        auto del = getDeleteBounds (i);

        g.setColour (kBorder);
        g.drawRoundedRectangle (cb, 3.0f, 1.0f);
        if (task.done)
        {
            g.setColour (kAccent);
            juce::Path check;
            check.startNewSubPath (cb.getX() + 3.0f, cb.getCentreY());
            check.lineTo (cb.getX() + 7.0f, cb.getBottom() - 4.0f);
            check.lineTo (cb.getRight() - 3.0f, cb.getY() + 3.0f);
            g.strokePath (check, juce::PathStrokeType (2.0f));
        }

        auto textArea = row.reduced (36.0f, 0.0f).withTrimmedRight (34.0f);
        g.setFont (14.0f);
        g.setColour (task.done ? kMuted : kText);
        g.drawFittedText (task.text, textArea.toNearestInt(), juce::Justification::centredLeft, 1);
        if (task.done)
        {
            const float renderedTextWidth = juce::jmin ((float) textArea.getWidth(),
                                                        (float) g.getCurrentFont().getStringWidth (task.text));
            g.setColour (kMuted);
            g.drawLine ((float) textArea.getX(),
                        row.getCentreY(),
                        (float) textArea.getX() + renderedTextWidth,
                        row.getCentreY(),
                        1.2f);
        }

        g.setColour (kDanger);
        g.drawRoundedRectangle (del, 4.0f, 1.0f);
        g.drawLine (del.getX() + 5.0f, del.getY() + 5.0f, del.getRight() - 5.0f, del.getBottom() - 5.0f, 1.2f);
        g.drawLine (del.getRight() - 5.0f, del.getY() + 5.0f, del.getX() + 5.0f, del.getBottom() - 5.0f, 1.2f);
    }
}

void TaskListComponent::resized()
{
    refreshSize();
}

void TaskListComponent::mouseDown (const juce::MouseEvent& event)
{
    pressed = hitAt (event.position);
    if (pressed.index < 0)
        return;

    if (pressed.zone == HitZone::Row)
    {
        dragFrom = pressed.index;
        dragOver = pressed.index;
        dragging = true;
        repaint();
    }
}

void TaskListComponent::mouseDrag (const juce::MouseEvent& event)
{
    if (! dragging || dragFrom < 0)
        return;

    auto hit = hitAt (event.position);
    if (hit.index >= 0 && hit.index != dragOver)
    {
        dragOver = hit.index;
        repaint();
    }
}

void TaskListComponent::mouseUp (const juce::MouseEvent& event)
{
    const auto released = hitAt (event.position);

    if (dragging)
    {
        if (dragFrom >= 0 && dragOver >= 0 && dragOver != dragFrom)
            processor.moveTask (dragFrom, dragOver);
    }
    else if (pressed.index >= 0 && pressed.index == released.index)
    {
        if (pressed.zone == HitZone::Checkbox)
        {
            const auto task = processor.getTask (pressed.index);
            processor.setTaskDone (pressed.index, ! task.done);
        }
        else if (pressed.zone == HitZone::Delete)
        {
            processor.removeTask (pressed.index);
        }
    }

    dragFrom = -1;
    dragOver = -1;
    dragging = false;
    pressed = {};
    refreshSize();
    repaint();
}

int TaskListComponent::getPreferredHeight() const
{
    return juce::jmax (120, processor.getNumTasks() * rowHeight + 8);
}

void TaskListComponent::refreshSize()
{
    setSize (juce::jmax (200, getWidth()), getPreferredHeight());
}

TaskListComponent::HitInfo TaskListComponent::hitAt (juce::Point<float> p) const
{
    const int index = (int) (p.y / (float) rowHeight);
    if (! juce::isPositiveAndBelow (index, processor.getNumTasks()))
        return {};

    if (getCheckboxBounds (index).contains (p))
        return { index, HitZone::Checkbox };
    if (getDeleteBounds (index).contains (p))
        return { index, HitZone::Delete };
    return { index, HitZone::Row };
}

juce::Rectangle<float> TaskListComponent::getCheckboxBounds (int row) const
{
    const float y = (float) row * (float) rowHeight;
    return { 12.0f, y + 9.0f, 14.0f, 14.0f };
}

juce::Rectangle<float> TaskListComponent::getDeleteBounds (int row) const
{
    const float y = (float) row * (float) rowHeight;
    return { (float) getWidth() - 30.0f, y + 8.0f, 18.0f, 18.0f };
}

TodoListNativeAudioProcessorEditor::TodoListNativeAudioProcessorEditor (TodoListNativeAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p), taskList (p)
{
    addAndMakeVisible (viewport);
    viewport.setViewedComponent (&taskList, false);
    viewport.setScrollBarsShown (true, false);

    addAndMakeVisible (input);
    input.setColour (juce::TextEditor::backgroundColourId, kPanel.darker (0.2f));
    input.setColour (juce::TextEditor::textColourId, kText);
    input.setColour (juce::TextEditor::outlineColourId, kBorder);
    input.setTextToShowWhenEmpty ("type task and press enter", kMuted);
    input.onReturnKey = [this] { addFromInput(); };

    addAndMakeVisible (addButton);
    addButton.setColour (juce::TextButton::buttonColourId, kAccent.darker (0.25f));
    addButton.setColour (juce::TextButton::textColourOnId, kText);
    addButton.setColour (juce::TextButton::textColourOffId, kText);
    addButton.addListener (this);

    addAndMakeVisible (collapseButton);
    collapseButton.addListener (this);

    addAndMakeVisible (popoutButton);
    popoutButton.addListener (this);
    popoutButton.setColour (juce::TextButton::buttonColourId, kPanel.brighter (0.12f));

    addAndMakeVisible (stats);
    stats.setColour (juce::Label::textColourId, kMuted);
    stats.setJustificationType (juce::Justification::centredRight);

    audioProcessor.onTasksChanged = [this]
    {
        juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<TodoListNativeAudioProcessorEditor> (this)]
        {
            if (safe != nullptr)
                safe->refreshFromState();
        });
    };

    setSize (430, 360);
    refreshFromState();
}

TodoListNativeAudioProcessorEditor::~TodoListNativeAudioProcessorEditor()
{
    closeDetachedWindow();
    audioProcessor.onTasksChanged = nullptr;
}

void TodoListNativeAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (kBg);
    g.setColour (kBorder);
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 8.0f, 1.0f);
    g.setColour (kText);
    g.setFont (juce::Font (juce::FontOptions (15.0f, juce::Font::bold)));
    g.drawText ("todo list", 12, 8, 160, 24, juce::Justification::centredLeft);
}

void TodoListNativeAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (10);
    auto header = area.removeFromTop (28);
    popoutButton.setBounds (header.removeFromRight (90));
    if (mainPopOnlyMode)
    {
        collapseButton.setVisible (false);
        stats.setVisible (false);
        viewport.setVisible (false);
        input.setVisible (false);
        addButton.setVisible (false);
        return;
    }

    collapseButton.setVisible (true);
    stats.setVisible (true);
    collapseButton.setBounds (header.removeFromRight (100));
    stats.setBounds (header.removeFromRight (160));

    if (audioProcessor.getCollapsed())
        return;

    auto inputRow = area.removeFromBottom (34);
    addButton.setBounds (inputRow.removeFromRight (68));
    input.setBounds (inputRow.reduced (0, 2));
    viewport.setBounds (area.reduced (0, 4));
    taskList.setSize (viewport.getWidth() - 8, taskList.getPreferredHeight());
}

void TodoListNativeAudioProcessorEditor::buttonClicked (juce::Button* button)
{
    if (button == &addButton)
    {
        addFromInput();
        return;
    }

    if (button == &popoutButton)
    {
        toggleDetachedWindow();
        return;
    }

    if (button == &collapseButton)
    {
        audioProcessor.setCollapsed (! audioProcessor.getCollapsed());
        updateCollapsedLayout();
    }
}

void TodoListNativeAudioProcessorEditor::refreshFromState()
{
    taskList.refreshSize();
    updateStats();
    updateCollapsedLayout();
    repaint();
}

void TodoListNativeAudioProcessorEditor::addFromInput()
{
    audioProcessor.addTask (input.getText());
    input.clear();
}

void TodoListNativeAudioProcessorEditor::updateCollapsedLayout()
{
    if (mainPopOnlyMode)
    {
        updateMainWindowMode();
        return;
    }

    const auto isCollapsed = audioProcessor.getCollapsed();
    collapseButton.setButtonText (isCollapsed ? "expand" : "collapse");
    viewport.setVisible (! isCollapsed);
    input.setVisible (! isCollapsed);
    addButton.setVisible (! isCollapsed);

    const int width = 430;
    const int targetHeight = isCollapsed ? 56 : 360;
    if (getWidth() != width || getHeight() != targetHeight)
        setSize (width, targetHeight);

    resized();
}

void TodoListNativeAudioProcessorEditor::updateStats()
{
    const auto total = audioProcessor.getNumTasks();
    int active = 0;
    for (int i = 0; i < total; ++i)
        if (! audioProcessor.getTask (i).done)
            ++active;

    stats.setText (juce::String (active) + " active | " + juce::String (total - active) + " done",
                   juce::dontSendNotification);
}

void TodoListNativeAudioProcessorEditor::toggleDetachedWindow()
{
    if (detachedWindow != nullptr)
    {
        closeDetachedWindow();
        return;
    }

    class DetachedWindow final : public juce::DocumentWindow
    {
    public:
        DetachedWindow() : DocumentWindow ("todo list", kBg, closeButton, true) {}
        std::function<void()> onClosePressed;

        void closeButtonPressed() override
        {
            if (onClosePressed)
                onClosePressed();
        }
    };

    auto w = std::make_unique<DetachedWindow>();
    w->setUsingNativeTitleBar (true);
    w->setAlwaysOnTop (true);
    w->setResizable (true, false);
    w->setResizeLimits (360, 56, 860, 900);
    w->setContentOwned (new DetachedTodoComponent (audioProcessor), true);
    w->centreWithSize (430, 330);
    w->onClosePressed = [this] { closeDetachedWindow(); };
    w->setVisible (true);

    detachedWindow = std::move (w);
    collapsedBeforePopout = audioProcessor.getCollapsed();
    audioProcessor.setCollapsed (true);
    mainPopOnlyMode = true;
    updateMainWindowMode();
}

void TodoListNativeAudioProcessorEditor::closeDetachedWindow()
{
    if (detachedWindow == nullptr)
        return;

    detachedWindow->setVisible (false);
    detachedWindow.reset();
    mainPopOnlyMode = false;
    audioProcessor.setCollapsed (collapsedBeforePopout);
    updateMainWindowMode();
}

void TodoListNativeAudioProcessorEditor::updateMainWindowMode()
{
    if (mainPopOnlyMode)
    {
        popoutButton.setButtonText ("close pop");
        if (getWidth() != 220 || getHeight() != 52)
            setSize (220, 52);
        resized();
        return;
    }

    popoutButton.setButtonText ("pop out");
    updateCollapsedLayout();
}
