/* -LICENSE-START-
 ** Copyright (c) 2019 Blackmagic Design
 **  
 ** Permission is hereby granted, free of charge, to any person or organization 
 ** obtaining a copy of the software and accompanying documentation (the 
 ** "Software") to use, reproduce, display, distribute, sub-license, execute, 
 ** and transmit the Software, and to prepare derivative works of the Software, 
 ** and to permit third-parties to whom the Software is furnished to do so, in 
 ** accordance with:
 ** 
 ** (1) if the Software is obtained from Blackmagic Design, the End User License 
 ** Agreement for the Software Development Kit (“EULA”) available at 
 ** https://www.blackmagicdesign.com/EULA/DeckLinkSDK; or
 ** 
 ** (2) if the Software is obtained from any third party, such licensing terms 
 ** as notified by that third party,
 ** 
 ** and all subject to the following:
 ** 
 ** (3) the copyright notices in the Software and this entire statement, 
 ** including the above license grant, this restriction and the following 
 ** disclaimer, must be included in all copies of the Software, in whole or in 
 ** part, and all derivative works of the Software, unless such copies or 
 ** derivative works are solely in the form of machine-executable object code 
 ** generated by a source language processor.
 ** 
 ** (4) THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS 
 ** OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
 ** FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT 
 ** SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE 
 ** FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE, 
 ** ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 ** DEALINGS IN THE SOFTWARE.
 ** 
 ** A copy of the Software is available free of charge at 
 ** https://www.blackmagicdesign.com/desktopvideo_sdk under the EULA.
 ** 
 ** -LICENSE-END-
 */

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <map>
#include <mutex>
#include <string>

#include "platform.h"
#include "DeckLinkAPI.h"


static const std::chrono::seconds kProfileActivationTimeout{ 5 };

// Profile description map 
const std::map<BMDProfileID, std::string> kDeviceProfiles =
{
	std::make_pair(bmdProfileOneSubDeviceFullDuplex,	"one sub-device full-duplex"),
	std::make_pair(bmdProfileOneSubDeviceHalfDuplex,	"one sub-device half-duplex"),
	std::make_pair(bmdProfileTwoSubDevicesFullDuplex,	"two sub-devices full-duplex"),
	std::make_pair(bmdProfileTwoSubDevicesHalfDuplex,	"two sub-devices half-duplex"),
	std::make_pair(bmdProfileFourSubDevicesHalfDuplex,	"four sub-devices half-duplex"),
};

HRESULT GetDeckLinkProfileID(IDeckLinkProfile* profile, BMDProfileID* profileID)
{
	IDeckLinkProfileAttributes*		profileAttributes = nullptr;
	HRESULT							result;

	result = profile->QueryInterface(IID_IDeckLinkProfileAttributes, (void**)&profileAttributes);
	if (result != S_OK)
		*profileID = (BMDProfileID)0;
	else
	{
		int64_t		profileIDInt;

		// Get Profile ID attribute
		result = profileAttributes->GetInt(BMDDeckLinkProfileID, &profileIDInt);

		*profileID = (BMDProfileID)((result == S_OK) ? profileIDInt : 0);

		profileAttributes->Release();
	}

	return result;
}

std::string GetDeckLinkProfileString(BMDProfileID profileID)
{
	auto profileSearch = kDeviceProfiles.find(profileID);
	if (profileSearch == kDeviceProfiles.end())
		return "";
	else
		return profileSearch->second;
}

class ProfileCallback : public IDeckLinkProfileCallback
{
public:
	ProfileCallback(IDeckLinkProfile* requestedProfile) : m_refCount(1), m_requestedProfile(requestedProfile), m_requestedProfileActivated(false)
	{
		m_requestedProfile->AddRef();
		
		GetDeckLinkProfileID(m_requestedProfile, &m_requestedProfileID);
	}

	virtual ~ProfileCallback() 
	{
		m_requestedProfile->Release();
	}

	bool WaitForProfileActivation(void)
	{
		dlbool_t isActiveProfile = false;

		// Check whether requested profile is already the active profile, then we can return without waiting
		if ((m_requestedProfile->IsActive(&isActiveProfile) == S_OK) && isActiveProfile)
		{
			fprintf(stderr, "Profile %s is active.\n", GetDeckLinkProfileString(m_requestedProfileID).c_str());
			return true;
		}
		else
		{
			std::unique_lock<std::mutex> lock(m_profileActivatedMutex);
			if (m_requestedProfileActivated)
				return true;
			else
				// Wait until the ProfileActivated callback occurs
				return m_profileActivatedCondition.wait_for(lock, kProfileActivationTimeout, [&]{ return m_requestedProfileActivated; });
		}
	}

	// IDeckLinkInputCallback interface
	
