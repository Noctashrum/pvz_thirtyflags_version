#include "TodFoley.h"
#include "TodDebug.h"
#include "TodCommon.h"
#include "../SexyAppFramework/SoundManager.h"

using namespace Sexy;

int gFoleyParamArraySize;        //[0x6A9F04]
FoleyParams* gFoleyParamArray;   //[0x6A9F00]

FoleyInstance::FoleyInstance()
{
	mInstance = nullptr;
	mRefCount = 0;
	mPaused = false;
	mStartTime = 0;
	mPauseOffset = 0;
}

//0x514ED0
FoleyTypeData::FoleyTypeData()
{
	mLastVariationPlayed = -1;
}

int TodDSoundInstance::GetSoundPosition()
{
	unsigned long dwCurrentPlayerCursor;
	mSoundBuffer->GetCurrentPosition(&dwCurrentPlayerCursor, nullptr);
	return dwCurrentPlayerCursor;
}

void TodDSoundInstance::SetSoundPosition(int thePosition)
{
	mSoundBuffer->SetCurrentPosition(thePosition);
}

void TodFoleyInitialize(FoleyParams* theFoleyParamArray, int theFoleyParamArraySize)
{
	TOD_ASSERT(gFoleyParamArray == nullptr && gFoleyParamArraySize == 0);
	gFoleyParamArray = theFoleyParamArray;
	gFoleyParamArraySize = theFoleyParamArraySize;
}

void TodFoleyDispose()
{
	gFoleyParamArray = nullptr;
	gFoleyParamArraySize = 0;
}

//0x514F70
void SoundSystemReleaseFinishedInstances(TodFoley* theSoundSystem)
{
	for (int aFoleyType = 0; aFoleyType < gFoleyParamArraySize; aFoleyType++)
		for (int i = 0; i < MAX_FOLEY_INSTANCES; i++)
		{
			FoleyInstance* aFoleyInstance = &theSoundSystem->mFoleyTypeData[aFoleyType].mFoleyInstances[i];
			if (aFoleyInstance->mRefCount == 0)
			{
				TOD_ASSERT(aFoleyInstance->mInstance == nullptr);
			}
			else if (!aFoleyInstance->mPaused)
			{
				TOD_ASSERT(aFoleyInstance->mInstance);
				if (!aFoleyInstance->mInstance->IsPlaying())
				{
					aFoleyInstance->mInstance->Release();
					aFoleyInstance->mInstance = nullptr;
					aFoleyInstance->mRefCount = 0;
				}
			}
		}
}

//0x514FE0
bool SoundSystemHasFoleyPlayedTooRecently(TodFoley* theSoundSystem, FoleyType theFoleyType)
{
	FoleyTypeData* aFoleyData = &theSoundSystem->mFoleyTypeData[(int)theFoleyType];
	for (int i = 0; i < MAX_FOLEY_INSTANCES; i++)
	{
		FoleyInstance* aFoleyInstance = &aFoleyData->mFoleyInstances[i];
		if (aFoleyInstance->mRefCount != 0 && gSexyAppBase->mUpdateCount - aFoleyInstance->mStartTime < 10)  // 若同种音效存在近 10 cs 内播放的实例
			return true;
	}
	return false;
}

FoleyParams* LookupFoley(FoleyType theFoleyType)
{
	TOD_ASSERT(theFoleyType >= 0 && theFoleyType < gFoleyParamArraySize);
	TOD_ASSERT(gFoleyParamArraySize < MAX_FOLEY_TYPES);
	FoleyParams* aFoleyParams = &gFoleyParamArray[(int)theFoleyType];
	TOD_ASSERT(aFoleyParams->mFoleyType == theFoleyType);
	return aFoleyParams;
}

FoleyInstance* SoundSystemFindInstance(TodFoley* theSoundSystem, FoleyType theFoleyType)
{
	FoleyTypeData* aFoleyData = &theSoundSystem->mFoleyTypeData[(int)theFoleyType];
	for (int i = 0; i < MAX_FOLEY_INSTANCES; i++)
	{
		FoleyInstance* aFoleyInstance = &aFoleyData->mFoleyInstances[i];
		if (aFoleyInstance->mRefCount > 0)
		{
			TOD_ASSERT(aFoleyInstance->mInstance);
			return aFoleyInstance;
		}
	}
	return nullptr;
}

FoleyInstance* SoundSystemGetFreeInstanceIndex(TodFoley* theSoundSystem, FoleyType theFoleyType)
{
	FoleyTypeData* aFoleyData = &theSoundSystem->mFoleyTypeData[(int)theFoleyType];
	for (int i = 0; i < MAX_FOLEY_INSTANCES; i++)
	{
		FoleyInstance* aFoleyInstance = &aFoleyData->mFoleyInstances[i];
		if (aFoleyInstance->mRefCount == 0)
		{
			TOD_ASSERT(aFoleyInstance->mInstance == nullptr);
			return aFoleyInstance;
		}
	}
	return nullptr;
}

