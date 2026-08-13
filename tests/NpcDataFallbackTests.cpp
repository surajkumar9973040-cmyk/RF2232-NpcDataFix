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
    };

    Scenario* g_scenario = nullptr;
    constexpr std::uint32_t CharacterSerial = 1234;
    constexpr std::uint32_t AccountSerial = 5678;
    constexpr char Race = 2;

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
        if (serial != AccountSerial || race != Race || values == nullptr)
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
        g_scenario->calls.emplace_back("Insert_AnimusData");
        return Next(g_scenario->insertAnimus, g_scenario->insertAnimusIndex);
    }

    const DatabaseApi Api{UpdateNpc, InsertNpc, UpdateAnimus, InsertAnimus};

    bool Run(Scenario& scenario)
    {
        CCheckSumCharacAccountTrunkData data{};
        data.m_dwSerial = CharacterSerial;
        data.m_dwAccountSerial = AccountSerial;
        data.m_byRace = Race;
        CRFWorldDatabase database{};
        g_scenario = &scenario;
        return ApplyNpcDataFallback(&data, &database, Api);
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

    Expect("AnimusData insert and retry",
        {{true}, {}, {false, true}, {true}}, true,
        {"Update_NpcData", "Update_AnimusData", "Insert_AnimusData", "Update_AnimusData"});

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

    std::cout << "All NpcData fallback tests passed.\n";
    return 0;
}
