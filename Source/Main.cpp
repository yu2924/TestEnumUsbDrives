//
//  Main.cpp
//  TestEnumUsbDrives_App
//
//  Created by yu2924 on 2021-02-05
//

#include <JuceHeader.h>
#include "UsbVolumeList/UsbVolumeList.h"

class MainComponent : public juce::Component
{
private:
    UsbVolumeList volumeList;
    juce::ComboBox volumeCombo;
    juce::TextButton ejectButton;
    juce::TextButton refleshButton;
    juce::Label infoLabel;
    juce::File currentVolumeRootDirectory;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
public:
    MainComponent()
    {
        volumeCombo.onChange = [this]() { updateInfoLabel(); };
        addAndMakeVisible(volumeCombo);
        ejectButton.setButtonText("eject");
        ejectButton.onClick = [this]()
        {
            int ii = volumeCombo.getSelectedItemIndex();
            if((size_t)volumeList.getArray().size() <= (size_t)ii) return;
            if(volumeList.isEjecting()) return;
            volumeList.ejectWholeDevice(ii, [this](const juce::Result& r)
            {
                if(r.failed())
                {
                    DBG("ERROR: " << r.getErrorMessage());
                    juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::AlertIconType::WarningIcon, "Error", r.getErrorMessage());
                }
                updateInfoLabel();
            });
        };
        addAndMakeVisible(ejectButton);
        refleshButton.setButtonText("update");
        refleshButton.onClick = [this]()
        {
            volumeList.refresh();
        };
        addAndMakeVisible(refleshButton);
        volumeList.onChange = [this]()
        {
            refill();
        };
        addAndMakeVisible(infoLabel);
        refill();
        setSize(480, 200);
    }
    void refill()
    {
        volumeCombo.clear(juce::NotificationType::dontSendNotification);
        int iselection = 0;
        for(const auto& vol : volumeList.getArray())
        {
            int ii = volumeCombo.getNumItems();
            volumeCombo.addItem(vol.friendlyDisplayName, ii + 1);
            if(vol.rootDirectory == currentVolumeRootDirectory) iselection = ii;
        }
        volumeCombo.setSelectedItemIndex(iselection, juce::NotificationType::dontSendNotification);
        updateInfoLabel();
    }
    void updateInfoLabel()
    {
        int ii = volumeCombo.getSelectedItemIndex();
        if((size_t)ii < (size_t)volumeList.getArray().size())
        {
            const UsbVolumeList::Volume& vol = volumeList.getArray().getReference(ii);
            currentVolumeRootDirectory = vol.rootDirectory;
            juce::StringArray sa;
            sa.add("friendlyDisplayName=" + vol.friendlyDisplayName);
            sa.add("rootDirectory=" + vol.rootDirectory.getFullPathName());
            sa.add("volumeLabel=" + vol.volumeLabel);
            sa.add("filesystemname=" + vol.fileSystemName);
            sa.add("friendlyDeviceName=" + vol.friendlyDeviceName);
            sa.add("deviceInstance=" + vol.deviceInstance.toString());
            infoLabel.setText(sa.joinIntoString("\n"), juce::NotificationType::dontSendNotification);

        }
        else
        {
            currentVolumeRootDirectory = juce::File();
            infoLabel.setText("", juce::NotificationType::dontSendNotification);
        }
    }
    virtual void resized() override
    {
        enum { Margin = 20, Spacing = 5, ItemHeight = 20, ButtonWidth = 50 };
        juce::Rectangle<int> rc = getLocalBounds().reduced(Margin);
        juce::Rectangle<int> rcrow = rc.removeFromTop(ItemHeight);
        refleshButton.setBounds(rcrow.removeFromRight(ButtonWidth));
        ejectButton.setBounds(rcrow.removeFromRight(ButtonWidth));
        volumeCombo.setBounds(rcrow);
        rc.removeFromTop(Spacing);
        infoLabel.setBounds(rc);
    }
    virtual void paint(juce::Graphics& g) override
    {
        g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
    }
};

class MainWindow : public juce::DocumentWindow
{
public:
    MainWindow(juce::String name)
        : DocumentWindow(name, juce::Desktop::getInstance().getDefaultLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId), DocumentWindow::allButtons)
    {
        setUsingNativeTitleBar(true);
        setContentOwned(new MainComponent(), true);
#if JUCE_IOS || JUCE_ANDROID
        setFullScreen(true);
#else
        setResizable(true, true);
        centreWithSize(getWidth(), getHeight());
#endif
        setVisible(true);
    }
    virtual void closeButtonPressed() override
    {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }
private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
};

class TestEnumUsbDrivesApplication : public juce::JUCEApplication
{
public:
    TestEnumUsbDrivesApplication() {}
    virtual const juce::String getApplicationName() override { return ProjectInfo::projectName; }
    virtual const juce::String getApplicationVersion() override { return ProjectInfo::versionString; }
    virtual bool moreThanOneInstanceAllowed() override { return true; }
    virtual void initialise(const juce::String&) override
    {
        mainWindow.reset(new MainWindow(getApplicationName()));
    }
    virtual void shutdown() override
    {
        mainWindow = nullptr;
    }
    virtual void systemRequestedQuit() override
    {
        quit();
    }
    virtual void anotherInstanceStarted(const juce::String&) override
    {
    }
private:
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION (TestEnumUsbDrivesApplication)