//0x515020
void TodFoley::PlayFoleyPitch(FoleyType theFoleyType, float thePitch)
{
	FoleyParams* aFoleyParams = LookupFoley(theFoleyType);
	SoundSystemReleaseFinishedInstances(this);  // 释放已播放完成的音效实例
	if (SoundSystemHasFoleyPlayedTooRecently(this, theFoleyType) && !TestBit(aFoleyParams->mFoleyFlags, FoleyFlags::FOLEYFLAGS_LOOP))
		return;  // 非循环音效不可重叠播放

	if (TestBit(aFoleyParams->mFoleyFlags, FoleyFlags::FOLEYFLAGS_ONE_AT_A_TIME))  // 如果定义了不可叠加播放
	{
		FoleyInstance* aFoleyInstance = SoundSystemFindInstance(this, theFoleyType);
		if (aFoleyInstance != nullptr)
		{
			aFoleyInstance->mRefCount++;  // 增加 1 次引用计数
			aFoleyInstance->mStartTime = gSexyAppBase->mUpdateCount;  // 刷新开始的时间
			return;
		}
	}
	FoleyInstance* aFoleyInstance = SoundSystemGetFreeInstanceIndex(this, theFoleyType);
	if (aFoleyInstance == nullptr)  // 如果已经存在 8 个音效实例
		return;

	int aVariations = 0;
	int aVariationsArray[10];
	FoleyTypeData* aFoleyData = &mFoleyTypeData[(int)theFoleyType];
	for (int i = 0; i < 10; i++)
	{
		if (!TestBit(aFoleyParams->mFoleyFlags, FoleyFlags::FOLEYFLAGS_DONT_REPEAT) || aFoleyData->mLastVariationPlayed != i)  // 如果未重复或不禁止重复
		{
			if (aFoleyParams->mSfxID[i] == nullptr)
				break;
			aVariationsArray[aVariations] = i;
			aVariations++;
		}
	}
	TOD_ASSERT(aVariations > 0);
	int aVariation = TodPickFromArray(aVariationsArray, aVariations);
	aFoleyData->mLastVariationPlayed = aVariation;
	SoundInstance* aSoundInstance = gSexyAppBase->mSoundManager->GetSoundInstance(*aFoleyParams->mSfxID[aVariation]);
	if (aSoundInstance == nullptr)
		return;

	aFoleyInstance->mInstance = aSoundInstance;
	aFoleyInstance->mRefCount = 1;
	aFoleyInstance->mStartTime = gSexyAppBase->mUpdateCount;
	aFoleyData->mLastVariationPlayed = aVariation;
	if (thePitch != 0.0f)  // 如果参数指定了音高
		aSoundInstance->AdjustPitch(thePitch);  // 调整音高
	if (TestBit(aFoleyParams->mFoleyFlags, FoleyFlags::FOLEYFLAGS_USES_MUSIC_VOLUME))  // 如果定义了使用音乐音量
		ApplyMusicVolume(aFoleyInstance);  // 将音效的音量调整为与音乐一致
	bool aIsLooping = TestBit(aFoleyParams->mFoleyFlags, FoleyFlags::FOLEYFLAGS_LOOP);
	aSoundInstance->Play(aIsLooping, false);  // 正式开始播放音效
}

//0x515240
void TodFoley::PlayFoley(FoleyType theFoleyType)
{
	FoleyParams* aFoleyParams = LookupFoley(theFoleyType);
	float aPitch = 0.0f;
	if (aFoleyParams->mPitchRange != 0.0f)  // 如果定义了音高范围
		aPitch = Sexy::Rand(aFoleyParams->mPitchRange);  // 在范围内随机选取一个音高
	PlayFoleyPitch(theFoleyType, aPitch);
}

//0x515290
void TodFoley::StopFoley(FoleyType theFoleyType)
{
	SoundSystemReleaseFinishedInstances(this);
	FoleyInstance* aFoleyInstance = SoundSystemFindInstance(this, theFoleyType);
	if (aFoleyInstance == nullptr)
		return;

	TOD_ASSERT(aFoleyInstance->mRefCount > 0);
	TOD_ASSERT(aFoleyInstance->mInstance);
	aFoleyInstance->mRefCount--;  // 减少 1 次引用计数
	if (aFoleyInstance->mRefCount == 0)  // 如果减少之后无引用，则直接释放
	{
		aFoleyInstance->mInstance->Release();
		aFoleyInstance->mInstance = nullptr;
	}
}

