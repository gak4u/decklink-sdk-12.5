/* -LICENSE-START-
 ** Copyright (c) 2015 Blackmagic Design
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
#pragma once

#include "ControllerImp.h"
#include "VideoWriter.h"

#include "DeckLinkAPI.h"

#include <vector>
#include <string>
#include <QAtomicInt>

class DeckLinkDevice : public IDeckLinkEncoderInputCallback
{
public:
	DeckLinkDevice(ControllerImp* uiDelegate, IDeckLink* device);
	virtual ~DeckLinkDevice();
	
	bool						init();
	
	//
	// IDeckLinkInputCallback interface

	virtual HRESULT				VideoInputSignalChanged (/* in */ BMDVideoInputFormatChangedEvents notificationEvents, /* in */ IDeckLinkDisplayMode *newDisplayMode, /* in */ BMDDetectedVideoInputFormatFlags detectedSignalFlags);
	virtual HRESULT				VideoPacketArrived (/* in */ IDeckLinkEncoderVideoPacket* videoPacket);
	virtual HRESULT				AudioPacketArrived (/* in */ IDeckLinkEncoderAudioPacket* audioPacket);

	// IUnknown needs only a dummy implementation
	virtual HRESULT				QueryInterface (REFIID iid, LPVOID *ppv);
	virtual ULONG				AddRef();
	virtual ULONG				Release();
	
	QString						getDeviceName() { return !m_deviceName.isEmpty() ? m_deviceName : "Unknown device"; }
	uint32_t					getDisplayModeFrameRate(int displayModeIndex);
	QString						getDisplayModeName(int displayModeIndex);

	bool						isCapturing() { return m_currentlyCapturing; }
	bool						startCapture(int videoModeIndex, uint32_t bitrate) ;
	void						stopCapture(bool deleteFile = false);
	
	int							defaultMode() { return m_defaultMode; }

	// public members
	IDeckLink*					m_deckLink;
	IDeckLinkEncoderInput*		m_deckLinkEncoderInput;
	IDeckLinkEncoderConfiguration*	m_deckLinkEncoderConfiguration;

	VideoWriter*				m_videoWriter;

	bool						m_currentlyCapturing;

	
private:

	ControllerImp*			m_uiDelegate;
	std::vector<IDeckLinkDisplayMode*>	m_modeList;
	int					m_defaultMode;
	QString				m_deviceName;
	QAtomicInt			m_refCount;
};

class DeckLinkDeviceDiscovery :  public IDeckLinkDeviceNotificationCallback
{
private:
	ControllerImp*		m_uiDelegate;
	IDeckLinkDiscovery*	deckLinkDiscovery;
	QAtomicInt			m_refCount;
public:
	DeckLinkDeviceDiscovery(ControllerImp* uiDelegate);
	virtual ~DeckLinkDeviceDiscovery();
	
	bool			Enable();
	void			Disable();
			
	// IDeckLinkDeviceArrivalNotificationCallback interface
    virtual HRESULT		DeckLinkDeviceArrived (/* in */ IDeckLink* deckLinkDevice);
	virtual HRESULT		DeckLinkDeviceRemoved (/* in */ IDeckLink* deckLinkDevice);
	
	// IUnknown needs only a dummy implementation
	virtual HRESULT		QueryInterface (REFIID iid, LPVOID *ppv);
	virtual ULONG		AddRef ();
	virtual ULONG		Release ();
};
