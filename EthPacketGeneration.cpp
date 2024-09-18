#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <array>
#include <vector>
#include <cstdint>
#include <map>

#define ETH_HEADER_SIZE 26
using namespace std;

template <typename T, uint8_t my_size>
array<uint8_t, my_size> intToArray(T number)
{

    array<uint8_t, my_size> result;
    uint8_t *byte = reinterpret_cast<uint8_t *>(&number);

    for (char i = my_size - 1; i >= 0; --i)
    {
        result[i] = *byte++;
    }
    return result;
}

class parseConfigurations
{
public:
    uint8_t LineRate;
    uint8_t CaptureSizeMs;
    uint8_t MinNumOfIFGsPerPacket;
    uint64_t DestAddress;
    uint64_t SourceAddress;
    uint16_t MaxPacketSize;
    uint8_t BurstSize;
    uint32_t BurstPeriodicity_us;

    parseConfigurations(string fileName)
    {
        // Create a map to store key-value pairs
        map<string, uint64_t> config;

        // Create a text string, which is used to output the text file
        string line;

        // Read from the text file
        ifstream MyReadFile(fileName);

        // Use a while loop together with the getline() function to read the file line by line
        while (getline(MyReadFile, line))
        {

            line.erase(remove_if(line.begin(), line.end(), [](unsigned char x)
                                 { return isspace(x); }),
                       line.end());

            // Remove comments (anything after "//")
            auto comment_pos = line.find("//");
            if (comment_pos != string::npos)
            {
                line.erase(line.begin() + line.find("//"), line.end());
            }

            // Skip empty lines
            if (line.empty())
            {
                continue;
            }

            // Split the line at the '=' sign
            auto key = line.substr(0, line.find('='));
            auto valueStr = line.substr(line.find('=') + 1);

            // Convert the value from string to appropriate data type
            uint64_t value;
            if (valueStr.find("0x") == 0) // Check if the value is hexadecimal
            {
                value = stoull(valueStr, nullptr, 16);
            }
            else
            {
                value = stoull(valueStr); // Decimal value
            }

            // Insert the key-value pair into the map
            config[key] = value;
        }

        // Set the configuration fields
        LineRate = static_cast<uint8_t>(config["Eth.LineRate"]);
        CaptureSizeMs = static_cast<uint8_t>(config["Eth.CaptureSizeMs"]);
        MinNumOfIFGsPerPacket = static_cast<uint8_t>(config["Eth.MinNumOfIFGsPerPacket"]);
        DestAddress = config["Eth.DestAddress"];
        SourceAddress = config["Eth.SourceAddress"];
        MaxPacketSize = static_cast<uint16_t>(config["Eth.MaxPacketSize"]);
        BurstSize = static_cast<uint8_t>(config["Eth.BurstSize"]);
        BurstPeriodicity_us = static_cast<uint16_t>(config["Eth.BurstPeriodicity_us"]);

        // Close the file
        MyReadFile.close();
    }
};

class EthFrame
{
private:
    const array<uint8_t, 8> preamble{0xfb, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0xd5};
    array<uint8_t, 6> destAddress;
    array<uint8_t, 6> sourceAddress;
    array<uint8_t, 2> etherSize;
    vector<uint8_t> payload;
    array<uint8_t, 4> fcs;

    vector<uint8_t> frame;

public:
    // EthFrame Constructor
    EthFrame(const array<uint8_t, 6> &dest, const array<uint8_t, 6> &src, const array<uint8_t, 2> &size, const vector<uint8_t> &data)
        : destAddress(dest), sourceAddress(src), etherSize(size), payload(move(data))
    {
    }
    // Method to calculate FCS
    array<uint8_t, 4> crc32(const vector<uint8_t> &data)
    {
        uint32_t crc = 0xFFFFFFFF;
        for (auto byte : data)
        {
            crc ^= static_cast<uint32_t>(byte);
            for (int i = 0; i < 8; ++i)
            {
                if (crc & 1)
                {
                    crc = (crc >> 1) ^ 0xEDB88320;
                }
                else
                {
                    crc >>= 1;
                }
            }
        }
        crc = crc ^ 0xFFFFFFFF;
        array<uint8_t, 4> crcArray;
        crcArray[0] = static_cast<uint8_t>(crc >> 24);
        crcArray[1] = static_cast<uint8_t>(crc >> 16);
        crcArray[2] = static_cast<uint8_t>(crc >> 8);
        crcArray[3] = static_cast<uint8_t>(crc);

        return crcArray;
    }

