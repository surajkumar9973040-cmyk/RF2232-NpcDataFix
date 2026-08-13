#include <NpcDataFix/NpcDataFallback.hpp>

#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace NpcDataFix
{
    struct CRFWorldDatabase
    {
    };
}

namespace
{
    using namespace NpcDataFix;

    constexpr std::uint32_t CharacterSerial = 1234;
    constexpr std::uint32_t AccountSerial = 5678;
    constexpr char Race = 2;

    struct Scenario
    {
        std::vector<bool> updateNpc;
        std::vector<bool> insertNpc;
        std::vector<bool> updateAnimus;
        std::vector<bool> insertAnimus;
        std::vector<std::string> calls;
        std::size_t updateNpcIndex = 0;
        std::size_t insertNpcIndex = 0;
        std::size_t updateAnimusIndex = 0;
        std::size_t insertAnimusIndex = 0;
        std::vector<std::string> observerEvents;
        char race = Race;
        std::vector<long double> expectedAnimusInsertValues;
    };

    Scenario* g_scenario = nullptr;

    bool Next(std::vector<bool>& values, std::size_t& index)
    {
        if (index >= values.size())
        {
            std::cerr << "Unexpected mock call\n";
            std::exit(2);
        }
        return values[index++];
    }

    bool __cdecl UpdateNpc(CRFWorldDatabase*, std::uint32_t serial, std::uint32_t* values)
    {
        if (serial != CharacterSerial || values == nullptr)
        {
            std::exit(2);
        }
        g_scenario->calls.emplace_back("Update_NpcData");
        return Next(g_scenario->updateNpc, g_scenario->updateNpcIndex);
    }

    bool __cdecl InsertNpc(CRFWorldDatabase*, std::uint32_t serial, std::uint32_t* values)
    {
        if (serial != CharacterSerial || values == nullptr)
        {
            std::exit(2);
        }
        g_scenario->calls.emplace_back("Insert_NpcData");
        return Next(g_scenario->insertNpc, g_scenario->insertNpcIndex);
    }

    bool __cdecl UpdateAnimus(
        CRFWorldDatabase*, std::uint32_t serial, char race, long double* values)
    {
        if (serial != AccountSerial || race != g_scenario->race || values == nullptr)
        {
            std::exit(2);
        }
        g_scenario->calls.emplace_back("Update_AnimusData");
        return Next(g_scenario->updateAnimus, g_scenario->updateAnimusIndex);
    }

    bool __cdecl InsertAnimus(CRFWorldDatabase*, std::uint32_t serial, long double* values)
    {
        if (serial != AccountSerial || values == nullptr)
        {
            std::exit(2);
        }
        if (!g_scenario->expectedAnimusInsertValues.empty())
        {
            if (g_scenario->expectedAnimusInsertValues.size() != 6)
            {
                std::exit(2);
            }
            for (std::size_t index = 0; index < 6; ++index)
            {
                if (values[index] != g_scenario->expectedAnimusInsertValues[index])
                {
                    std::cerr << "Invalid six-value AnimusData insert buffer\n";
                    std::exit(2);
                }
            }
        }
        g_scenario->calls.emplace_back("Insert_AnimusData");
        return Next(g_scenario->insertAnimus, g_scenario->insertAnimusIndex);
    }

    const DatabaseApi Api{UpdateNpc, InsertNpc, UpdateAnimus, InsertAnimus};

    const char* OperationName(const FallbackOperation operation)
    {
        switch (operation)
        {
        case FallbackOperation::NpcUpdateInitial:
            return "NpcUpdateInitial";
        case FallbackOperation::NpcInsert:
            return "NpcInsert";
        case FallbackOperation::NpcUpdateRetry:
            return "NpcUpdateRetry";
        case FallbackOperation::AnimusUpdateInitial:
            return "AnimusUpdateInitial";
        case FallbackOperation::AnimusInsert:
            return "AnimusInsert";
        case FallbackOperation::AnimusUpdateRetry:
            return "AnimusUpdateRetry";
        }
        std::exit(2);
    }

    void __cdecl BeforeOperation(void* const context, const FallbackOperation operation) noexcept
    {
        auto* const scenario = static_cast<Scenario*>(context);
        scenario->observerEvents.emplace_back(std::string("before:") + OperationName(operation));
    }

    void __cdecl AfterOperation(
        void* const context,
        const FallbackOperation operation,
        const bool result) noexcept
    {
        auto* const scenario = static_cast<Scenario*>(context);
        scenario->observerEvents.emplace_back(
            std::string("after:") + OperationName(operation) + (result ? ":1" : ":0"));
    }

