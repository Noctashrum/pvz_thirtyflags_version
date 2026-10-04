#ifndef __DATAARRAY_H__
#define __DATAARRAY_H__

#include <new.h>
#include <string.h>
#include <intrin.h>
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

	// 【性能】活槽位图：一位一槽。
	//
	// 为什么需要：IterateNext 原本是「从当前槽逐槽线性扫到 mMaxUsedCount」，
	// 而 mMaxUsedCount 是只增不减的高水位，槽位里放的是大对象
	// （Zombie / Plant / Projectile 都是几百字节），于是每次遍历都要跨数百字节
	// 去探测空槽 —— 全是 cache miss。实测 245 个投射物每帧各调一次
	// IterateZombies，就是每帧几十万次这种探测，这正是「越打越卡」的机械原因。
	// 位图把「探活」成本降到 O(容量/32)，且遍历顺序与结果与原实现完全一致。
	unsigned int*			mUsedBits;

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
		mUsedBits = nullptr;
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

		// 【性能】位图与池一一对应，多分配一组以覆盖 sink 槽。
		unsigned int aBitWords = (theMaxSize + 32U) >> 5;
		mUsedBits = (unsigned int*)operator new[](sizeof(unsigned int) * aBitWords);
		memset(mUsedBits, 0, sizeof(unsigned int) * aBitWords);

		// Mark the sink as not-in-use so a stale id never resolves to it.
		DataArraySink()->mID = 0;
	}

	void DataArrayDispose()
	{
		if (mBlock != nullptr)
		{
			DataArrayFreeAll();
			operator delete[](mBlock);
			operator delete[](mUsedBits);
			mUsedBits = nullptr;
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
		DataArrayClearUsedBit(anId);
	}

	void DataArrayFreeAll()
	{
		T* aItem = nullptr;
		while (IterateNext(aItem))
			DataArrayFree(aItem);

		mFreeListHead = 0U;
		mMaxUsedCount = 0U;
		if (mUsedBits != nullptr)
		{
			memset(mUsedBits, 0, sizeof(unsigned int) * ((mMaxSize + 32U) >> 5));
		}
	}

	inline unsigned int DataArrayGetID(T* theItem)
	{
		if (DataArrayIsSink(theItem))
			return 0;

		DataArrayItem* aItem = (DataArrayItem*)theItem;
		TOD_ASSERT(DataArrayGet(aItem->mID) == theItem, "Failed: DataArrayGetID(0x%x) for %s", theItem, mName);
		return aItem->mID;
	}

		// 【性能】位图辅助
	inline void DataArraySetUsedBit(unsigned int theIndex)
	{
		if (mUsedBits != nullptr)
			mUsedBits[theIndex >> 5] |= (1U << (theIndex & 31));
	}

	inline void DataArrayClearUsedBit(unsigned int theIndex)
	{
		if (mUsedBits != nullptr)
			mUsedBits[theIndex >> 5] &= ~(1U << (theIndex & 31));
	}

	// 【性能】读档后重建位图：存档只同步 mBlock（含 mID），位图不在存档里，
	// 所以读盘后必须按 mID 的 key 部分重新标一遍，否则遍历会漏掉所有实体。
	void DataArrayRebuildUsedBits()
	{
		if (mUsedBits == nullptr || mBlock == nullptr)
			return;

		memset(mUsedBits, 0, sizeof(unsigned int) * ((mMaxSize + 32U) >> 5));
		for (unsigned int i = 0; i < mMaxUsedCount; i++)
		{
			if (mBlock[i].mID & DATA_ARRAY_KEY_MASK)
				DataArraySetUsedBit(i);
		}
	}

	// 【性能】遍历：用位图一次跳过整段（32 槽）空位，不再逐槽触碰几百字节的大对象。
	// 返回值、遍历顺序（按槽位下标升序）与原实现完全一致。
	bool IterateNext(T*& theItem)
	{
		unsigned int aIndex;
		if (theItem == nullptr)
			aIndex = 0;
		else if (theItem == (T*)-1)
			return false;
		else
			aIndex = (unsigned int)(((DataArrayItem*)theItem) - mBlock) + 1;

		if (mUsedBits == nullptr)
		{
			// 位图不可用（理论上不会发生）：退回原来的逐槽线性扫描
			while (aIndex < mMaxUsedCount)
			{
				if (mBlock[aIndex].mID & DATA_ARRAY_KEY_MASK)
				{
					theItem = (T*)&mBlock[aIndex];
					return true;
				}
				aIndex++;
			}
			theItem = (T*)-1;
			return false;
		}

		// 空池快路径：池里一个活对象都没有（每旗开局、清场瞬间很常见）
		if (mSize == 0)
		{
			theItem = (T*)-1;
			return false;
		}

		unsigned int aEnd = mMaxUsedCount;
		while (aIndex < aEnd)
		{
			unsigned int aWordIndex = aIndex >> 5;
			// 清掉当前位之前的所有位，只保留「aIndex 及其之后」的活槽
			unsigned int aBits = mUsedBits[aWordIndex] & (0xFFFFFFFFu << (aIndex & 31));
			if (aBits == 0)
			{
				// 整段 32 槽全空：直接跳到下一段，一个槽都不触碰
				aIndex = (aWordIndex + 1) << 5;
				continue;
			}

			// 定位这一段里最低的置位：用硬件指令一次搞定（朴素逐位循环最多 31 次移位，
			// 实测那才是位图版的主要开销）
			unsigned long aBit = 0;
			_BitScanForward(&aBit, aBits);

			aIndex = (aWordIndex << 5) + (unsigned int)aBit;
			if (aIndex >= aEnd)
				break;

			theItem = (T*)&mBlock[aIndex];
			return true;
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
		mUsedBits[aNext >> 5] |= (1U << (aNext & 31));

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