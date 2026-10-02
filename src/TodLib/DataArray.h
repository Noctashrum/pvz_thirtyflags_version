#ifndef __DATAARRAY_H__
#define __DATAARRAY_H__

#include <new.h>
#include <string.h>
#include "TodDebug.h"
#include "TodCommon.h"

enum 
{
	DATA_ARRAY_INDEX_MASK = 65535,
	DATA_ARRAY_KEY_MASK = -65536,
	DATA_ARRAY_KEY_SHIFT = 16,
	DATA_ARRAY_MAX_SIZE = 65536,
	DATA_ARRAY_KEY_FIRST = 1
};

template <typename T> class DataArray
{
public:
	class DataArrayItem
	{
	public:
		T					mItem;
		unsigned int		mID;
	};
	
public:
	DataArrayItem*			mBlock;
	unsigned int			mMaxUsedCount;
	unsigned int			mMaxSize;
	unsigned int			mFreeListHead;
	unsigned int			mSize;
	unsigned int			mNextKey;
	const char*				mName;

public:
	DataArray<T>()
	{
		mBlock = nullptr;
		mMaxUsedCount = 0U;
		mMaxSize = 0U;
		mFreeListHead = 0U;
		mSize = 0U;
		mNextKey = 1U;
		mName = nullptr;
	}

	~DataArray<T>()
	{
		DataArrayDispose();
	}

	// One slot past the logical capacity is allocated as an "overflow sink".
	// It is never handed out in normal operation; DataArrayAlloc() returns it only
	// when the pool is exhausted, so that a full pool is reported instead of
	// writing past the end of the block. See DataArrayAlloc().
	DataArrayItem* DataArraySink()
	{
		return &mBlock[mMaxSize];
	}

	bool DataArrayIsSink(T* theItem)
	{
		return mBlock != nullptr && theItem == &mBlock[mMaxSize].mItem;
	}

	void DataArrayInitialize(unsigned int theMaxSize, const char* theName)
	{
		TOD_ASSERT(mBlock == nullptr);
		mBlock = (DataArrayItem*)operator new[](sizeof(DataArrayItem) * (theMaxSize + 1));
		mMaxSize = theMaxSize;
		mNextKey = ((((WORD)theName[1] & 0xF) << 8) | theName[2]) + 0xD000;
		mName = theName;

		// Mark the sink as not-in-use so a stale id never resolves to it.
		DataArraySink()->mID = 0;
	}

	void DataArrayDispose()
	{
		if (mBlock != nullptr)
		{
			DataArrayFreeAll();
			operator delete[](mBlock);
			mBlock = nullptr;
			mMaxUsedCount = 0U;
			mMaxSize = 0U;
			mFreeListHead = 0U;
			mSize = 0U;
			mName = nullptr;
		}
	}

	void DataArrayFree(T* theItem)
	{
		// The sink is shared and not on the free list; freeing it would corrupt
		// both mSize and the free list. It can only arrive here from code that
		// was handed the sink by a full DataArrayAlloc().
		if (DataArrayIsSink(theItem))
		{
			PVZ_LOG_ONCE(PVZ_CH_MEMORY, PVZ_LOG_WARN,
				"DataArrayFree() on the overflow sink for '%s' - ignored",
				mName != nullptr ? mName : "?");
			return;
		}

		DataArrayItem* aItem = (DataArrayItem*)theItem;
		TOD_ASSERT(DataArrayGet(aItem->mID) == theItem, "Failed: DataArrayFree(0x%x) in %s", theItem, mName);
		theItem->~T();
		unsigned int anId = aItem->mID & DATA_ARRAY_INDEX_MASK;
		aItem->mID = mFreeListHead;
		mFreeListHead = anId;
		mSize--;
	}

	void DataArrayFreeAll()
	{
		T* aItem = nullptr;
		while (IterateNext(aItem))
			DataArrayFree(aItem);

		mFreeListHead = 0U;
		mMaxUsedCount = 0U;
	}

	inline unsigned int DataArrayGetID(T* theItem)
	{
		if (DataArrayIsSink(theItem))
			return 0;

		DataArrayItem* aItem = (DataArrayItem*)theItem;
		TOD_ASSERT(DataArrayGet(aItem->mID) == theItem, "Failed: DataArrayGetID(0x%x) for %s", theItem, mName);
		return aItem->mID;
	}

	bool IterateNext(T*& theItem)
	{
		DataArray<T>::DataArrayItem* aItem = (DataArray<T>::DataArrayItem*)theItem;
		if (aItem == nullptr)
			aItem = &mBlock[0];
		else
			aItem++;

		DataArray<T>::DataArrayItem* aLast = &mBlock[mMaxUsedCount];
		while ((unsigned int)aItem < (unsigned int)aLast)
		{
			if (aItem->mID & DATA_ARRAY_KEY_MASK)
			{
				theItem = (T*)aItem;
				return true;
			}
			aItem++;
		}

		theItem = (T*)-1;
		return false;
	}

	T* DataArrayAlloc()
	{
		// ---- pool exhausted ----------------------------------------------------------------------
		// The original code only had TOD_ASSERT(mSize < mMaxSize) here, which compiled to
		// nothing in Release. When the pool filled up, mMaxUsedCount kept growing past
		// mMaxSize and this function wrote straight past the end of the block - silent heap
		// corruption that usually surfaced much later as an unrelated access violation.
		//
		// Now: report it loudly and hand back the reserved sink slot. The caller gets a
		// valid, zeroed object to write into, so the game keeps running, and the log says
		// exactly which pool ran out. Nothing is added to the free list and mSize does not
		// advance, so the accounting stays correct.
		if (mBlock == nullptr || mSize >= mMaxSize)
		{
			PVZ_LOG_THROTTLED(PVZ_CH_MEMORY, PVZ_LOG_ERROR, 25,
				"DataArray FULL: '%s' %u/%u - returning sink, this entity is NOT in the pool",
				mName != nullptr ? mName : "?", mSize, mMaxSize);
			PVZ_LOG_ONCE(PVZ_CH_MEMORY, PVZ_LOG_ERROR,
				"DataArray '%s' hit its %u slot limit. Raise the capacity passed to "
				"DataArrayInitialize() (Board.cpp) or stop spawning so many.",
				mName != nullptr ? mName : "?", mMaxSize);

			if (mBlock == nullptr)
				return nullptr;

			DataArrayItem* aSink = DataArraySink();
			new (aSink)T();
			aSink->mID = 0;
			return (T*)aSink;
		}

		TOD_ASSERT(mFreeListHead <= mMaxUsedCount, "DataArrayAlloc error in %s", mName);
		unsigned int aNext = mMaxUsedCount;
		if (mFreeListHead == mMaxUsedCount)
			mFreeListHead = ++mMaxUsedCount;
		else
		{
			aNext = mFreeListHead;
			mFreeListHead = mBlock[mFreeListHead].mID;
		}

		DataArray<T>::DataArrayItem* aNewItem = &mBlock[aNext];
		memset(aNewItem, 0, sizeof(DataArrayItem));
		aNewItem->mID = (mNextKey++ << DATA_ARRAY_KEY_SHIFT) | aNext;
		if (mNextKey == DATA_ARRAY_MAX_SIZE) mNextKey = 1;
		mSize++;

		// Getting close to the limit is worth knowing before it actually happens -
		// this is how you find out a mod needs a bigger pool rather than a crash.
		if (mMaxSize > 0u && (mSize * 10u) >= (mMaxSize * 9u))
		{
			PVZ_LOG_THROTTLED(PVZ_CH_MEMORY, PVZ_LOG_WARN, 100,
				"DataArray near capacity: '%s' %u/%u",
				mName != nullptr ? mName : "?", mSize, mMaxSize);
		}

		new (aNewItem)T();
		return (T*)aNewItem;
	}

	T* DataArrayTryToGet(unsigned int theId)
	{
		if (!theId || (theId & DATA_ARRAY_INDEX_MASK) >= mMaxSize)
			return nullptr;

		DataArrayItem* aBlock = &mBlock[theId & DATA_ARRAY_INDEX_MASK];
		return aBlock->mID == theId ? &aBlock->mItem : nullptr;
	}

	T* DataArrayGet(unsigned int theId)
	{
		T* aItem = DataArrayTryToGet(theId);
		TOD_ASSERT(aItem != nullptr, "Failed: DataArrayGet(0x%x) for %s", theId, mName);

		// Stale or bogus id. Returning the sink keeps the caller out of unmapped
		// memory; the assert above has already recorded the id that failed to resolve.
		if (aItem == nullptr)
		{
			if (mBlock == nullptr)
				return nullptr;
			return &DataArraySink()->mItem;
		}
		return aItem;
	}
};

#endif