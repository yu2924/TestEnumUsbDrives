//
//  UsbVolumeList.h
//
//  Created by yu2924 on 2021-02-05
//  CC0 1.0 Universal
//

#pragma once

#include <JuceHeader.h>

class UsbVolumeList : public juce::ChangeBroadcaster
{
protected:
	class Impl;
	std::unique_ptr<Impl> impl;
public:
	std::function<void()> onChange;
	struct Volume
	{
		// friendlyDisplayName e.g. "MACRIUM_PE (H:)" or "MACRIUM_PE"
		juce::String friendlyDisplayName;
		// rootDirectory e.g. "H:\" or "/Volumes/NO NAME/"
		juce::File rootDirectory;
		// volumeLabel e.g. "MACRIUM_PE"
		juce::String volumeLabel;
		// fileSystemName e.g. "FAT32" or "MS-DOS (FAT32)"
		juce::String fileSystemName;
		// friendlyDeviceName e.g. "JetFlash Transcend 2GB USB Device" or "JetFlash Transcend 2GB"
		juce::String friendlyDeviceName;
		// key of the ejectable volume container
		//   win32: the DEVINST of the containing device
		//   macos: the "BSD name" of the whole disk
		juce::var deviceInstance;
	};
	UsbVolumeList();
	virtual ~UsbVolumeList();
	void refresh();
	const juce::Array<Volume>& getArray() const;
	void ejectWholeDevice(const juce::var& devinst, std::function<void(const juce::Result&)> callback);
	void ejectWholeDeviceAtIndex(int ivol, std::function<void(const juce::Result&)> callback);
	bool isEjecting() const;
};
