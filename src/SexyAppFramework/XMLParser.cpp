#include "XMLParser.h"
#include "Debug.h"
#include "..\PakLib\PakInterface.h"

using namespace Sexy;

XMLParser::XMLParser()
{
	mFile = NULL;
	mLineNum = 0;
	mAllowComments = false;
}

XMLParser::~XMLParser()
{
}

void XMLParser::Fail(const SexyString& theErrorText)
{
	mHasFailed = true;
	mErrorText = theErrorText;
}

void XMLParser::Init()
{
	mSection = _S("");
	mLineNum = 1;
	mHasFailed = false;
	mErrorText = _S("");
}

bool XMLParser::AddAttribute(XMLElement* theElement, const SexyString& theAttributeKey, const SexyString& theAttributeValue)
{
	std::pair<XMLParamMap::iterator,bool> aRet;

	aRet = theElement->mAttributes.insert(XMLParamMap::value_type(theAttributeKey, theAttributeValue));
	if (!aRet.second)
		aRet.first->second = theAttributeValue;

	if (theAttributeKey != _S("/"))
		theElement->mAttributeIteratorList.push_back(aRet.first);

	return aRet.second;
}

bool XMLParser::AddAttributeEncoded(XMLElement* theElement, const SexyString& theAttributeKey, const SexyString& theAttributeValue)
{
	std::pair<XMLParamMap::iterator, bool> aRet;

	aRet = theElement->mAttributesEncoded.insert(XMLParamMap::value_type(theAttributeKey, theAttributeValue));
	if (!aRet.second)
		aRet.first->second = theAttributeValue;

	if (theAttributeKey != _S("/"))
		theElement->mAttributeEncodedIteratorList.push_back(aRet.first);

	return aRet.second;
}

bool XMLParser::OpenFile(const std::string& theFileName)
{		
	if (!EncodingParser::OpenFile(theFileName))
	{
		mLineNum = 0;
		Fail(StringToSexyString("Unable to open file " + theFileName));
		return false;
	}
	
	mFileName = theFileName.c_str();
	Init();
	return true;
}

void XMLParser::SetStringSource(const std::wstring& theString)
{
	Init();

	EncodingParser::SetStringSource(theString);
}

