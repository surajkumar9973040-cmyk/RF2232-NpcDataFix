#include <NpcDataFix/NpcDataFallback.hpp>

namespace NpcDataFix
{
    bool ApplyNpcDataFallback(
        CCheckSumCharacAccountTrunkData* const data,
        CRFWorldDatabase* const database,
        const DatabaseApi& api)
    {
        if (data == nullptr || database == nullptr ||
            api.updateNpcData == nullptr || api.insertNpcData == nullptr ||
            api.updateAnimusData == nullptr || api.insertAnimusData == nullptr)
        {
            return false;
        }

        bool result = false;

        do
        {
            result = api.updateNpcData(database, data->m_dwSerial, data->m_dwValues);
            if (!result)
            {
                result = api.insertNpcData(database, data->m_dwSerial, data->m_dwValues);
                if (!result)
                {
                    break;
                }

                result = api.updateNpcData(database, data->m_dwSerial, data->m_dwValues);
                if (!result)
                {
                    break;
                }
            }

            result = api.updateAnimusData(
                database, data->m_dwAccountSerial, data->m_byRace, data->m_dValues);
            if (!result)
            {
                result = api.insertAnimusData(database, data->m_dwAccountSerial, data->m_dValues);
                if (!result)
                {
                    break;
                }

                result = api.updateAnimusData(
                    database, data->m_dwAccountSerial, data->m_byRace, data->m_dValues);
                if (!result)
                {
                    break;
                }
            }

            result = true;
        } while (false);

        return result;
    }
}
