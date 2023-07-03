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
#include "ColourPalette.h"

const QColor ColourPalette::kColour4			= QColor(241, 241, 241);
const QColor ColourPalette::kColour5			= QColor(146, 146, 146);
const QColor ColourPalette::kColour6		= QColor(40, 40, 40);
const QColor ColourPalette::kColour7		= QColor(26, 26, 26);
const QColor ColourPalette::kColour8		= QColor(23, 23, 23);

const QColor ColourPalette::kColour1			= QColor(33, 33, 38);
const QColor ColourPalette::kColour2			= QColor(9, 9, 9);
const QColor ColourPalette::kColour3		= QColor(40, 40, 46);

QString ColourPalette::toHtmlQString(const QColor& colour)
{
	return QString("#%1%2%3").arg(colour.red(), 2, 16, QChar('0')).arg(colour.green(), 2, 16, QChar('0')).arg(colour.blue(), 2, 16, QChar('0'));
}

QString ColourPalette::toHtmlQString(const QColor& colour, int alpha)
{
	return QString("rgba(%1, %2, %3, %4)").arg(colour.red()).arg(colour.green()).arg(colour.blue()).arg(alpha);
}

QString ColourPalette::processStyleSheet(const char* styleSheet)
{
#define RPL_COL(x) sS.replace(QString(#x), toHtmlQString(x))

	QString sS(styleSheet);

	RPL_COL(kColour4);
	RPL_COL(kColour5);
	RPL_COL(kColour6);
	RPL_COL(kColour7);
	RPL_COL(kColour8);

	RPL_COL(kColour1);
	RPL_COL(kColour2);
	RPL_COL(kColour3);

	return sS;

#undef RPL_COL
}
