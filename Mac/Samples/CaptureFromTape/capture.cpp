/* -LICENSE-START-
 ** Copyright (c) 2010 Blackmagic Design
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
#include "capture.h"
#include "common.h"
#include <libkern/OSAtomic.h>


// settings for NTSC 29.97 - UYVY pixel format
#define BMD_DISPLAYMODE			bmdModeNTSC
#define TCISDROPFRAME			true
#define PIXEL_FMT				bmdFormat8BitYUV

// in and out points						H H :M M :S S :F F    
#define START_TC				MAKE_TC_BCD(0,1, 0,0, 0,0, 0,0)
#define STOP_TC					MAKE_TC_BCD(0,1, 0,0, 0,5, 0,0)

// capture offset (in frames)
#define CAPTURE_OFFSET			0

// Prepare IDeckLinkInput to capture video:
// - Get frame width, height, duration, ...
// - Setup callback object and video mode and
// - Start streams
bool	CaptureHelper::setupDeckLinkInput()
{
	bool							result = false;
    IDeckLinkDisplayModeIterator*	displayModeIterator = NULL;
    IDeckLinkDisplayMode*			deckLinkDisplayMode = NULL;
	
	m_width = -1;
	
	// get frame scale and duration for the video mode
    if (m_deckLinkInput->GetDisplayModeIterator(&displayModeIterator) != S_OK)
		goto bail;
	
    while (displayModeIterator->Next(&deckLinkDisplayMode) == S_OK)
    {
		if (deckLinkDisplayMode->GetDisplayMode() == BMD_DISPLAYMODE)
		{
			m_width = deckLinkDisplayMode->GetWidth();
			m_height = deckLinkDisplayMode->GetHeight();
			deckLinkDisplayMode->GetFrameRate(&m_frameDuration, &m_timeScale);
			deckLinkDisplayMode->Release();
			deckLinkDisplayMode = NULL;
			
			break;
		}
		
		deckLinkDisplayMode->Release();
		deckLinkDisplayMode = NULL;
    }
	
	displayModeIterator->Release();
	displayModeIterator = NULL;
	
	if (m_width == -1)
	{
		printf("Unable to find requested video mode\n");
		goto bail;
	}
	
	// convert the in- and out-point timecodes to a frame count
	GET_FRAME_COUNT(m_inPointFrameCount, START_TC, m_timeScale, m_frameDuration);
	GET_FRAME_COUNT(m_outPointFrameCount, STOP_TC, m_timeScale, m_frameDuration);
	
	// set callback
	m_deckLinkInput->SetCallback(this);
	
	// enable video input
	if (m_deckLinkInput->EnableVideoInput(BMD_DISPLAYMODE, PIXEL_FMT, bmdVideoInputFlagDefault) != S_OK)
	{
		printf("Could not enable video input\n");
		goto bail;
	}
	
	// start streaming
	if (m_deckLinkInput->StartStreams() != S_OK)
	{
		printf("Could not start streams\n");
		goto bail;
	}

	
	result = true;
	
bail:
	return result;
}

// Prepare IDeckLinkDeckControl to capture
bool	CaptureHelper::setupDeckControl()
{
	BMDDeckControlError err;
	bool result = false;
	
	// set callback, preroll and offset
	m_deckControl->SetCallback(this);
	m_deckControl->SetPreroll(2);
	m_deckControl->SetCaptureOffset(CAPTURE_OFFSET*2); // offset must be given in fields (not frames)
	
	pthread_mutex_lock(&m_mutex);
		// open connection to deck
		if (m_deckControl->Open(m_timeScale, m_frameDuration, TCISDROPFRAME, &err) == S_OK)
		{
			// wait for a deck to be connected
			m_waitingForDeckConnected = true;
			pthread_cond_wait(&m_condition, &m_mutex);
			m_waitingForDeckConnected = false;
			result = true;
		}
		else
			printf("Could not open serial port (%s)\n", ERR_TO_STR(err));
	pthread_mutex_unlock(&m_mutex);

	return result;
	
}

// Close IDeckLinkDeckControl
void	CaptureHelper::cleanupDeckControl()
{
	m_deckControl->Close(false);
	m_deckControl->SetCallback(NULL);
}

// Stop video input
void	CaptureHelper::cleanupDeckLinkInput()
{
	m_deckLinkInput->StopStreams();
	m_deckLinkInput->DisableVideoInput();
	m_deckLinkInput->SetCallback(NULL);
}

// Start a capture operation
void	CaptureHelper::doCapture()
{
	BMDDeckControlError err;
	
	// setup DeckLink Input interface
	if (! setupDeckLinkInput())
		goto bail;
	
	// setup DeckControl interface
	if (! setupDeckControl())
		goto bail;

	pthread_mutex_lock(&m_mutex);
		// start capture
		if (m_deckControl->StartCapture(true, START_TC, STOP_TC, &err) == S_OK)
		{
			// wait for capture to finish or error to occur
			m_waitingForCaptureEnd = true;
			pthread_cond_wait(&m_condition, &m_mutex);
			m_waitingForCaptureEnd = false;
		}
		else
			printf("Could not start capture (%s)\n", ERR_TO_STR(err));
	pthread_mutex_unlock(&m_mutex);
	
bail:
	cleanupDeckControl();
	cleanupDeckLinkInput();
}

#pragma mark IDeckLinkDeckControlStatusCallback methods
/*
 * IDeckLinkDeckControlStatusCallback methods
 */
