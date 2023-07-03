/* -LICENSE-START-
** Copyright (c) 2020 Blackmagic Design
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

#include <atomic>
#include <functional>

#include <QObject>
#include <QString>

#include "com_ptr.h"
#include "DeckLinkAPI.h"

class DeckLinkOutputDevice : public IDeckLinkVideoOutputCallback, public IDeckLinkAudioOutputCallback
{
	using ScheduledFrameCompletedFunc	= std::function<void(void)>;
	using RenderAudioSamplesFunc		= std::function<void(void)>;
	using ScheduledPlaybackStoppedFunc	= std::function<void(void)>; 
	using DisplayModeQueryFunc			= std::function<void(com_ptr<IDeckLinkDisplayMode>&)>;

public:
	DeckLinkOutputDevice(QObject* owner, com_ptr<IDeckLink>& deckLink);
	virtual ~DeckLinkOutputDevice() = default;

	// IUnknown
	HRESULT		QueryInterface(REFIID iid, LPVOID *ppv) override;
	ULONG		AddRef() override;
	ULONG		Release() override;

	// IDeckLinkVideoOutputCallback
	HRESULT		ScheduledFrameCompleted(IDeckLinkVideoFrame* completedFrame, BMDOutputFrameCompletionResult result) override;
	HRESULT		ScheduledPlaybackHasStopped() override;

	// IDeckLinkAudioOutputCallback
	HRESULT		RenderAudioSamples(bool preroll) override;

	// Other methods
	const QString						getDeviceName() const { return m_deviceName; }
	com_ptr<IDeckLinkOutput>			getDeviceOutput() const { return m_deckLinkOutput; }
	com_ptr<IDeckLink>					getDeckLinkInstance() const { return m_deckLink; }
	com_ptr<IDeckLinkConfiguration>		getDeviceConfiguration() const { return m_deckLinkConfiguration; }
	com_ptr<IDeckLinkProfileManager>	getProfileManager() const { return m_deckLinkProfileManager; }

	void	queryDisplayModes(DisplayModeQueryFunc func);
	void	onScheduledFrameCompleted(const ScheduledFrameCompletedFunc& callback) { m_scheduledFrameCompletedCallback = callback; }
	void	onRenderAudioSamples(const RenderAudioSamplesFunc& callback) { m_renderAudioSamplesCallback = callback; }
	void	onScheduledPlaybackStopped(const ScheduledPlaybackStoppedFunc& callback) { m_scheduledPlaybackStoppedCallback = callback; }

private:
	std::atomic<ULONG>					m_refCount;
	QObject*							m_owner;

	com_ptr<IDeckLink>					m_deckLink;
	com_ptr<IDeckLinkOutput>			m_deckLinkOutput;
	com_ptr<IDeckLinkConfiguration>		m_deckLinkConfiguration;
	com_ptr<IDeckLinkProfileManager>	m_deckLinkProfileManager;
	QString								m_deviceName;

	ScheduledFrameCompletedFunc			m_scheduledFrameCompletedCallback;
	ScheduledPlaybackStoppedFunc		m_scheduledPlaybackStoppedCallback;
	RenderAudioSamplesFunc				m_renderAudioSamplesCallback;
};

