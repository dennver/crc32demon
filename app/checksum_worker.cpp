#include <jsoncpp/json/json.h>
#include <fstream>

#include "checksum_worker.h"
#include "crc32.h"
 
namespace Worker
{
    std::string toHex(uint32_t i)
    {
        std::stringstream stream;
        stream << "0x" << std::hex << i;
        return stream.str();
    }

    ChecksumWorker::ChecksumWorker(unsigned int threads, unsigned int chSize) : numOfThreads(threads), chunkSize(chSize), logger(GetLogger()) {}

    void ChecksumWorker::HandleEvent(Queue::Event e)
    {
        uint32_t crc;
        const bool fileDeleted = e.type == Queue::EventType::DELETED;
        if (!fileDeleted)
            crc = calculateCRC32Parallel(e.fileName, chunkSize, numOfThreads);
        if (auto search = fileMap.find(e.fileName); search != fileMap.end() && !e.firstEvent)
        {
            FileIntegrity& fi = fileMap[e.fileName];
            fi.result_crc32 = crc;            
            if (fileDeleted)
                fi.status = "ABSENT";
            else if (fi.etalon_crc32 != fi.result_crc32)
                fi.status = "FAIL";
            else
                fi.status = "OK";

            std::string message;
            if (fi.result_crc32 == fi.etalon_crc32)
                message = "OK";
            else
                message = std::string("FAIL ") + toHex(fi.etalon_crc32) + " - " + toHex(fi.result_crc32);

            logger->writeLog("Integrity check: " + fi.status);
        }
        else
        {
            FileIntegrity fi;
            fi.etalon_crc32 = crc;
            fi.status = "NEW";
            fileMap[e.fileName] = fi; 
        }
    }

    void ChecksumWorker::DumpEvents()
    {
        Json::Value root(Json::arrayValue);
        for (auto const& f : fileMap)
        {
            // Append a new JSON object to the array
            Json::Value obj;
            obj["path"] = f.first;
            obj["etalon_crc32"] = toHex(f.second.etalon_crc32);
            obj["result_crc32"] = toHex(f.second.result_crc32);
            obj["status"] = f.second.status;
            root.append(obj);
        }
        std::ofstream outputFile("result.json", std::ios_base::trunc);
        if (!outputFile.is_open())
        {
            logger->writeLog("cannot open result file");
            return;
        }

        outputFile << root;
        outputFile.close();
    }
}