bool XMLParser::NextElement(XMLElement* theElement)
{
	for (;;)
	{		
		theElement->mType = XMLElement::TYPE_NONE;
		theElement->mSection = mSection;
		theElement->mValue = _S("");
		theElement->mAttributes.clear();
		theElement->mAttributesEncoded.clear();
		theElement->mInstruction.erase();
		theElement->mAttributeIteratorList.clear();
		theElement->mAttributeEncodedIteratorList.clear();

		bool hasSpace = false;	
		bool inQuote = false;
		bool gotEndQuote = false;

		bool doingAttribute = false;
		bool AttributeVal = false;
		std::wstring aAttributeKey;
		std::wstring aAttributeValue;

		std::wstring aLastAttributeKey;
		std::wstring aLastAttributeKeyEncoded;
		
		for (;;)
		{
			// Process character by character

			wchar_t c;

			switch (EncodingParser::GetChar(&c))
			{
			case GetCharReturnType::SUCCESSFUL:
				break;

			case GetCharReturnType::INVALID_CHARACTER:
				Fail(_S("Illegal Character"));
				return false;

			case GetCharReturnType::END_OF_FILE:
				if (theElement->mType != XMLElement::TYPE_NONE)
					Fail(_S("Unexpected End of File"));
				return false;

			default:
				Fail(_S("Internal Error"));
				return false;
			}
			
			bool processChar = false;

			if (c == L'\n')
			{
				mLineNum++;
			}

			if (theElement->mType == XMLElement::TYPE_COMMENT)
			{
				// Just add text to theElement->mInstruction until we find -->

				SexyString* aStrPtr = &theElement->mInstruction;

				*aStrPtr += (SexyChar)c;

				int aLen = aStrPtr->length();

				if ((c == L'>') && (aLen >= 3) && ((*aStrPtr)[aLen - 2] == L'-') && ((*aStrPtr)[aLen - 3] == L'-'))
				{
					*aStrPtr = aStrPtr->substr(0, aLen - 3);
					break;
				}
			}
			else if (theElement->mType == XMLElement::TYPE_INSTRUCTION)
			{
				// Just add text to theElement->mInstruction until we find ?>

				SexyString* aStrPtr = &theElement->mValue;

				if ((theElement->mInstruction.length() != 0) || (::iswspace(c)))
					aStrPtr = &theElement->mInstruction;

				*aStrPtr += (SexyChar)c;

				int aLen = aStrPtr->length();

				if ((c == L'>') && (aLen >= 2) && ((*aStrPtr)[aLen - 2] == L'?'))
				{
					*aStrPtr = aStrPtr->substr(0, aLen - 2);
					break;
				}
			}
			else
			{
				if (c == L'"')
				{
					inQuote = !inQuote;
					if (theElement->mType == XMLElement::TYPE_NONE || theElement->mType == XMLElement::TYPE_ELEMENT)
						processChar = true;

					if (!inQuote)
						gotEndQuote = true;
				}
				else if (!inQuote)
				{
					if (c == L'<')
					{
						if (theElement->mType == XMLElement::TYPE_ELEMENT)
						{
							PutChar(c);
							break;
						}

						if (theElement->mType == XMLElement::TYPE_NONE)
						{
							theElement->mType = XMLElement::TYPE_START;
						}
						else
						{
							Fail(_S("Unexpected '<'"));
							return false;
						}
					}
					else if (c == L'>')
					{
						if (theElement->mType == XMLElement::TYPE_START)
						{
							bool insertEnd = false;
						
							if (aAttributeKey == L"/")
							{
								// We will get this if we have a space before the />, so we can ignore it
								//  and go about our business now
								insertEnd = true;
							}
							else
							{
								// Probably isn't committed yet
								if (aAttributeKey.length() > 0)
								{
									aLastAttributeKey = XMLDecodeString(aAttributeKey);
									aLastAttributeKeyEncoded = aAttributeKey;

									AddAttribute(theElement, WStringToSexyString(aLastAttributeKey), WStringToSexyString(XMLDecodeString(aAttributeValue)));
									AddAttributeEncoded(theElement, WStringToSexyString(aLastAttributeKey), WStringToSexyString(aAttributeValue));
						
									aAttributeKey = L"";
									aAttributeValue = L"";
								}
								
								if (aLastAttributeKey.length() > 0)
								{
									SexyString aVal = theElement->mAttributes[WStringToSexyString(aLastAttributeKey)];
									int aLen = aVal.length();
									
									if ((aLen > 0) && (aVal[aLen - 1] == '/'))
									{
										// Its an empty element, fake start and end segments
										AddAttribute(theElement, WStringToSexyString(aLastAttributeKey), XMLDecodeString(aVal.substr(0, aLen - 1)));
										insertEnd = true;
									}

									aVal = theElement->mAttributesEncoded[WStringToSexyString(aLastAttributeKeyEncoded)];
									aLen = aVal.length();

									if ((aLen > 0) && (aVal[aLen - 1] == '/'))
									{
										AddAttributeEncoded(theElement, WStringToSexyString(aLastAttributeKeyEncoded), aVal.substr(0, aLen - 1));
										insertEnd = true;
									}
								}
								else
								{
									int aLen = theElement->mValue.length();
									
									if ((aLen > 0) && (theElement->mValue[aLen - 1] == '/'))
									{
										// Its an empty element, fake start and end segments
										theElement->mValue = theElement->mValue.substr(0, aLen - 1);
										insertEnd = true;
									}
								}
							}
						
							// Do we want to fake an ending section?
							if (insertEnd)
							{
								SexyString anAddString = _S("</") + theElement->mValue + _S(">");
								PutString(SexyStringToWStringFast(anAddString));

								// clear out aAttributeKey, since it contains "/" as its value and will insert
								// it into the element's attribute map.
								aAttributeKey = L"";
							}
						
							if (mSection.length() != 0)
								mSection += _S("/");
							
							mSection += theElement->mValue;
							
							break;
						}
						else if (theElement->mType == XMLElement::TYPE_END)
						{
							int aLastSlash = mSection.rfind(_S('/'));
							if ((aLastSlash == -1) && (mSection.length() == 0))
							{
								Fail(_S("Unexpected End"));
								return false;
							}

							SexyString aLastSectionName = mSection.substr(aLastSlash + 1);

							if (aLastSectionName != theElement->mValue)
							{
								Fail(_S("End '") + theElement->mValue + _S("' Doesn't Match Start '") + aLastSectionName + _S("'"));
								return false;
							}

							if (aLastSlash == -1)
								mSection.erase(mSection.begin(), mSection.end());
							else
								mSection.erase(mSection.begin() + aLastSlash, mSection.end());

							break;
						}
						else
						{
							Fail(_S("Unexpected '>'"));
							return false;
						}
					}
					else if ((c == L'/') && (theElement->mType == XMLElement::TYPE_START) && (theElement->mValue == _S("")))
					{
						theElement->mType = XMLElement::TYPE_END;
					}
					else if ((c == L'?') && (theElement->mType == XMLElement::TYPE_START) && (theElement->mValue == _S("")))
					{
						theElement->mType = XMLElement::TYPE_INSTRUCTION;
					}
					else if (::iswspace((wint_t)c))
					{
						if (theElement->mValue != _S(""))
							hasSpace = true;

						// It's a comment!
						if ((theElement->mType == XMLElement::TYPE_START) && (theElement->mValue == _S("!--")))
							theElement->mType = XMLElement::TYPE_COMMENT;
					}
					else if (c > 32)
					{
						processChar = true;
					}
					else
					{
						Fail(_S("Illegal Character"));
						return false;
					}
				}
				else
				{
					processChar = true;
				}

				if (processChar)
				{
					if (theElement->mType == XMLElement::TYPE_NONE)
						theElement->mType = XMLElement::TYPE_ELEMENT;

					if (theElement->mType == XMLElement::TYPE_START)
					{
						if (hasSpace)
						{
							if ((!doingAttribute) || ((!AttributeVal) && (c != _S('='))) ||
								((AttributeVal) && ((aAttributeValue.length() > 0) || gotEndQuote)))
							{
								if (doingAttribute)
								{
									AddAttribute(theElement, WStringToSexyString(XMLDecodeString(aAttributeKey)), WStringToSexyString(XMLDecodeString(aAttributeValue)));
									AddAttributeEncoded(theElement, WStringToSexyString(aAttributeKey), WStringToSexyString(aAttributeValue));

									aAttributeKey = L"";
									aAttributeValue = L"";

									aLastAttributeKey = aAttributeKey;
									aLastAttributeKeyEncoded = aLastAttributeKey;
								}
								else
								{
									doingAttribute = true;
								}

								AttributeVal = false;
							}

							hasSpace = false;
						}

						std::wstring* aStrPtr = NULL;

						if (!doingAttribute)
						{
							theElement->mValue += (SexyChar)c;
						}
						else
						{
							if (c == L'=')
							{
								AttributeVal = true;
								gotEndQuote = false;
							}
							else
							{
								if (!AttributeVal)
									aStrPtr = &aAttributeKey;
								else
									aStrPtr = &aAttributeValue;
							}
						}

						if (aStrPtr != NULL)
						{
							*aStrPtr += c;
						}
					}
					else
					{
						if (hasSpace)
						{
							theElement->mValue += _S(" ");
							hasSpace = false;
						}

						theElement->mValue += (SexyChar)c;
					}
				}
			}
		}		

		if (aAttributeKey.length() > 0)
		{
			AddAttribute(theElement, WStringToSexyString(XMLDecodeString(aAttributeKey)), WStringToSexyString(XMLDecodeString(aAttributeValue)));
			AddAttribute(theElement, WStringToSexyString(aAttributeKey), WStringToSexyString(aAttributeValue));
		}

		theElement->mValueEncoded = theElement->mValue;
		theElement->mValue = XMLDecodeString(theElement->mValue);				

		// Ignore comments
		if ((theElement->mType != XMLElement::TYPE_COMMENT) || mAllowComments)
			return true;
	}
}

bool XMLParser::HasFailed()
{
	return mHasFailed;
}

SexyString XMLParser::GetErrorText()
{
	return mErrorText;
}

int XMLParser::GetCurrentLineNum()
{
	return mLineNum;
}

std::string XMLParser::GetFileName()
{
	return mFileName;
}
