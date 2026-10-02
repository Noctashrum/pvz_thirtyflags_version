#ifndef __TODDEFINITION_H__
#define __TODDEFINITION_H__

#include <string>
#include "TodList.h"
#include "TodCommon.h"

namespace Sexy
{
	class Font;
	class Image;
	class XMLParser;
	class SexyVector2;
}

enum DefFieldType
{
	DT_INVALID,
	DT_INT,
	DT_FLOAT,
	DT_STRING,
	DT_ENUM,
	DT_VECTOR2,
	DT_ARRAY,
	DT_TRACK_FLOAT,
	DT_FLAGS,
	DT_IMAGE,
	DT_FONT
};

// ====================================================================================================
// ★ 【浮点参数轨道节点】
// ----------------------------------------------------------------------------------------------------
// 每个节点描述属性数值随时间变化的一个阶段。
// ====================================================================================================
class FloatParameterTrackNode
{
public:
	float					   mTime;						// 阶段的起始时间
	float					   mLowValue;					// 阶段内数据允许的最小值
	float					   mHighValue;					// 阶段内数据允许的最大值
	TodCurves				   mCurveType;					// 从当前阶段过渡至下一阶段的缓动效果曲线
	TodCurves				   mDistribution;				// 阶段内数据在最小值和最大值之间的概率分布曲线
};

// ====================================================================================================
// ★ 【浮点参数轨道】
// ----------------------------------------------------------------------------------------------------
// 每条轨道描述发射器的一种属性的数值随时间的变化规律和取值范围。
// ====================================================================================================
class FloatParameterTrack
{
public:
	FloatParameterTrackNode*	mNodes;
	int							mCountNodes;
};

bool							FloatTrackIsSet(const FloatParameterTrack& theTrack);
void							FloatTrackSetDefault(FloatParameterTrack& theTrack, float theValue);
float							FloatTrackEvaluate(FloatParameterTrack& theTrack, float theTimeValue, float theInterp);
float							FloatTrackEvaluateFromLastTime(FloatParameterTrack& theTrack, float theTimeValue, float theInterp);
bool							FloatTrackIsConstantZero(FloatParameterTrack& theTrack);

/*
	[为通俗理解以下内容，在此规定]
		“用于存储其他类的定义数据”的类，称为定义数据类，记作 _DefClass。相应地，将被 _DefClass 定义的类记作 _Class。
		例如，ReanimatorDefinition 作为 Reanimation 类（动画类）的定义数据类，TodParticleDefinition 作为 TodParticleSystem 类（粒子系统类）的定义数据类等。
*/

// ====================================================================================================
// ★ 【定义标志】
// ----------------------------------------------------------------------------------------------------
// 对于按标志位判断的（/枚举类型的）数据，一个 DefSymbol 记录其一个标志位上的（/一个枚举项的）值。
// ====================================================================================================
class DefSymbol
{
public:
	int								mSymbolValue;			//+0x0：标志位上的值或枚举项对应的数值，若为 -1 则表示不存在该项
	const char*						mSymbolName;			//+0x4：标志位或枚举项的名称，为空指针时表示不存在该项，故被作为读取结束的标志
};

// ====================================================================================================
// ★ 【结构字段】
// ----------------------------------------------------------------------------------------------------
// 结构字段记录了一个类中的一个的成员变量（_MemVar）的数据和其在所处类中的结构。
// ====================================================================================================
class DefField
{
public:
	const char*						mFieldName;				//+0x0：指向 _MemVar 的名称。指向空字符数组时表示无此变量，故被作为读取结束的标志
	int								mFieldOffset;			//+0x4：_MemVar 在所处类中的偏移量（结合汇编理解）
	DefFieldType					mFieldType;				//+0x8：*_MemVar 的数据存储类型，不同类型的数据的读取方式也有所不同
	void*							mExtraData;				//+0xC：额外数据。用于对 *_MemVar 中包含的指针变量进行深拷贝。
	// 若 _MemVar 为指向其他定义数据的指针型变量，则 mExtraData 为指向 _MemVar 所定义的类的定义结构图的指针；
	// 若 _MemVar 为标志或枚举类型的数据，则 mExtraData 为指向其各标志数据的 DefSymbol 数组的指针；否则，mExtraData 为空指针。
	// 虽然借助一个 _DefClass 类的定义结构图就已经可以通过相关函数读取该 _DefClass 的全部数据（即进行浅拷贝），
	// 但是 _DefClass 中的部分指针变量指向的数据仍然需要进一步依靠相应类型的定义结构图进行递归读取（即进行深拷贝）。
	// 即：通过层层嵌套的定义结构图，将原本含有层级关系的各类型变量指针“展开”，直到当前变量的数据中已经不存在可以“展开”的指针为止，递归读取结束。
};