HRESULT	CaptureHelper::TimecodeUpdate (/* in */ BMDTimecodeBCD currentTimecode)
{
	// Timecodes are NOT reported during capture !
	// To find out whether we have reached the in/out point, use the serial 
	// timecode attached to each video frame (see VideoInputFrameArrived() )
	return S_OK;
}

HRESULT	CaptureHelper::DeckControlEventReceived (/* in */ BMDDeckControlEvent event, /* in */ BMDDeckControlError err)
{
	printf("\n === '%s' event (%s)\n", EVT_TO_STR(event), ERR_TO_STR(err));
	
	switch (event){
		case bmdDeckControlPrepareForCaptureEvent:
			// We receive this event a few frames before we reach the in-point. The serial timecode
			// attached to IDeckLinkVideoFrames (received in VideoInputFrameArrived) is now
			// valid and can be used to find the in- and out-points.
			m_captureStarted = true;
			break;
		case bmdDeckControlCaptureCompleteEvent:
			// we receive this event a few frames after the out-point.
			// It is now safe to unblock the main thread, close the 
			// connection to the deck and release the IDeckLinkDeckControl 
			// and IDeckLinkInput interfaces

			// fallthrough
			
		case bmdDeckControlAbortedEvent:
			m_captureStarted = false;
			
			// unblock the main thread
			pthread_mutex_lock(&m_mutex);
				if (m_waitingForCaptureEnd)
					pthread_cond_signal(&m_condition);
			pthread_mutex_unlock(&m_mutex);
			break;
	}
	
	return S_OK;
	
}

HRESULT	CaptureHelper::VTRControlStateChanged (/* in */ BMDDeckControlVTRControlState newState, /* in */ BMDDeckControlError error)
{
	// this method is called only when in VTRControl mode (not capture/export mode).	
	return S_OK;
}

HRESULT	CaptureHelper::DeckControlStatusChanged (/* in */ BMDDeckControlStatusFlags flags, /* in */ uint32_t mask)
{
	
	printf("\n ===  State change callback:\n");
	printf("New status: %s - %s - %s - %s\n", FLAGS_TO_STRS(flags));
	
	// unblock the main thread if waiting for a deck to be connected
	pthread_mutex_lock(&m_mutex);
	if ((m_waitingForDeckConnected) && (mask & bmdDeckControlStatusDeckConnected) && (flags & bmdDeckControlStatusDeckConnected))
		pthread_cond_signal(&m_condition);
	pthread_mutex_unlock(&m_mutex);
	
	return S_OK;
}

#pragma mark IDeckLinkInputCallback methods
/*
 *IDeckLinkInputCallback methods
 */
