#ifndef __TYPINGCHECK_H__
#define __TYPINGCHECK_H__

#include "../SexyAppFramework/KeyCodes.h"

class TypingCheck
{
protected:
	std::string		mPhrase;
	std::string		mRecentTyping;

public:
	TypingCheck();
	TypingCheck(const std::string& thePhrase);

	void			SetPhrase(const std::string& thePhrase);
	void			AddKeyCode(Sexy::KeyCode theKeyCode);
	void			AddChar(char theChar);
	bool			Check();
	bool			Check(Sexy::KeyCode theKeyCode);
};

#endif