	HRESULT STDMETHODCALLTYPE ProfileChanging(IDeckLinkProfile* profileToBeActivated, dlbool_t /*streamsWillBeForcedToStop*/) override
	{
		// The profile change is stalled until we return from the callback. 
		// We don't want to block this callback, otherwise the wait condition may timeout

		BMDProfileID profileID;
		GetDeckLinkProfileID(profileToBeActivated, &profileID);

		fprintf(stderr, "Changing to profile %s.\n", GetDeckLinkProfileString(profileID).c_str());

		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE ProfileActivated(IDeckLinkProfile *activatedProfile) override
	{
		BMDProfileID activatedProfileID;
		
		GetDeckLinkProfileID(activatedProfile, &activatedProfileID);

		if (activatedProfileID == m_requestedProfileID)
		{
			{
				std::lock_guard<std::mutex> lock(m_profileActivatedMutex);
				m_requestedProfileActivated = true;
			}
			m_profileActivatedCondition.notify_one();
		}
			
		fprintf(stderr, "Profile %s has been activated.\n", GetDeckLinkProfileString(activatedProfileID).c_str());

		return S_OK;
	}

	// IUnknown interface

	HRESULT	STDMETHODCALLTYPE	QueryInterface(REFIID iid, LPVOID *ppv) override
	{
		*ppv = nullptr;
		return E_NOINTERFACE;
	}

	ULONG	STDMETHODCALLTYPE	AddRef() override
	{
		return ++m_refCount;
	}

	ULONG	STDMETHODCALLTYPE	Release() override
	{
		ULONG refCount = --m_refCount;
		if (refCount == 0)
			delete this;

		return refCount;
	}

private:
	std::condition_variable				m_profileActivatedCondition;
	std::mutex							m_profileActivatedMutex;
	BMDProfileID						m_requestedProfileID;
	IDeckLinkProfile*					m_requestedProfile;
	bool								m_requestedProfileActivated;
	
	std::atomic<ULONG>					m_refCount;
};


int main(int argc, char* argv[])
{
	// Configuration settings
	bool							displayHelp				= false;
	int								deckLinkIndex			= -1;
	int								profileIndex			= -1;

	IDeckLinkIterator*				deckLinkIterator		= nullptr;
	IDeckLink*						selectedDeckLink		= nullptr;
	IDeckLinkProfileManager*		deckLinkProfileManager	= nullptr;
	IDeckLinkProfileIterator*		deckLinkProfileIterator	= nullptr;
	IDeckLinkProfile*				selectedProfile			= nullptr;
	ProfileCallback*				profileCallback			= nullptr;

	std::string						selectedDeviceName;

	HRESULT							result;
	
	// Process the command line arguments
	for (int i = 1; i < argc; i++)
	{
		if (strcmp(argv[i], "-d") == 0)
			deckLinkIndex = atoi(argv[++i]);

		else if (strcmp(argv[i], "-p") == 0)
			profileIndex = atoi(argv[++i]);

		else if ((strcmp(argv[i], "?") == 0) || (strcmp(argv[i], "-h") == 0))
			displayHelp = true;

		else
		{
			// Unknown argument on command line
			fprintf(stderr, "Unknown argument %s\n", argv[i]);
			displayHelp = true;
		}
	}

	if ((deckLinkIndex < 0) || (profileIndex < 0))
		displayHelp = true;

	if (displayHelp)
	{
		fprintf(stderr,
			"\n"
			"Usage: ActivateProfile -d <device id> -p <profile id>\n"
			"\n"
			"    -h/?: help\n"
			"    -d <device id>:\n"
			);
	}

	// Create an IDeckLinkIterator object to enumerate all DeckLink cards in the system
	result = GetDeckLinkIterator(&deckLinkIterator);
	if (result != S_OK)
		goto bail;
	else
	{
		int			idx  = 0;
		IDeckLink*	deckLink = nullptr;

		while (deckLinkIterator->Next(&deckLink) == S_OK)
		{
			dlstring_t deckLinkName;
			std::string deckLinkNameString;

			result = deckLink->GetDisplayName(&deckLinkName);
			if (result == S_OK)
			{
				deckLinkNameString = DlToStdString(deckLinkName);
				DeleteString(deckLinkName);
			}
			else
			{
				fprintf(stderr, "Could not get device name.\n");
				deckLink->Release();
				goto bail;
			}

			// Print display name for each device
			if (displayHelp)
			{
				fprintf(stderr,
					"       %c%2d:  %s\n",
					(idx == deckLinkIndex) ? '*' : ' ',
					idx,
					deckLinkNameString.c_str()
					);
			}

			// Check whether device is selected index, otherwise release
			if (idx++ == deckLinkIndex)
			{
				selectedDeckLink = deckLink;
				selectedDeviceName = deckLinkNameString;
			}
			else
				deckLink->Release();
		}
	}

	if (displayHelp)
	{
		fprintf(stderr,
			"    -p <profile id>:\n"
			);

		if (deckLinkIndex < 0)
		{
			fprintf(stderr,	"         Select device to view available profiles.\n");
			goto bail;

		}
	}

	if (selectedDeckLink == nullptr)
	{
		fprintf(stderr, "Invalid device selected, run with -h for list of devices.\n");
		goto bail;
	}

	// Get Profile manager for selected DeckLink device
	result = selectedDeckLink->QueryInterface(IID_IDeckLinkProfileManager, (void**)&deckLinkProfileManager);
	if (result != S_OK)
	{
		fprintf(stderr, "Unable to query IDeckLinkProfileManager interface, it is likely that the selected device has only 1 profile.\n");
		goto bail;
	}

	// Get Profile Iterator and enumerate the profiles for the device
	result = deckLinkProfileManager->GetProfiles(&deckLinkProfileIterator);
	if ((result != S_OK) || (deckLinkProfileIterator == nullptr))
	{
		fprintf(stderr, "Unable to get IDeckLinkProfileIterator interface.\n");
		goto bail;
	}
	else
	{
		int					idx				= 0;
		IDeckLinkProfile*	deckLinkProfile	= nullptr;

		while (deckLinkProfileIterator->Next(&deckLinkProfile) == S_OK)
		{
			if (displayHelp)
			{
				dlbool_t profileIsActive;
				BMDProfileID profileID;

				// Check whether selected profile is already the active profile
				if (deckLinkProfile->IsActive(&profileIsActive) != S_OK)
					profileIsActive = false;

				// Get the profileID for this profile
				result = GetDeckLinkProfileID(deckLinkProfile, &profileID);
				if (result != S_OK)
				{
					fprintf(stderr, "Unable to get profile identifier.\n");
					deckLinkProfile->Release();
					goto bail;
				}

				fprintf(stderr,
					"       %c%2d:  %s\n",
					profileIsActive ? '*' : ' ',
					idx,
					GetDeckLinkProfileString(profileID).c_str()
					);
			}

			if (idx++ == profileIndex)
				selectedProfile = deckLinkProfile;
			else
			{
				deckLinkProfile->Release();
				deckLinkProfile = nullptr;
			}
		}
	}

	if (displayHelp && (profileIndex < 0))
		goto bail;

	if (selectedProfile == nullptr)
	{
		fprintf(stderr, "Invalid profile selected, run with -h for list of profiles.\n");
		goto bail;
	}

	// Create profile callback object to determine when we have activated requested profile
	profileCallback = new ProfileCallback(selectedProfile);

	// Register IDeckLinkProfileCallback to monitor profile activation
	result = deckLinkProfileManager->SetCallback(profileCallback);
	if (result != S_OK)
	{
		fprintf(stderr, "Unable to register profile callback\n");
		goto bail;
	}
	
	// OK to write new configuration - print configuration
	fprintf(stderr, "Changing profile on device %s...\n", selectedDeviceName.c_str());

	// Activate new profile 
	result = selectedProfile->SetActive();
	if (result == E_ACCESSDENIED)
	{
		fprintf(stderr, "Error: Profile change already in progress\n");
		goto bail;
	}
	else if (result == E_FAIL)
	{
		fprintf(stderr, "Unable to activate profile\n");
		goto bail;
	}
	else
	{
		// Wait until profile change occurs.
		if (!profileCallback->WaitForProfileActivation())
			fprintf(stderr, "Timed out waiting for the new profile to be activated.  Another application may be delaying the profile change.\n");
	}

bail:
	fprintf(stderr, "\n");

	if (selectedProfile != nullptr)
	{
		selectedProfile->Release();
		selectedProfile = nullptr;
	}

	if (deckLinkProfileIterator != nullptr)
	{
		deckLinkProfileIterator->Release();
		deckLinkProfileIterator = nullptr;
	}

	if (profileCallback != nullptr)
	{
		profileCallback->Release();
		profileCallback = nullptr;
	}

	if (deckLinkProfileManager != nullptr)
	{
		deckLinkProfileManager->SetCallback(nullptr);
		deckLinkProfileManager->Release();
		deckLinkProfileManager = nullptr;
	}

	if (selectedDeckLink != nullptr)
	{
		selectedDeckLink->Release();
		selectedDeckLink = nullptr;
	}

	if (deckLinkIterator != nullptr)
	{
		deckLinkIterator->Release();
		deckLinkIterator = nullptr;
	}

	return(result == S_OK) ? 0 : 1;
}