    bool Run(Scenario& scenario, const bool observe = false)
    {
        CCheckSumCharacAccountTrunkData data{};
        data.m_dwSerial = CharacterSerial;
        data.m_dwAccountSerial = AccountSerial;
        data.m_byRace = scenario.race;
        data.m_dValues[0] = 12.5;
        data.m_dValues[1] = 34.5;
        CRFWorldDatabase database{};
        g_scenario = &scenario;
        const FallbackObserver observer{&scenario, BeforeOperation, AfterOperation};
        return ApplyNpcDataFallback(
            &data, &database, Api, observe ? &observer : nullptr);
    }

    void Expect(
        const char* name,
        Scenario scenario,
        bool expectedResult,
        std::vector<std::string> expectedCalls)
    {
        const bool actualResult = Run(scenario);
        if (actualResult != expectedResult || scenario.calls != expectedCalls)
        {
            std::cerr << "FAILED: " << name << '\n';
            std::exit(1);
        }
    }

    void ExpectAnimusInsertMapping(
        const char race,
        std::vector<long double> expectedValues)
    {
        Scenario scenario{{true}, {}, {false, true}, {true}};
        scenario.race = race;
        scenario.expectedAnimusInsertValues = std::move(expectedValues);
        Expect("AnimusData six-value insert mapping", std::move(scenario), true,
            {"Update_NpcData", "Update_AnimusData", "Insert_AnimusData", "Update_AnimusData"});
    }

    void ExpectObserved(
        const char* const name,
        Scenario scenario,
        const bool expectedResult,
        const std::vector<std::string>& expectedEvents)
    {
        const bool actualResult = Run(scenario, true);
        if (actualResult != expectedResult || scenario.observerEvents != expectedEvents)
        {
            std::cerr << "FAILED observer: " << name << '\n';
            std::exit(1);
        }
    }
}

int main()
{
    Expect("both updates succeed",
        {{true}, {}, {true}, {}}, true,
        {"Update_NpcData", "Update_AnimusData"});

    Expect("NpcData insert and retry",
        {{false, true}, {true}, {true}, {}}, true,
        {"Update_NpcData", "Insert_NpcData", "Update_NpcData", "Update_AnimusData"});

    Expect("NpcData insert fails",
        {{false}, {false}, {}, {}}, false,
        {"Update_NpcData", "Insert_NpcData"});

    Expect("NpcData retry fails",
        {{false, false}, {true}, {}, {}}, false,
        {"Update_NpcData", "Insert_NpcData", "Update_NpcData"});

    ExpectAnimusInsertMapping(0, {12.5, 34.5, 0.0, 0.0, 0.0, 0.0});
    ExpectAnimusInsertMapping(1, {0.0, 0.0, 12.5, 34.5, 0.0, 0.0});
    ExpectAnimusInsertMapping(2, {0.0, 0.0, 0.0, 0.0, 12.5, 34.5});

    Expect("invalid AnimusData race does not insert",
        {{true}, {}, {false}, {}, {}, 0, 0, 0, 0, {}, 3}, false,
        {"Update_NpcData", "Update_AnimusData"});

    Expect("both fallbacks run in upstream order",
        {{false, true}, {true}, {false, true}, {true}}, true,
        {"Update_NpcData", "Insert_NpcData", "Update_NpcData",
         "Update_AnimusData", "Insert_AnimusData", "Update_AnimusData"});

    Expect("AnimusData insert fails",
        {{true}, {}, {false}, {false}}, false,
        {"Update_NpcData", "Update_AnimusData", "Insert_AnimusData"});

    Expect("AnimusData retry fails",
        {{true}, {}, {false, false}, {true}}, false,
        {"Update_NpcData", "Update_AnimusData", "Insert_AnimusData", "Update_AnimusData"});

    ExpectObserved("both fallbacks are observed in exact upstream order",
        {{false, true}, {true}, {false, true}, {true}}, true,
        {"before:NpcUpdateInitial", "after:NpcUpdateInitial:0",
         "before:NpcInsert", "after:NpcInsert:1",
         "before:NpcUpdateRetry", "after:NpcUpdateRetry:1",
         "before:AnimusUpdateInitial", "after:AnimusUpdateInitial:0",
         "before:AnimusInsert", "after:AnimusInsert:1",
         "before:AnimusUpdateRetry", "after:AnimusUpdateRetry:1"});

    ExpectObserved("failed NpcData insert stops observer before AnimusData",
        {{false}, {false}, {}, {}}, false,
        {"before:NpcUpdateInitial", "after:NpcUpdateInitial:0",
         "before:NpcInsert", "after:NpcInsert:0"});

    std::cout << "All NpcData fallback tests passed.\n";
    return 0;
}
