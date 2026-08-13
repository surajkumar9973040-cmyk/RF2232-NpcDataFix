#include <NpcDataFix/NpcDataFallback.hpp>

namespace NpcDataFix
{
    namespace
    {
        void ObserveBefore(
            const FallbackObserver* const observer,
            const FallbackOperation operation) noexcept
        {
            if (observer != nullptr && observer->before != nullptr)
            {
                observer->before(observer->context, operation);
            }
        }

        void ObserveAfter(
            const FallbackObserver* const observer,
            const FallbackOperation operation,
            const bool result) noexcept
        {
            if (observer != nullptr && observer->after != nullptr)
            {
                observer->after(observer->context, operation, result);
            }
        }
    }

    bool ApplyNpcDataFallback(
        CCheckSumCharacAccountTrunkData* const data,
        CRFWorldDatabase* const database,
        const DatabaseApi& api,
        const FallbackObserver* const observer)
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
            ObserveBefore(observer, FallbackOperation::NpcUpdateInitial);
            result = api.updateNpcData(database, data->m_dwSerial, data->m_dwValues);
            ObserveAfter(observer, FallbackOperation::NpcUpdateInitial, result);
            if (!result)
            {
                ObserveBefore(observer, FallbackOperation::NpcInsert);
                result = api.insertNpcData(database, data->m_dwSerial, data->m_dwValues);
                ObserveAfter(observer, FallbackOperation::NpcInsert, result);
                if (!result)
                {
                    break;
                }

                ObserveBefore(observer, FallbackOperation::NpcUpdateRetry);
                result = api.updateNpcData(database, data->m_dwSerial, data->m_dwValues);
                ObserveAfter(observer, FallbackOperation::NpcUpdateRetry, result);
                if (!result)
                {
                    break;
                }
            }

            ObserveBefore(observer, FallbackOperation::AnimusUpdateInitial);
            result = api.updateAnimusData(
                database, data->m_dwAccountSerial, data->m_byRace, data->m_dValues);
            ObserveAfter(observer, FallbackOperation::AnimusUpdateInitial, result);
            if (!result)
            {
                // Insert_AnimusData consumes all six race values even though the
                // checksum object contains only the current race's pair. The
                // upstream callback passed the two-element member directly and
                // therefore read 32 bytes beyond the object. Build the complete
                // row explicitly, then let the retry update the selected pair.
                long double insertValues[6]{};
                const auto race = static_cast<unsigned char>(data->m_byRace);
                if (race >= 3)
                {
                    break;
                }
                const auto valueIndex = static_cast<std::size_t>(race) * 2;
                insertValues[valueIndex] = data->m_dValues[0];
                insertValues[valueIndex + 1] = data->m_dValues[1];

                ObserveBefore(observer, FallbackOperation::AnimusInsert);
                result = api.insertAnimusData(
                    database, data->m_dwAccountSerial, insertValues);
                ObserveAfter(observer, FallbackOperation::AnimusInsert, result);
                if (!result)
                {
                    break;
                }

                ObserveBefore(observer, FallbackOperation::AnimusUpdateRetry);
                result = api.updateAnimusData(
                    database, data->m_dwAccountSerial, data->m_byRace, data->m_dValues);
                ObserveAfter(observer, FallbackOperation::AnimusUpdateRetry, result);
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
