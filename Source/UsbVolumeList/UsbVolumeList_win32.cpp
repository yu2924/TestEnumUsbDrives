//
//  UsbVolumeList_win32.cpp
//
//  Created by yu2924 on 2021-02-05
//  CC0 1.0 Universal
//

#include "UsbVolumeList.h"

#if JUCE_WINDOWS

#define STRICT
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define NOKERNEL
#define NOGDI
#define NOSERVICE
#define NOMCX
#define NOIME
#include <Windows.h>
#include <winioctl.h>
// #include <Dbt.h>
#include <SetupAPI.h>
#pragma comment(lib, "SetupAPI.lib")
#include <cfgmgr32.h>
#pragma comment(lib, "Cfgmgr32.lib")

namespace UsbVolumeListImpl
{
	// purpose:
	//   obtain a list of { drive, disk } pair
	// procedure:
	//   1. obtain the list of disks (i.e. containers of multiple volumes)
	//   2. obtain the list of volumes
	//   3. obtain the list of drives (i.e. volumes with drive letter given)
	//   4. lookup drive => volume, then lookup volume => disk
	static_assert(sizeof(DEVINST) == sizeof(juce::uint32), "sizeof(DEVINST)==sizeof(juce::uint32)");
	// --------------------------------------------------------------------------------
	// disk list
	struct DiskEntry
	{
		DEVINST parentDevInst; // i.e. ejectable container
		juce::String diskId; // e.g. "USBSTOR\DISK&VEN_JETFLASH&PROD_TRANSCEND_2GB&REV_8.07\RA4NEY1B&0"
		juce::String friendlyName; // e.g. "JetFlash Transcend 2GB USB Device"
		juce::StringArray volumeIdList; // e.g. { "STORAGE\Volume\_??_USBSTOR#Disk&Ven_JetFlash&Prod_Transcend_2GB&Rev_8.07#RA4NEY1B&0#{53f56307-b6bf-11d0-94f2-00a0c91efb8b}" }
	};
	static juce::Array<DiskEntry> enumDiskList(bool removableonly)
	{
		HDEVINFO hdevinfo = SetupDiGetClassDevsW(&GUID_DEVINTERFACE_DISK, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
		if(hdevinfo == INVALID_HANDLE_VALUE) return {};
		juce::Array<DiskEntry> disklist;
		for(DWORD idev = 0; ; ++idev)
		{
			SP_DEVINFO_DATA devinfodata = {};
			devinfodata.cbSize = sizeof(devinfodata);
			if(!SetupDiEnumDeviceInfo(hdevinfo, idev, &devinfodata)) break;
			DiskEntry entry = {};
			// parent devinst
			{
				DEVINST parentdevinst = 0;
				CM_Get_Parent(&parentdevinst, devinfodata.DevInst, 0);
				if(!parentdevinst) continue;
				ULONG status = 0, problem = 0;
				CM_Get_DevNode_Status(&status, &problem, parentdevinst, 0);
				if(removableonly)
				{
					ULONG m = DN_DISABLEABLE | DN_REMOVABLE;
					if((status & m) != m) continue;
				}
				entry.parentDevInst = parentdevinst;
			}
			// diskid
			{
				ULONG cc = 0;
				CM_Get_Device_ID_Size(&cc, devinfodata.DevInst, 0);
				std::vector<WCHAR> buf((size_t)cc + 1, 0);
				CM_Get_Device_IDW(devinfodata.DevInst, buf.data(), cc, 0);
				entry.diskId = buf.data();
			}
			// SPDRP_FRIENDLYNAME
			{
				DWORD cb = 0;
				SetupDiGetDeviceRegistryPropertyW(hdevinfo, &devinfodata, SPDRP_FRIENDLYNAME, NULL, NULL, 0, &cb);
				if(GetLastError() == ERROR_INSUFFICIENT_BUFFER)
				{
					std::vector<uint8_t> buf(cb, 0);
					SetupDiGetDeviceRegistryPropertyW(hdevinfo, &devinfodata, SPDRP_FRIENDLYNAME, NULL, buf.data(), cb, &cb);
					entry.friendlyName = juce::String((LPCWSTR)buf.data());
				}
			}
			// volumeidlist
			{
				ULONG cc = 0;
				CM_Get_Device_ID_List_SizeW(&cc, entry.diskId.toUTF16(), CM_GETIDLIST_FILTER_REMOVALRELATIONS);
				std::vector<WCHAR> buf((size_t)cc + 1, 0);
				CM_Get_Device_ID_ListW(entry.diskId.toUTF16(), buf.data(), cc, CM_GETIDLIST_FILTER_REMOVALRELATIONS);
				if(!buf.empty()) { for(LPWSTR p = buf.data(); *p; p += lstrlenW(p) + 1) entry.volumeIdList.add(p); }
			}
			disklist.add(entry);
		}
		SetupDiDestroyDeviceInfoList(hdevinfo);
		return disklist;
	}
	// --------------------------------------------------------------------------------
	// volume list
	struct VolumeEntry
	{
		juce::String volumeId; // e.g. "STORAGE\VOLUME\_??_USBSTOR#DISK&VEN_JETFLASH&PROD_TRANSCEND_2GB&REV_8.07#RA4NEY1B&0#{53F56307-B6BF-11D0-94F2-00A0C91EFB8B}"
		juce::String devicePath;  // e.g. "\Device\HarddiskVolume65"
	};
	static juce::Array<VolumeEntry> enumVolumeList()
	{
		HDEVINFO hdevinfo = SetupDiGetClassDevsW(&GUID_DEVINTERFACE_VOLUME, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
		if(hdevinfo == INVALID_HANDLE_VALUE) return {};
		juce::Array<VolumeEntry> volumelist;
		for(DWORD idev = 0; ; ++idev)
		{
			SP_DEVINFO_DATA devinfodata = {};
			devinfodata.cbSize = sizeof(devinfodata);
			if(!SetupDiEnumDeviceInfo(hdevinfo, idev, &devinfodata)) break;
			VolumeEntry entry = {};
			// volumeid
			{
				ULONG cc = 0;
				CM_Get_Device_ID_Size(&cc, devinfodata.DevInst, 0);
				std::vector<WCHAR> buf((size_t)cc + 1, 0);
				CM_Get_Device_IDW(devinfodata.DevInst, buf.data(), cc, 0);
				entry.volumeId = buf.data();
			}
			// SPDRP_PHYSICAL_DEVICE_OBJECT_NAME
			{
				DWORD cb = 0;
				SetupDiGetDeviceRegistryPropertyW(hdevinfo, &devinfodata, SPDRP_PHYSICAL_DEVICE_OBJECT_NAME, NULL, NULL, 0, &cb);
				if(GetLastError() == ERROR_INSUFFICIENT_BUFFER)
				{
					std::vector<uint8_t> buf(cb, 0);
					SetupDiGetDeviceRegistryPropertyW(hdevinfo, &devinfodata, SPDRP_PHYSICAL_DEVICE_OBJECT_NAME, NULL, buf.data(), cb, &cb);
					entry.devicePath = juce::String((LPCWSTR)buf.data());
				}
			}
			volumelist.add(entry);
		}
		SetupDiDestroyDeviceInfoList(hdevinfo);
		return volumelist;
	}
	// --------------------------------------------------------------------------------
	// drive list
	struct DriveEntry
	{
		juce::String driveLetter; // e.g. "H"
		juce::String devicePath; // e.g. "\Device\HarddiskVolume65"
		juce::String volumeLabel; // e.g. "MACRIUM_PE"
		juce::String fileSystemName; // e.g. "FAT32"
	};
	static juce::Array<DriveEntry> enumDriveList(bool removableonly)
	{
		juce::Array<DriveEntry> drivelist;
		WCHAR drvnames[128] = {};
		GetLogicalDriveStringsW(_countof(drvnames), drvnames);
		for(LPCWSTR drvletter = drvnames; *drvletter; drvletter += lstrlenW(drvletter) + 1)
		{
			WCHAR devname[3] = { *drvletter, L':', 0 }; // e.g. "H:"
			WCHAR rootdir[4] = { *drvletter, L':', L'\\', 0 }; // e.g. "H:\"
			WCHAR devpath[MAX_PATH] = {};
			if(!QueryDosDeviceW(devname, devpath, _countof(devpath))) continue;
			if(removableonly)
			{
				if(GetDriveTypeW(rootdir) != DRIVE_REMOVABLE) continue;
			}
			WCHAR volname[MAX_PATH + 1]{};
			WCHAR fsname[MAX_PATH + 1]{};
			GetVolumeInformationW(rootdir, volname, _countof(volname), nullptr, nullptr, nullptr, fsname, _countof(fsname));
			DriveEntry entry = {};
			entry.driveLetter = juce::String(drvletter, 1);
			entry.devicePath = juce::String(devpath);
			entry.volumeLabel = juce::String(volname);
			entry.fileSystemName = juce::String(fsname);
			drivelist.add(entry);
		}
		return drivelist;
	}
	// --------------------------------------------------------------------------------
	static juce::Array<UsbVolumeList::Volume> populateUsbVolumeList(bool removableonly)
	{
		juce::Array<DiskEntry> disklist = enumDiskList(removableonly);
		juce::Array<VolumeEntry> volumelist = enumVolumeList();
		juce::Array<DriveEntry> drivelist = enumDriveList(removableonly);
		juce::Array<UsbVolumeList::Volume> resultlist;
		for(const auto& drive : drivelist)
		{
			// resolve volume by devicePath
			const VolumeEntry* volume = nullptr;
			for(const auto& ve : volumelist)
			{
				if(ve.devicePath == drive.devicePath) { volume = &ve; break; }
			}
			if(!volume) continue;
			// resolve disk by volumeId
			const DiskEntry* disk = nullptr;
			for(const auto& de : disklist)
			{
				if(de.volumeIdList.contains(volume->volumeId, true)) { disk = &de; break; }
			}
			if(!disk) continue;
			// push
			UsbVolumeList::Volume vol = {};
			vol.rootDirectory = drive.driveLetter + ":\\"; // e.g. "H:\"
			vol.volumeLabel = drive.volumeLabel; // e.g. "MACRIUM_PE"
			vol.fileSystemName = drive.fileSystemName; // e.g. "FAT32"
			vol.friendlyDeviceName = disk->friendlyName; // e.g. "JetFlash Transcend 2GB USB Device"
			vol.deviceInstance = (juce::int32)disk->parentDevInst;
			vol.friendlyDisplayName = vol.volumeLabel + " (" + drive.driveLetter + ":)"; // e.g. "MACRIUM_PE (H:)"
			resultlist.add(vol);
		}
		return resultlist;
	}
	static juce::String getPnpVetoTypeMessage(PNP_VETO_TYPE vt)
	{
		// cf. https://docs.microsoft.com/en-us/windows/win32/api/cfg/ne-cfg-pnp_veto_type
		static const struct { PNP_VETO_TYPE code; juce::String desc; } TABLE[] =
		{
			{ PNP_VetoTypeUnknown			, "The specified operation was rejected for an unknown reason." },
			{ PNP_VetoLegacyDevice			, "The device does not support the specified PnP operation." },
			{ PNP_VetoPendingClose			, "The specified operation cannot be completed because of a pending close operation." },
			{ PNP_VetoWindowsApp			, "A Microsoft Win32 application vetoed the specified operation." },
			{ PNP_VetoWindowsService		, "A Win32 service vetoed the specified operation." },
			{ PNP_VetoOutstandingOpen		, "The requested operation was rejected because of outstanding open handles." },
			{ PNP_VetoDevice				, "The device supports the specified operation, but the device rejected the operation." },
			{ PNP_VetoDriver				, "The driver supports the specified operation, but the driver rejected the operation." },
			{ PNP_VetoIllegalDeviceRequest	, "The device does not support the specified operation." },
			{ PNP_VetoInsufficientPower		, "There is insufficient power to perform the requested operation." },
			{ PNP_VetoNonDisableable		, "The device cannot be disabled." },
			{ PNP_VetoLegacyDriver			, "The driver does not support the specified PnP operation." },
			{ PNP_VetoInsufficientRights	, "The caller has insufficient privileges to complete the operation." },
		};
		for(const auto& e : TABLE) { if(e.code == vt) return TRANS(e.desc); }
		return "unknown";
	}
}

using namespace UsbVolumeListImpl;

class UsbVolumeList::Impl : public juce::AsyncUpdater
{
public:
	static DWORD CALLBACK NotifyCallbackProc(HCMNOTIFICATION, PVOID ctx, CM_NOTIFY_ACTION action, PCM_NOTIFY_EVENT_DATA, DWORD)
	{
		// if(action == CM_NOTIFY_ACTION_DEVICEINTERFACEARRIVAL) { DBG("CM_NOTIFY_ACTION_DEVICEINTERFACEARRIVAL"); }
		// if(action == CM_NOTIFY_ACTION_DEVICEINTERFACEREMOVAL) { DBG("CM_NOTIFY_ACTION_DEVICEINTERFACEREMOVAL"); }
		if((action == CM_NOTIFY_ACTION_DEVICEINTERFACEARRIVAL) || (action == CM_NOTIFY_ACTION_DEVICEINTERFACEREMOVAL))
		{
			if(Impl* p = (Impl*)ctx) p->triggerAsyncUpdate();
		}
		return ERROR_SUCCESS;
	}
	UsbVolumeList* mOuter;
	HCMNOTIFICATION mHcmn{};
	juce::Array<Volume> mVolumeList;
	bool mEjecting = false;
	Impl(UsbVolumeList* p) : mOuter(p)
	{
		CM_NOTIFY_FILTER nf{ sizeof(nf) };
		nf.FilterType = CM_NOTIFY_FILTER_TYPE_DEVICEINTERFACE;
		nf.u.DeviceInterface.ClassGuid = GUID_DEVINTERFACE_VOLUME;
		CM_Register_Notification(&nf, this, NotifyCallbackProc, &mHcmn);
		mVolumeList = populateUsbVolumeList(true);
	}
	virtual ~Impl()
	{
		if(mHcmn) CM_Unregister_Notification(mHcmn);
	}
	virtual void handleAsyncUpdate() override
	{
		refresh();
	}
	void refresh()
	{
		mVolumeList = populateUsbVolumeList(true);
		if(mOuter->onChange) mOuter->onChange();
		mOuter->sendChangeMessage();
	}
	const juce::Array<Volume>& getArray() const
	{
		return mVolumeList;
	}
	void ejectWholeDevice(const juce::var& devinst, std::function<void(const juce::Result&)> callback)
	{
		if(mEjecting) { if(callback) callback(juce::Result::fail("nested invocation")); return; }
		mEjecting = true;
		PNP_VETO_TYPE vt = PNP_VetoTypeUnknown;
		WCHAR vname[MAX_PATH] = {};
		CONFIGRET r = CM_Request_Device_EjectW((DEVINST)(juce::int32)devinst, &vt, vname, _countof(vname), 0);
		mEjecting = false;
		if(r == CR_SUCCESS)
		{
			if(callback) callback(juce::Result::ok());
		}
		else
		{
			juce::String err;
			err << getPnpVetoTypeMessage(vt);
			if(*vname) err << " " << juce::String(vname).quoted();
			if(callback) callback(juce::Result::fail(err));
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
};

UsbVolumeList::UsbVolumeList() { impl = std::make_unique<Impl>(this); }
UsbVolumeList::~UsbVolumeList() { impl.reset(); }
void UsbVolumeList::refresh() { impl->refresh(); }
const juce::Array<UsbVolumeList::Volume>& UsbVolumeList::getArray() const { return impl->getArray(); }
void UsbVolumeList::ejectWholeDevice(const juce::var& devinst, std::function<void(const juce::Result&)> callback) { impl->ejectWholeDevice(devinst, callback); }
void UsbVolumeList::ejectWholeDeviceAtIndex(int ivol, std::function<void(const juce::Result&)> callback) { impl->ejectWholeDeviceAtIndex(ivol, callback); }
bool UsbVolumeList::isEjecting() const { return impl->isEjecting(); }

#endif // JUCE_WINDOWS