// ====================================================================================================
// ★ 【定义结构图】
// ----------------------------------------------------------------------------------------------------
// 定义结构图描述了一种定义数据类（_DefClass）中定义数据的存储格式和读取方式，类似于“_DefDefClass”。
// ====================================================================================================
class DefMap
{
public:
	DefField*						mMapFields;						//+0x0：结构字段的数组，记录 _DefClass 类中的各成员变量在 _DefClass 中的结构（每项记录一种结构）
	int								mDefSize;						//+0x4：一个 _DefClass 实例所占用的内存大小，也即后续初次读取时的读取长度，一般为 sizeof(_DefClass)
	void*							(*mConstructorFunc)(void*);		//+0x8：_DefClass 类型实例的构造函数的指针
};

// ====================================================================================================
// ★ 【定义数组】
// ----------------------------------------------------------------------------------------------------
// 一个定义数组对应一种指针类型变量指向的类型。
// ====================================================================================================
class DefinitionArrayDef
{
public:
	void*							mArrayData;				//+0x0：由若干个特定定义数据类型的实例构成的数组，例如动画定义中的“轨道”定义
	int								mArrayCount;			//+0x4：数组的大小，例如动画定义中的“轨道”数量或粒子系统定义中的“发射器”数量
	// 定义数据类中的一个“数组（指针） + 数量”的组合，在读取时将被 DefField 视作一个 DefinitionArrayDef 结构
	// 例如 TodParticleDefinition 下的 *mEmitterDefs 和 mEmitterDefCount、以及 TodEmitterDefinition 下的 *mParticleFields 和 mParticleFieldCount 等。
	// 在读取时，作为 mArrayCount 的一项数据总是能在初次读取时就被正确读取（因为是整数类型），故其也会在后续 mArrayData 的修复过程中成为校验参考
};

// ====================================================================================================
// ★ 【压缩定义数据头】
// ----------------------------------------------------------------------------------------------------
// 在压缩数据前添加一个压缩定义数据头，记录校验缓存值及原始数据长度，用于在解压时检测数据完整性。
// ====================================================================================================
class CompressedDefinitionHeader
{
public:
	unsigned int					mCookie;				//+0x0：用于压缩校验的缓存值
	unsigned long					mUncompressedSize;		//+0x4：未压缩数据的长度
};

// ====================================================================================================
// ★ 【定义路径】
// ----------------------------------------------------------------------------------------------------
// 定义路径在一种贴图前缀与该前缀的贴图存放的文件夹路径之间建立关联。
// ====================================================================================================
class DefLoadResPath
{
public:
	const char*						mPrefix;				//+0x0：贴图的前缀，如“IMAGE_"
	const char*						mDirectory;				//+0x4：前缀对应的贴图所在文件夹，如“images\”
};

