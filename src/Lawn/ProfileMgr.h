#ifndef __PROFILEMGR_H__
#define __PROFILEMGR_H__

#include "../SexyAppFramework/Common.h"

class DataSync;
class PlayerInfo;
typedef std::pair<SexyString, PlayerInfo> ProfilePair;
typedef std::map<SexyString, PlayerInfo, SexyStringLessNoCase> ProfileMap;

class ProfileMgr
{
protected:
	ProfileMap			mProfileMap;			//+0x4
	unsigned long		mNextProfileId;			//+0x10
	unsigned long		mNextProfileUseSeq;		//+0x14

protected:
	void				SyncState(DataSync& theSync);
	void				DeleteOldestProfile();
	void				DeleteOldProfiles();

public:
	bool				DeleteProfile(const SexyString& theName);

protected:
	void				DeleteProfile(ProfileMap::iterator theProfile);

public:
	ProfileMgr();
	virtual ~ProfileMgr();

	void				Clear();
	void				Load();
	void				Save();
	int					GetNumProfiles() const;
	PlayerInfo*			GetProfile(const SexyString& theName);
	PlayerInfo*			AddProfile(const SexyString& theName);
	PlayerInfo*			GetAnyProfile();
	bool				RenameProfile(const SexyString& theOldName, const SexyString& theNewName);
	ProfileMap&			GetProfileMap();
};

#endif
