#pragma once

#ifndef Engine_Macro_h__
#define Engine_Macro_h__

namespace Engine
{
#define	VTXCNTX		129
#define VTXCNTZ		129
#define VTXITV		1

#ifndef			MSG_BOX
#define			MSG_BOX(_message)			MessageBox(NULL, TEXT(_message), L"System Message", MB_OK)
#endif

#define			NS_BEGIN(NAMESPACE)		namespace NAMESPACE {
#define			NS_END						}

#define			USING(NAMESPACE)	using namespace NAMESPACE;

#ifdef ENGINE_EXPORTS
#define ENGINE_DLL __declspec(dllexport)
#else
#define ENGINE_DLL
#endif

#define NO_COPY(CLASSNAME)								\
		private:										\
		CLASSNAME(const CLASSNAME&) = delete;			\
		CLASSNAME& operator = (const CLASSNAME&)= delete;		

#define SINGLETONCLASS(c) \
private: \
    explicit c(); \
    c(const c& rhs) = delete; \
    c& operator=(const c& rhs) = delete; \
    virtual ~c(); \
public: \
    static c& GetInstance(); \
private: \
    enum { __singleton_dummy = 0 }

    template <typename T>
    T clamp(T value, T min, T max)
    {
        return (value < min) ? min : (value > max ? max : value);
    }

#define DELTA_TIME CTime::GetInstance().Get_DeltaTime()

#define BEGIN_SERIALIZEFIELD \
	public: \
	std::vector<FieldInfo> GetInspectorFields() override { \
	std::vector<FieldInfo> _fields;

#define SERIALIZEFIELD(var) \
	_fields.push_back({std::wstring(L#var).substr(3), DetectFieldType(var), &var});

#define END_SERIALIZEFIELD \
	return _fields; }
}

#endif // Engine_Macro_h__
