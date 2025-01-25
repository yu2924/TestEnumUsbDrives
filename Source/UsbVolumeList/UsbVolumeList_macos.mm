//
//  UsbVolumeList_macos.mm
//
//  Created by yu2924 on 2021-02-05
//  CC0 1.0 Universal
//

// requires DiskArbitration.framework

#import "UsbVolumeList.h"

#if JUCE_MAC

#import <Foundation/Foundation.h>
#import <DiskArbitration/DiskArbitration.h>

namespace UsbVolumeListImpl
{
#if DEBUG
	static void DbgDumpDic(NSString* title, NSDictionary* dic)
	{
		NSLog(@"[%@]", title);
		for(NSString* key in dic)
		{
			NSLog(@"%@, %@, \"%@\"", key, [dic[key] class], dic[key]);
		}
		NSLog(@"");
	}
#else
#define DbgDumpDic(title, dic)
#endif
	static juce::Array<UsbVolumeList::Volume> enumVolumeList(DASessionRef session)
	{
		juce::Array<UsbVolumeList::Volume> result;
		JUCE_AUTORELEASEPOOL
		{
			NSString* dir = @"/Volumes";
			for(NSString* fn in [NSFileManager.defaultManager contentsOfDirectoryAtPath:dir error:nil])
			{
				NSString* path = [dir stringByAppendingPathComponent:fn];
				DADiskRef voldisk = DADiskCreateFromVolumePath(kCFAllocatorDefault, session, (CFURLRef)[NSURL fileURLWithPath:path]);
				NSDictionary* volumedic = (NSDictionary*)DADiskCopyDescription(voldisk);
				DADiskRef wholedisk = DADiskCopyWholeDisk(voldisk);
				NSDictionary* wholedic = (NSDictionary*)DADiskCopyDescription(wholedisk);
				NSNumber* removable = wholedic[@"DAMediaRemovable"];
				NSNumber* ejectable = wholedic[@"DAMediaEjectable"];
				if(removable.boolValue && ejectable.boolValue)
				{
					// collect
					// DbgDumpDic(@"volume", volumedic);
					// DbgDumpDic(@"disk", wholedic);
					// NSString* vVolumeName = volumedic[@"DAVolumeName"]; // e.g."NO NAME"
					NSURL* vVolumePath = volumedic[@"DAVolumePath"]; // e.g. "file:///Volumes/NO%20NAME/"
					NSString* vVolumeType = volumedic[@"DAVolumeType"]; // e.g. "MS-DOS (FAT32)"
					NSString* vVolumeKind = volumedic[@"DAVolumeKind"]; // e.g. "exfat", only on SD cards?
					NSString* wDeviceVendor = wholedic[@"DADeviceVendor"]; // e.g. "JetFlash"
					NSString* wDeviceModel = wholedic[@"DADeviceModel"]; // e.g. "Transcend 2GB"
					NSString* wBSDName = wholedic[@"DAMediaBSDName"]; // e.g. "disk5"
					// NSString* wMediaName = wholedic[@"DAMediaName"]; // e.g. "JetFlash Transcend 2GB Media"
					// pack
					juce::File volumerootdir(juce::String(vVolumePath.path.UTF8String)); // e.g. "/Volumes/NO NAME"
					juce::String volumelabel = volumerootdir.getVolumeLabel(); // e.g. "MACRIUM_PE";
					UsbVolumeList::Volume vol;
					vol.friendlyDisplayName = volumelabel;
					vol.rootDirectory = volumerootdir;
					vol.volumeLabel = volumelabel;
					vol.fileSystemName = juce::String(vVolumeType ? vVolumeType.UTF8String : vVolumeKind.UTF8String);
					vol.friendlyDeviceName = juce::String(wDeviceVendor.UTF8String) + " " + juce::String(wDeviceModel.UTF8String);
					vol.deviceInstance = juce::String(wBSDName.UTF8String);
					result.add(vol);
				}
				if(wholedic) [wholedic release];
				if(wholedisk) CFRelease(wholedisk);
				if(volumedic) [volumedic release];
				if(voldisk) CFRelease(voldisk);
			}
		}
		// DBG("enumVolumeList: " << result.size() << " items");
		// for(const auto& vol : result) DBG("  " << vol.friendlyName.quoted() << " " << vol.rootDirectory.getFullPathName().quoted() << " wholebsdname=" << vol.deviceInstance.toString().quoted());
		return result;
	}
	static juce::String getDissenterMessage(DADissenterRef dissenter)
	{
		juce::String msg;
		JUCE_AUTORELEASEPOOL
		{
			msg << juce::String::formatted("code: 0x%08x", DADissenterGetStatus(dissenter));
			NSString* status = (NSString*)DADissenterGetStatusString(dissenter);
			if(status) msg << " " << juce::String(status.UTF8String);
		}
		return msg;
	}
}

using namespace UsbVolumeListImpl;

