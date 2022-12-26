#pragma once
#include <chrono>
#include <algorithm>
#include <fstream>

//2022
//Basic profilers by The Cherno
//https://www.youtube.com/watch?v=xlAH4dbMVnU
//https://gist.github.com/TheCherno/31f135eea6ee729ab5f26a6908eb3a5e

// HOW TO/COULD USE
// Usage: include this header file somewhere in your code (eg. precompiled header), and then use like:
//
// Instrumentor::Get().BeginSession("Session Name");        // Begin session 
// {
//     InstrumentationTimer timer("Profiled Scope Name");   // Place code like this in scopes you'd like to include in profiling
//     // Code
// }
// Instrumentor::Get().EndSession();                        // End Session
//
// You will probably want to macro-fy this, to switch on/off easily and use things like __FUNCSIG__ for the profile name.
//
// If you want to create multiple sessions, for now, you need to create a seperate file for each session

// THE VISUAL PROFILER IS USED WITH CHROMES TRACING
// chrome://tracing/
// Just drag and drop the .json file into the window

template<typename Fn>
class ProfilerTimer
{
public:

	ProfilerTimer(const char* name, Fn&& func) 
		: Name(name), Func(func), Stopped(false)
	{
		StartTimePoint = std::chrono::high_resolution_clock::now();
	}

	~ProfilerTimer()
	{
		if (!Stopped)
			Stop();
	}

	void Stop()
	{
		auto endTimePoint = std::chrono::high_resolution_clock::now();
		long long start = std::chrono::time_point_cast<std::chrono::microseconds>(StartTimePoint).time_since_epoch().count();
		long long end = std::chrono::time_point_cast<std::chrono::microseconds>(endTimePoint).time_since_epoch().count();
		Stopped = true;
		float duration = (end - start) * 0.001f;// mircoseconds to milliseconds
		Func({ Name, duration });
		//printf("Duration Of %s: %.4f ms\n", Name, duration);
	}

private:
	const char* Name;
	Fn Func;
	bool Stopped;
	std::chrono::time_point<std::chrono::steady_clock> StartTimePoint;

};

#define PROFILE_SCOPE(name) ProfilerTimer timer##__LINE__(name, [&](ProfilerResult profileResult) { Profilers.push_back(profileResult); })

namespace VisualProfiler {
    struct ProfileResult
    {
        std::string Name;
        long long Start, End;
        uint32_t ThreadID;
    };

    struct InstrumentationSession
    {
        std::string Name;
    };
}

namespace VisualProfiler {
    class Instrumentor
    {
    private:
        InstrumentationSession* m_CurrentSession;
        std::ofstream m_OutputStream;
        int m_ProfileCount;
    public:
        Instrumentor()
            : m_CurrentSession(nullptr), m_ProfileCount(0)
        {
        }

        //File must be a .json within the ProfileResults folder: ProfileResults/filename.json      
        void BeginSession(const std::string& name, const std::string& filepath = "ProfileResults/results.json")
        {
            m_OutputStream.open(filepath);
            WriteHeader();
            m_CurrentSession = new InstrumentationSession{ name };
        }

        void EndSession()
        {
            WriteFooter();
            m_OutputStream.close();
            delete m_CurrentSession;
            m_CurrentSession = nullptr;
            m_ProfileCount = 0;
        }

        void WriteProfile(const ProfileResult& result)
        {
            if (m_ProfileCount++ > 0)
                m_OutputStream << ",";

            std::string name = result.Name;
            std::replace(name.begin(), name.end(), '"', '\'');

            m_OutputStream << "{";
            m_OutputStream << "\"cat\":\"function\",";
            m_OutputStream << "\"dur\":" << (result.End - result.Start) << ',';
            m_OutputStream << "\"name\":\"" << name << "\",";
            m_OutputStream << "\"ph\":\"X\",";
            m_OutputStream << "\"pid\":0,";
            m_OutputStream << "\"tid\":" << result.ThreadID << ",";
            m_OutputStream << "\"ts\":" << result.Start;
            m_OutputStream << "}";

            m_OutputStream.flush();
        }

        void WriteHeader()
        {
            m_OutputStream << "{\"otherData\": {},\"traceEvents\":[";
            m_OutputStream.flush();
        }

        void WriteFooter()
        {
            m_OutputStream << "]}";
            m_OutputStream.flush();
        }

        static Instrumentor& Get()
        {
            static Instrumentor instance;
            return instance;
        }
    };
}

namespace VisualProfiler {
    class InstrumentationTimer
    {
    public:

        InstrumentationTimer(const char* name)
            : m_Name(name), m_Stopped(false)
        {
            m_StartTimepoint = std::chrono::high_resolution_clock::now();
        }

        ~InstrumentationTimer()
        {
            if (!m_Stopped)
                Stop();
        }

        void Stop()
        {
            auto endTimepoint = std::chrono::high_resolution_clock::now();

            long long start = std::chrono::time_point_cast<std::chrono::microseconds>(m_StartTimepoint).time_since_epoch().count();
            long long end = std::chrono::time_point_cast<std::chrono::microseconds>(endTimepoint).time_since_epoch().count();

            uint32_t threadID = std::hash<std::thread::id>{}(std::this_thread::get_id());
            Instrumentor::Get().WriteProfile({ m_Name, start, end, threadID });

            m_Stopped = true;
        }

    private:
        const char* m_Name;
        std::chrono::time_point<std::chrono::high_resolution_clock> m_StartTimepoint;
        bool m_Stopped;
    };
}

#define PROFILING 1
#if PROFILING
#define VISUALPROFILE_SCOPE(name) VisualProfiler::InstrumentationTimer timer##__LINE__(name)
#define PROFILE_FUNCTION() VISUALPROFILE_SCOPE(__FUNCTION__)
#else
#define VISUALPROFILE_SCOPE(name)
#define PROFILE_FUNCTION()
#endif