HRESULT	CaptureHelper::VideoInputFrameArrived (IDeckLinkVideoInputFrame* arrivedFrame, IDeckLinkAudioInputPacket*)
{
	IDeckLinkTimecode *timecode = NULL;
	
	// check the serial timecode only when we were told the capture is about to start (bmdDeckControlPrepareForCaptureEvent)
	if (m_captureStarted && (arrivedFrame->GetTimecode(bmdTimecodeSerial, &timecode) == S_OK))
	{
		BMDTimecodeBCD tcBCD;
		uint32_t frameCount;

		// get the BCD timecode	and convert it to a frame count
		// so we can easily add the capture offset and make comparisons
		tcBCD = timecode->GetBCD();
		GET_FRAME_COUNT(frameCount, tcBCD, m_timeScale, m_frameDuration);

		// check if the timecode is between the in- and out-point.
		// note that the comparison MUST take into account the capture offset, as set using IDeckLinkDeckControl::SetCaptureOffset();
		if ( (frameCount >= (m_inPointFrameCount + CAPTURE_OFFSET)) && (frameCount <= (m_outPointFrameCount + CAPTURE_OFFSET)) )
		{
			// this frame is within the in-and out-points, do something useful with it
			uint8_t hours, minutes, seconds, frames;
			timecode->GetComponents(&hours, &minutes, &seconds, &frames);		
			printf("New frame TC: %02hhu:%02hhu:%02hhu:%02hhu\n", hours, minutes, seconds, frames);
		}
		
		timecode->Release();
		timecode = NULL;
	}
	
	return S_OK;
}

HRESULT	CaptureHelper::QueryInterface (REFIID iid, LPVOID *ppv)
{
	*ppv = NULL;
	return E_NOINTERFACE;
}

ULONG	CaptureHelper::AddRef ()
{
	return ++m_refCount;
}

ULONG	CaptureHelper::Release ()
{
	ULONG		newRefValue;
	
	newRefValue = --m_refCount;
	if (newRefValue == 0)
	{
		delete this;
		return 0;
	}
	
	return newRefValue;
}

#pragma mark CTOR / DTOR
/*
 * CTOR / DTOR
 */
CaptureHelper::CaptureHelper(IDeckLink *deckLink)
	: m_refCount(1), m_deckLink(deckLink), m_deckControl(NULL), m_deckLinkInput(NULL)
	, m_waitingForDeckConnected(false), m_waitingForCaptureEnd(false), m_captureStarted(false)
	, m_width(-1), m_height(-1), m_timeScale(0), m_frameDuration(0), m_inPointFrameCount(0)
	, m_outPointFrameCount(0)
{
	// create mutex and condition variable
	m_deckLink->AddRef();
	pthread_mutex_init(&m_mutex, NULL);
	pthread_cond_init(&m_condition, NULL);
}

CaptureHelper::~CaptureHelper()
{
	// release IDeckLinkInput and IDeckLinkDeckControl if required
	if (m_deckControl)
	{
		m_deckControl->Release();
		m_deckControl = NULL;
	}
	
	if (m_deckLinkInput)
	{
		m_deckLinkInput->Release();
		m_deckLinkInput = NULL;
	}
	
	pthread_cond_destroy(&m_condition);
	pthread_mutex_destroy(&m_mutex);
	
	if (m_deckLink)
	{
		m_deckLink->Release();
		m_deckLink = NULL;
	}
}

// Call init() before any other method. if init() fails, destroy the object
bool	CaptureHelper::init()
{
	bool result = false;
	
	// get IDeckLinkInput and IDeckLinkDeckControl
	if (m_deckLink->QueryInterface(IID_IDeckLinkInput, (void **)&m_deckLinkInput) != S_OK)
	{
		printf("Could not obtain the DeckLink Input interface\n");
		goto bail;
	}

	if (m_deckLink->QueryInterface(IID_IDeckLinkDeckControl, (void **)&m_deckControl) != S_OK)
	{
		printf("Could not obtain the DeckControl interface\n");
		goto bail;
	}
	
	result = true;
bail:
	return result;
}