std::string							DefinitionGetCompiledFilePathFromXMLFilePath(const std::string& theXMLFilePath);
bool								IsFileInPakFile(const std::string& theFilePath);
bool								DefinitionIsCompiled(const std::string& theXMLFilePath);
bool								DefinitionReadCompiledFile(const std::string& theCompiledFilePath, DefMap* theDefMap, void* theDefinition);
void								DefinitionFillWithDefaults(DefMap* theDefMap, void* theDefinition);
void								DefinitionXmlError(Sexy::XMLParser* theXmlParser, const SexyChar* theFormat, ...);
bool								DefSymbolValueFromString(DefSymbol* theSymbolMap, const char* theName, int* theResultValue);
TodCurves							DefParseTrackCurve(const char*& thePtr);
bool								DefParseTrackTime(const char*& thePtr, FloatParameterTrackNode* theNode);
bool								DefParseTrackRangeNode(const char*& thePtr, FloatParameterTrackNode* theNode);
bool								DefinitionReadXMLString(Sexy::XMLParser* theXmlParser, SexyString& theValue);
bool								DefinitionReadIntField(Sexy::XMLParser* theXmlParser, int* theValue);
bool								DefinitionReadFloatField(Sexy::XMLParser* theXmlParser, float* theValue);
bool								DefinitionReadStringField(Sexy::XMLParser* theXmlParser, char** theValue);
bool								DefinitionReadEnumField(Sexy::XMLParser* theXmlParser, int* theValue, DefSymbol* theSymbolMap);
bool								DefinitionReadVector2Field(Sexy::XMLParser* theXmlParser, Sexy::SexyVector2* theValue);
bool								DefinitionReadArrayField(Sexy::XMLParser* theXmlParser, DefinitionArrayDef* theArray, DefField* theField);
bool								DefinitionReadFloatTrackField(Sexy::XMLParser* theXmlParser, FloatParameterTrack* theTrack);
bool								DefinitionReadFlagField(Sexy::XMLParser* theXmlParser, const SexyString& theElementName, uint* theResultValue, DefSymbol* theSymbolMap);
bool								DefinitionReadImageField(Sexy::XMLParser* theXmlParser, Sexy::Image** theImage);
bool								DefinitionReadFontField(Sexy::XMLParser* theXmlParser, Sexy::Font** theFont);
bool								DefinitionReadField(Sexy::XMLParser* theXmlParser, DefMap* theDefMap, void* theDefinition, bool* theDone);
size_t								DefinitionGetArraySize(DefinitionArrayDef* theArray, DefMap* theDefMap);
size_t								DefGetSizeImage(Sexy::Image** theImage);
bool								TodFindFontPath(Sexy::Font* theFont, std::string* thePath);
size_t								DefGetSizeFont(Sexy::Font** theFont);
size_t								DefinitionGetDeepSize(DefMap* theDefMap, void* theDefinition);
void								DefWriteToCacheArray(void*& theWritePtr, DefinitionArrayDef* theArray, DefMap* theDefMap);
void								DefWriteToCacheString(void*& theWritePtr, const char* theString);
void								DefWriteToCacheImage(void*& theWritePtr, Sexy::Image** theImage);
void								DefWriteToCacheFont(void*& theWritePtr, Sexy::Font** theFont);
void								DefWriteToCacheFloatTrack(void*& theWritePtr, FloatParameterTrack* theTrack);
void								DefMapWriteToCache(void*& theWritePtr, DefMap* theDefMap, void* theDefinition);
void*								DefinitionCompressCompiledBuffer(unsigned char* theBuffer, size_t theBufferSize, size_t* theResultSize);
bool								DefinitionWriteCompiledFile(const std::string& theCompiledFilePath, DefMap* theDefMap, void* theDefinition);
bool								DefinitionCompileFile(const std::string& theXMLFilePath, const std::string& theCompiledFilePath, DefMap* theDefMap, void* theDefinition);
void*								DefinitionAlloc(size_t theSize);
void*								DefinitionUncompressCompiledBuffer(void* theCompressedBuffer, size_t theCompressedBufferSize, size_t& theUncompressedSize, const std::string& theCompiledFilePath);
uint								DefinitionCalcHashSymbolMap(int aSchemaHash, DefSymbol* theSymbolMap);
uint								DefinitionCalcHashDefMap(int aSchemaHash, DefMap* theDefMap, TodList<DefMap*>& theProgressMaps);
uint								DefinitionCalcHash(DefMap* theDefMap);
bool								DefReadFromCacheString(void*& theReadPtr, char** theString);
bool								DefReadFromCacheArray(void*& theReadPtr, DefinitionArrayDef* theArray, DefMap* theDefMap);
bool								DefReadFromCacheImage(void*& theReadPtr, Sexy::Image** theImage);
bool								DefReadFromCacheFont(void*& theReadPtr, Sexy::Font** theFont);
bool								DefReadFromCacheFloatTrack(void*& theReadPtr, FloatParameterTrack* theTrack);
bool								DefMapReadFromCache(void*& theReadPtr, DefMap* theDefMap, void* theDefinition);
bool								DefinitionCompileAndLoad(const std::string& theXMLFilePath, DefMap* theDefMap, void* theDefinition);
bool								DefinitionLoadMap(Sexy::XMLParser* theXmlParser, DefMap* theDefMap, void* theDefinition);
bool								DefinitionLoadImage(Sexy::Image** theImage, const SexyString& theName);
bool								DefinitionLoadFont(Sexy::Font** theFont, const SexyString& theName);
bool								DefinitionLoadXML(const std::string& theFilename, DefMap* theDefMap, void* theDefinition);
void								DefinitionFreeArrayField(DefinitionArrayDef* theArray, DefMap* theDefMap);
void								DefinitionFreeMap(DefMap* theDefMap, void* theDefinition);

#endif