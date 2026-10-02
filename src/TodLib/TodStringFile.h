#ifndef __TODSTRINGFILE_H__
#define __TODSTRINGFILE_H__

#include "../SexyAppFramework/Rect.h"
#include "../SexyAppFramework/Color.h"

namespace Sexy
{
	class Font;
	class Graphics;
}

enum TodStringFormatFlag
{
	TOD_FORMAT_IGNORE_NEWLINES,
	TOD_FORMAT_HIDE_UNTIL_MAGNETSHROOM
};

class TodStringListFormat
{
public:
	const SexyChar*			mFormatName;
	Sexy::Font**			mNewFont;
	Sexy::Color				mNewColor;
	int						mLineSpacingOffset;
	unsigned int			mFormatFlags;

public:
	TodStringListFormat();
	TodStringListFormat(const SexyChar* theFormatName, Sexy::Font** theFont, const Sexy::Color& theColor, int theLineSpacingOffset, unsigned int theFormatFlags);
};
extern int gTodStringFormatCount;				//[0x69DE4C]
extern TodStringListFormat* gTodStringFormats;	//[0x69DA34]
extern TodStringListFormat gTodDefaultStringFormats[2];  //0x6A5190

void						TodStringListSetColors(TodStringListFormat* theFormats, int theCount);
void						TodWriteStringSetFormat(const SexyChar* theFormat, TodStringListFormat& theCurrentFormat);
bool						TodStringListReadName(const SexyChar*& thePtr, std::string& theName);
bool						TodStringListReadValue(const SexyChar*& thePtr, SexyString& theValue);
bool						TodStringListReadItems(const SexyChar* theFileText);
bool						TodStringListReadFile(const char* theFileName);
void						TodStringListLoad(const char* theFileName);
SexyString					TodStringListFind(const SexyString& theName);
SexyString					TodStringTranslate(const SexyString& theString);
SexyString					TodStringTranslate(const SexyChar* theString);
bool						TodStringListExists(const SexyString& theString);
void						TodStringRemoveReturnChars(SexyString& theString);
bool						CharIsSpaceInFormat(SexyChar theChar, const TodStringListFormat& theCurrentFormat);
int							TodWriteString(Sexy::Graphics* g, const SexyString& theString, int theX, int theY, TodStringListFormat& theCurrentFormat, int theWidth, DrawStringJustification theJustification, bool drawString, int theOffset, int theLength);
int							TodWriteWordWrappedHelper(Sexy::Graphics* g, const SexyString& theString, int theX, int theY, TodStringListFormat& theCurrentFormat, int theWidth, DrawStringJustification theJustification, bool drawString, int theOffset, int theLength, int theMaxChars);
int							TodDrawStringWrappedHelper(Sexy::Graphics* g, const SexyString& theText, const Sexy::Rect& theRect, Sexy::Font* theFont, const Sexy::Color& theColor, DrawStringJustification theJustification, bool drawString);
void						TodDrawStringWrapped(Sexy::Graphics* g, const SexyString& theText, const Sexy::Rect& theRect, Sexy::Font* theFont, const Sexy::Color& theColor, DrawStringJustification theJustification);

#endif  //__TODSTRINGFILE_H__