    // Method to construct the Ethernet frame
    vector<uint8_t> constructFrame(uint32_t MinNumOfIFGsPerPacket)
    {
        // Add Destination MAC
        frame.insert(frame.end(), destAddress.begin(), destAddress.end());

        // Add Source MAC
        frame.insert(frame.end(), sourceAddress.begin(), sourceAddress.end());

        // Add etherSize
        frame.insert(frame.end(), etherSize.begin(), etherSize.end());

        // Add Payload
        frame.insert(frame.end(), payload.begin(), payload.end());

        // Add Frame Check Sequence
        fcs = crc32(frame);
        frame.insert(frame.end(), fcs.begin(), fcs.end());

        // Add Preamble (7 bytes) + Start Frame Delimiter (SFD) (1 byte)
        frame.insert(frame.begin(), preamble.begin(), preamble.end());


        // Insert minimum number of IFGS per packet
        for (uint32_t i = 0; i < MinNumOfIFGsPerPacket; i++)
        {
            frame.push_back(0x07);
        }

        // 4-Byte alignment
        while (frame.size() % 4)
        {
            frame.push_back(0x07);
        }
        return frame;
    }
};

class packetStreaming
{
private:
    vector<uint8_t> fullPacket;
    vector<uint8_t> periodicIFG;
    array<uint8_t, 6> destAddress;
    array<uint8_t, 6> sourceAddress;
    array<uint8_t, 2> etherSize;
    vector<uint8_t> payload;
    uint64_t lineRate;
    uint64_t captureSize;
    uint8_t burstSize;
    uint64_t burstPeriodicity;
    uint8_t minNumOfIFGsPerPacket;

public:
    packetStreaming(const parseConfigurations &configuration, vector<uint8_t> data)
    {
        lineRate = static_cast<uint64_t>(configuration.LineRate);
        captureSize = static_cast<uint64_t>(configuration.CaptureSizeMs);
        minNumOfIFGsPerPacket = configuration.MinNumOfIFGsPerPacket;
        destAddress = intToArray<uint64_t, 6>(configuration.DestAddress);
        sourceAddress = intToArray<uint64_t, 6>(configuration.SourceAddress);
        etherSize = intToArray<uint64_t, 2>(configuration.MaxPacketSize - ETH_HEADER_SIZE);
        burstSize = configuration.BurstSize;
        burstPeriodicity = static_cast<uint64_t>(configuration.BurstPeriodicity_us);
        data.resize(configuration.MaxPacketSize - ETH_HEADER_SIZE);
        payload = data;
    }

    vector<uint8_t> constructStream()
    {
        uint64_t totalBursts{(captureSize * 1000) / burstPeriodicity};
        uint64_t IFGperBurst{(lineRate * burstPeriodicity * 1000) / 8};
        EthFrame burst{destAddress, sourceAddress, etherSize, payload};
        auto tempFrame = burst.constructFrame(minNumOfIFGsPerPacket);
        periodicIFG.assign(IFGperBurst, 0x07);

        for (uint64_t i = 0; i < totalBursts; i++)
        {
            for (uint64_t i = 0; i < burstSize; i++)
            {
                fullPacket.insert(fullPacket.end(), tempFrame.begin(), tempFrame.end());
            }
            fullPacket.insert(fullPacket.end(), periodicIFG.begin(), periodicIFG.end());
        }

        return fullPacket;
    }
};



int main()
{
    uint8_t LineRate{10};
    uint8_t CaptureSizeMs{10};
    uint8_t MinNumOfIFGsPerPacket{12};
    uint64_t DestAddress{0x010101010101};
    uint64_t SourceAddress{0x333333333333};
    uint16_t MaxPacketSize{1500};
    uint8_t BurstSize{3};
    uint32_t BurstPeriodicity_us{100};
    vector<uint8_t> data = {0x0};
    vector<uint8_t> fullPacketStream;

    parseConfigurations configuration("first_milestone.txt");

    packetStreaming fullStream{configuration, data};

    fullPacketStream = fullStream.constructStream();

    // Create and open a text file
    ofstream MyFile("packets.txt");
    vector<uint32_t> &words = reinterpret_cast<vector<uint32_t> &>(fullPacketStream);
    // Write to the file
    for (auto word : words)
    {
        MyFile << hex << setw(8) << setfill('0') << static_cast<int>(word) << endl;
    }
    // Close the file
    MyFile.close();





    // for (auto byte : fullFrame)
    // {
    //     cout << hex << setw(2) << setfill('0') << static_cast<int>(byte) << endl;
    // }

    // for (auto word : words)
    // {
    //     cout << hex << setw(8) << setfill('0') << static_cast<int>(word) << endl;
    // }
    // cout << "\n";
    // cout << endl;

    return 0;
}