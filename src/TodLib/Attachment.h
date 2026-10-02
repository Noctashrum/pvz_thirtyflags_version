#ifndef __ATTACHMENT_H__
#define __ATTACHMENT_H__

#include "DataArray.h"
#include "../SexyAppFramework/SexyMatrix.h"

namespace Sexy
{
	class Graphics;
}

#define MAX_EFFECTS_PER_ATTACHMENT 16

class Trail;
class Reanimation;
class TodParticleSystem;
enum ParticleEffect;
enum ReanimationType;

enum AttachmentID
{ 
	ATTACHMENTID_NULL
};

enum EffectType
{
	EFFECT_PARTICLE,
	EFFECT_TRAIL,
	EFFECT_REANIM,
	EFFECT_ATTACHMENT,
	EFFECT_OTHER
};

class AttachEffect
{
public:
	unsigned int				mEffectID;
	EffectType					mEffectType;
	Sexy::SexyTransform2D		mOffset;
	bool						mDontDrawIfParentHidden;
	bool						mDontPropogateColor;
};

class Attachment
{
public:
	AttachEffect				mEffectArray[MAX_EFFECTS_PER_ATTACHMENT];
	int							mNumEffects;
	bool						mDead;

public:
	Attachment();
	~Attachment();

	void						Update();
	void						SetPosition(const Sexy::SexyVector2& thePosition);
	void						SetMatrix(const Sexy::SexyTransform2D& theMatrix);
	void						OverrideColor(const Sexy::Color& theColor);
	void						OverrideScale(float theScale);
	void						Draw(Sexy::Graphics* g, bool theParentHidden);
	void						AttachmentDie();
	void						Detach();
	void						CrossFade(const char* theCrossFadeName);
	void						PropogateColor(const Sexy::Color& theColor, bool theEnableAdditiveColor, const Sexy::Color& theAdditiveColor, bool theEnableOverlayColor, const Sexy::Color& theOverlayColor);
};

AttachEffect*					AttachReanim(AttachmentID& theAttachmentID, Reanimation* theReanimation, float theOffsetX, float theOffsetY);
AttachEffect*					AttachParticle(AttachmentID& theAttachmentID, TodParticleSystem* theParticleSystem, float theOffsetX, float theOffsetY);
AttachEffect*					AttachTrail(AttachmentID& theAttachmentID, Trail* theTrail, float theOffsetX, float theOffsetY);
void							AttachmentPropogateColor(AttachmentID theAttachmentID, const Sexy::Color& theColor, bool theEnableAdditiveColor, const Sexy::Color& theAdditiveColor, bool theEnableOverlayColor, const Sexy::Color& theOverlayColor);
void							AttachmentOverrideColor(AttachmentID theAttachmentID, const Sexy::Color& theColor);
void							AttachmentOverrideScale(AttachmentID theAttachmentID, float theScale);
void							AttachmentUpdateAndMove(AttachmentID& theAttachmentID, float theX, float theY);
void							AttachmentUpdateAndSetMatrix(AttachmentID& theAttachmentID, Sexy::SexyTransform2D& theMatrix);
void							AttachmentDraw(AttachmentID theAttachmentID, Sexy::Graphics* g, bool theParentHidden);
void							AttachmentDetach(AttachmentID& theAttachmentID);
void							AttachmentDetachCrossFadeParticleType(AttachmentID& theAttachmentID, ParticleEffect theParticleEffect, const char* theCrossFadeName);
void							AttachmentReanimTypeDie(AttachmentID& theAttachmentID, ReanimationType theReanimType);
void							AttachmentDie(AttachmentID& theAttachmentID);
void							AttachmentCrossFade(AttachmentID& theAttachmentID, const char* theCrossFadeName);
AttachEffect*					FindFirstAttachment(AttachmentID& theAttachmentID);
Reanimation*					FindReanimAttachment(AttachmentID& theAttachmentID);
AttachEffect*					CreateEffectAttachment(AttachmentID& theAttachmentID, EffectType theEffectType, unsigned int theDataID, float theOffsetX, float theOffsetY);
bool							IsFullOfAttachments(AttachmentID& theAttachmentID);

class AttachmentHolder
{
public:
	DataArray<Attachment>		mAttachments;

public:
	AttachmentHolder();
	~AttachmentHolder();

	void						InitializeHolder();
	void						DisposeHolder();
	Attachment*					AllocAttachment();
};

#endif