//0x5152D0
void TodFoley::GamePause(bool theEnteringPause)
{
	SoundSystemReleaseFinishedInstances(this);
	for (int aFoleyType = 0; aFoleyType < gFoleyParamArraySize; aFoleyType++)
	{
		FoleyParams* aFoleyParams = LookupFoley((FoleyType)aFoleyType);
		if (TestBit(aFoleyParams->mFoleyFlags, FoleyFlags::FOLEYFLAGS_MUTE_ON_PAUSE))  // 如果指定了暂停时静默
		{
			FoleyTypeData* aFoleyData = &mFoleyTypeData[aFoleyType];
			for (int i = 0; i < MAX_FOLEY_INSTANCES; i++)  // 设定每一个音效实例的暂停与否
			{
				FoleyInstance* aFoleyInstance = &aFoleyData->mFoleyInstances[i];
				if (aFoleyInstance->mRefCount != 0)  // 如果音效实例存在引用
				{
					TodDSoundInstance* aSoundInstance = (TodDSoundInstance*)aFoleyInstance->mInstance;
					if (theEnteringPause)
					{
						aFoleyInstance->mPaused = true;
						if (aSoundInstance->mSoundBuffer == nullptr)
						{
							aFoleyInstance->mPauseOffset = 0;
							aSoundInstance->Stop();
						}
						else
						{
							aFoleyInstance->mPauseOffset = aSoundInstance->GetSoundPosition();  // 备份暂停时的播放进度
							aSoundInstance->Stop();
						}
					}
					else if (aFoleyInstance->mPaused)
					{
						aFoleyInstance->mPaused = false;
						bool aIsLooping = TestBit(aFoleyParams->mFoleyFlags, FoleyFlags::FOLEYFLAGS_LOOP);
						aSoundInstance->Play(aIsLooping, false);
						if (aSoundInstance->mSoundBuffer != nullptr)
							aSoundInstance->SetSoundPosition(aFoleyInstance->mPauseOffset);  // 还原暂停前的播放进度
					}
				}
			}
		}
	}
}

//0x5153F0
void TodFoley::CancelPausedFoley()
{
	SoundSystemReleaseFinishedInstances(this);
	for (int aFoleyType = 0; aFoleyType < gFoleyParamArraySize; aFoleyType++)
	{
		FoleyTypeData* aFoleyData = &mFoleyTypeData[aFoleyType];
		for (int i = 0; i < MAX_FOLEY_INSTANCES; i++)  // 判断每一个音效实例的暂停与否
		{
			FoleyInstance* aFoleyInstance = &aFoleyData->mFoleyInstances[i];
			if (aFoleyInstance->mRefCount != 0 && aFoleyInstance->mPaused)  // 如果音效实例存在引用且处于暂停状态
			{
				aFoleyInstance->mRefCount = 0;
				aFoleyInstance->mInstance->Release();
				aFoleyInstance->mInstance = nullptr;
			}
		}
	}
}

//0x515460
void TodFoley::ApplyMusicVolume(FoleyInstance* theFoleyInstance)
{
	if (gSexyAppBase->mSfxVolume < 1e-6)
		theFoleyInstance->mInstance->SetVolume(0.0);
	else
		theFoleyInstance->mInstance->SetVolume(gSexyAppBase->mMusicVolume / gSexyAppBase->mSfxVolume);  // 这样得到的音量在乘以音效音量后就与音乐音量相等
}

//0x5154A0
void TodFoley::RehookupSoundWithMusicVolume()
{
	SoundSystemReleaseFinishedInstances(this);
	for (int aFoleyType = 0; aFoleyType < gFoleyParamArraySize; aFoleyType++)
	{
		FoleyParams* aFoleyParams = LookupFoley((FoleyType)aFoleyType);
		if (TestBit(aFoleyParams->mFoleyFlags, FoleyFlags::FOLEYFLAGS_USES_MUSIC_VOLUME))  // 如果规定了使用音乐音量
		{
			FoleyTypeData* aFoleyData = &mFoleyTypeData[aFoleyType];
			for (int i = 0; i < MAX_FOLEY_INSTANCES; i++)  // 设定每一个音效实例的音量
			{
				FoleyInstance* aFoleyInstance = &aFoleyData->mFoleyInstances[i];
				if (aFoleyInstance->mRefCount != 0)  // 如果音效实例存在引用
					ApplyMusicVolume(aFoleyInstance);
			}
		}
	}
}

//0x515560
bool TodFoley::IsFoleyPlaying(FoleyType theFoleyType)
{
	SoundSystemReleaseFinishedInstances(this);
	return SoundSystemFindInstance(this, theFoleyType) != nullptr;
}
