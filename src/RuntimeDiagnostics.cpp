#include <NpcDataFix/RuntimeDiagnostics.hpp>

namespace NpcDataFix::RuntimeDiagnostics
{
    namespace
    {
        constexpr char BuildId[] = "animus6-diag1";
        constexpr std::uint64_t MaximumLoggedCalls = 256;

        HMODULE g_module = nullptr;
        INIT_ONCE g_logInit = INIT_ONCE_STATIC_INIT;
        HANDLE g_log = INVALID_HANDLE_VALUE;
        SRWLOCK g_logLock = SRWLOCK_INIT;
        volatile LONG64 g_callId = 0;
        volatile LONG g_loggerFailureReported = 0;
        volatile LONG g_installLineWritten = 0;

        struct Line
        {
            char bytes[512]{};
            DWORD length = 0;

            void Character(const char value) noexcept
            {
                if (length < sizeof(bytes))
                {
                    bytes[length++] = value;
                }
            }

            void Text(const char* value) noexcept
            {
                if (value == nullptr)
                {
                    return;
                }
                while (*value != '\0')
                {
                    Character(*value++);
                }
            }

            void Unsigned(std::uint64_t value) noexcept
            {
                char reversed[20]{};
                unsigned count = 0;
                do
                {
                    reversed[count++] = static_cast<char>('0' + (value % 10));
                    value /= 10;
                } while (value != 0 && count < sizeof(reversed));

                while (count != 0)
                {
                    Character(reversed[--count]);
                }
            }

        };

        const char* OperationName(const FallbackOperation operation) noexcept
        {
            switch (operation)
            {
            case FallbackOperation::NpcUpdateInitial:
                return "npc.update.initial";
            case FallbackOperation::NpcInsert:
                return "npc.insert";
            case FallbackOperation::NpcUpdateRetry:
                return "npc.update.retry";
            case FallbackOperation::AnimusUpdateInitial:
                return "animus.update.initial";
            case FallbackOperation::AnimusInsert:
                return "animus.insert";
            case FallbackOperation::AnimusUpdateRetry:
                return "animus.update.retry";
            }
            return "unknown";
        }

        BOOL CALLBACK OpenLog(PINIT_ONCE, PVOID, PVOID*) noexcept
        {
            wchar_t path[1024]{};
            const DWORD length = GetModuleFileNameW(
                g_module, path, static_cast<DWORD>(sizeof(path) / sizeof(path[0])));
            if (length == 0 || length >= (sizeof(path) / sizeof(path[0])))
            {
                return TRUE;
            }

            DWORD filename = length;
            while (filename != 0 && path[filename - 1] != L'\\' && path[filename - 1] != L'/')
            {
                --filename;
            }

            constexpr wchar_t LogName[] = L"NpcDataFix-runtime.log";
            if (filename + (sizeof(LogName) / sizeof(LogName[0])) >
                (sizeof(path) / sizeof(path[0])))
            {
                return TRUE;
            }

            for (DWORD index = 0; index < sizeof(LogName) / sizeof(LogName[0]); ++index)
            {
                path[filename + index] = LogName[index];
            }

            g_log = CreateFileW(
                path,
                FILE_APPEND_DATA,
                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                nullptr,
                OPEN_ALWAYS,
                FILE_ATTRIBUTE_NORMAL,
                nullptr);
            return TRUE;
        }

        HANDLE LogHandle() noexcept
        {
            InitOnceExecuteOnce(&g_logInit, OpenLog, nullptr, nullptr);
            if (g_log == INVALID_HANDLE_VALUE &&
                InterlockedCompareExchange(&g_loggerFailureReported, 1, 0) == 0)
            {
                OutputDebugStringA(
                    "RF2232-NpcDataFix: cannot create NpcDataFix-runtime.log.\n");
            }
            return g_log;
        }

        void Write(Line& line) noexcept
        {
            line.Text("\r\n");
            const HANDLE log = LogHandle();
            if (log == INVALID_HANDLE_VALUE)
            {
                return;
            }

            AcquireSRWLockExclusive(&g_logLock);
            DWORD written = 0;
            WriteFile(log, line.bytes, line.length, &written, nullptr);
            ReleaseSRWLockExclusive(&g_logLock);
        }

        void Prefix(Line& line, const CallContext& call) noexcept
        {
            line.Text("RF2232-NpcDataFix build=");
            line.Text(BuildId);
            line.Text(" pid=");
            line.Unsigned(GetCurrentProcessId());
            line.Text(" tid=");
            line.Unsigned(GetCurrentThreadId());
            line.Text(" call=");
            line.Unsigned(call.id);
        }
    }

    void SetModule(const HMODULE module) noexcept
    {
        g_module = module;
    }

    CallContext BeginCall(const DWORD installStatus) noexcept
    {
        const auto id = static_cast<std::uint64_t>(InterlockedIncrement64(&g_callId));
        const CallContext call{id, id <= MaximumLoggedCalls};
        if (!call.enabled)
        {
            return call;
        }

        if (InterlockedCompareExchange(&g_installLineWritten, 1, 0) == 0)
        {
            Line install;
            install.Text("RF2232-NpcDataFix build=");
            install.Text(BuildId);
            install.Text(" event=install status=");
            install.Unsigned(installStatus);
            Write(install);
        }

        Line line;
        Prefix(line, call);
        line.Text(" event=enter");
        Write(line);
        return call;
    }

    void WriteIdentity(
        const CallContext& call,
        const std::uint32_t characterSerial,
        const std::uint32_t accountSerial,
        const char race) noexcept
    {
        if (!call.enabled)
        {
            return;
        }
        Line line;
        Prefix(line, call);
        line.Text(" event=identity serial=");
        line.Unsigned(characterSerial);
        line.Text(" account=");
        line.Unsigned(accountSerial);
        line.Text(" race=");
        line.Unsigned(static_cast<unsigned char>(race));
        Write(line);
    }

    void __cdecl BeforeOperation(
        void* const context,
        const FallbackOperation operation) noexcept
    {
        const auto& call = *static_cast<const CallContext*>(context);
        if (!call.enabled)
        {
            return;
        }
        Line line;
        Prefix(line, call);
        line.Text(" op=");
        line.Text(OperationName(operation));
        line.Text(" phase=begin");
        Write(line);
    }

    void __cdecl AfterOperation(
        void* const context,
        const FallbackOperation operation,
        const bool result) noexcept
    {
        const auto& call = *static_cast<const CallContext*>(context);
        if (!call.enabled)
        {
            return;
        }
        Line line;
        Prefix(line, call);
        line.Text(" op=");
        line.Text(OperationName(operation));
        line.Text(" phase=end result=");
        line.Character(result ? '1' : '0');
        Write(line);
    }

    void EndCall(const CallContext& call, const bool result) noexcept
    {
        if (!call.enabled)
        {
            return;
        }
        Line line;
        Prefix(line, call);
        line.Text(" event=exit result=");
        line.Character(result ? '1' : '0');
        Write(line);
    }
}