class UsbVolumeList::Impl : public juce::Timer
{
public:
	// NOTE: there is slight delay before the BSD path (the "DAVolumePath" property) is given
	static void onDiskAppeared(DADiskRef, void* ctx)
	{
		// DBG("DiskAppeared");
		((Impl*)ctx)->startTimer(1000);
	}
	static void onDiskDisappeared(DADiskRef, void* ctx)
	{
		// DBG("DiskDisappeared");
		((Impl*)ctx)->startTimer(100);
	}
	UsbVolumeList* mOuter;
	DASessionRef mDASession{};
	juce::Array<Volume> mVolumeList;
	std::function<void(const juce::Result&)> mEjectCallback;
	bool mEjecting = false;
	Impl(UsbVolumeList* p) : mOuter(p)
	{
		mDASession = DASessionCreate(NULL);
		DARegisterDiskAppearedCallback(mDASession, kDADiskDescriptionMatchVolumeMountable, onDiskAppeared, this);
		DARegisterDiskDisappearedCallback(mDASession, kDADiskDescriptionMatchVolumeMountable, onDiskDisappeared, this);
		DASessionSetDispatchQueue(mDASession, dispatch_get_main_queue());
		mVolumeList = enumVolumeList(mDASession);
	}
	virtual ~Impl()
	{
		DAUnregisterCallback(mDASession, (void*)onDiskDisappeared, this);
		DAUnregisterCallback(mDASession, (void*)onDiskAppeared, this);
		DASessionSetDispatchQueue(mDASession, NULL);
		if(mDASession) CFRelease(mDASession);
	}
	virtual void timerCallback () override
	{
		stopTimer();
		refresh();
	}
	void refresh()
	{
		mVolumeList = enumVolumeList(mDASession);
		if(mOuter->onChange) mOuter->onChange();
		mOuter->sendChangeMessage();
	}
	const juce::Array<Volume>& getArray() const
	{
		return mVolumeList;
	}
	void ejectWholeDevice(const juce::var& devinst, std::function<void(const juce::Result&)> callback)
	{
		JUCE_AUTORELEASEPOOL
		{
			if(mEjecting) { if(callback) callback(juce::Result::fail("nested invocation")); return; }
			DADiskRef wdisk = DADiskCreateFromBSDName(kCFAllocatorDefault, mDASession, devinst.toString().toRawUTF8());
			if(!wdisk) { if(callback) callback(juce::Result::fail("disk not found")); return; }
			mEjectCallback = callback;
			mEjecting = true;
			// DBG("calling DADiskUnmount()...");
			DADiskUnmount(wdisk, kDADiskUnmountOptionWhole, handleDiskUnmount, this);
			CFRelease(wdisk);
		}
	}
	void ejectWholeDeviceAtIndex(int ivol, std::function<void(const juce::Result&)> callback)
	{
		if((size_t)mVolumeList.size() <= (size_t)ivol) { if(callback) callback(juce::Result::fail("invalid index")); return; }
		const Volume& vol = mVolumeList.getReference(ivol);
		ejectWholeDevice(vol.deviceInstance, callback);
	}
	bool isEjecting() const
	{
		return mEjecting;
	}
	static void handleDiskUnmount(DADiskRef disk, DADissenterRef dissenter, void* context)
	{
		Impl* pthis = (Impl*)context;
		if(dissenter)
		{
			pthis->mEjecting = false;
			juce::String msg = getDissenterMessage(dissenter);
			// DBG("DADiskUnmountCallback: error " << msg);
			if(pthis->mEjectCallback) (pthis->mEjectCallback)(juce::Result::fail(msg));
			pthis->mEjectCallback = nullptr;
		}
		else
		{
			// DBG("DADiskUnmountCallback: succeeded");
			// DBG("calling DADiskEject()...");
			DADiskEject(disk, kDADiskEjectOptionDefault, handleDiskEject, context);
		}
	}
	static void handleDiskEject(DADiskRef disk, DADissenterRef dissenter, void* context)
	{
		Impl* pthis = (Impl*)context;
		pthis->mEjecting = false;
		if(dissenter)
		{
			juce::String msg = getDissenterMessage(dissenter);
			// DBG("DADiskEjectCallback: error " << msg);
			if(pthis->mEjectCallback) pthis->mEjectCallback(juce::Result::fail(msg));
		}
		else
		{
			// DBG("DADiskEjectCallback: succeeded");
			if(pthis->mEjectCallback) pthis->mEjectCallback(juce::Result::ok());
		}
		pthis->mEjectCallback = nullptr;
	}
};

UsbVolumeList::UsbVolumeList() { impl = std::make_unique<Impl>(this); }
UsbVolumeList::~UsbVolumeList() { impl.reset(); }
void UsbVolumeList::refresh() { impl->refresh(); }
const juce::Array<UsbVolumeList::Volume>& UsbVolumeList::getArray() const { return impl->getArray(); }
void UsbVolumeList::ejectWholeDevice(const juce::var& devinst, std::function<void(const juce::Result&)> callback) { impl->ejectWholeDevice(devinst, callback); }
void UsbVolumeList::ejectWholeDeviceAtIndex(int ivol, std::function<void(const juce::Result&)> callback) { impl->ejectWholeDeviceAtIndex(ivol, callback); }
bool UsbVolumeList::isEjecting() const { return impl->isEjecting(); }

#endif // JUCE_MAC
